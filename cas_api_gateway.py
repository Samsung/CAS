import argparse
from asyncio import gather
import asyncio
from contextlib import asynccontextmanager
from copy import copy
import datetime
from functools import lru_cache
import json
from os import path
import os
from typing import Dict, Literal, cast
from fastapi import APIRouter, BackgroundTasks, FastAPI, Request, Response
from fastapi_lifespan_manager import LifespanManager
from fastmcp import FastMCP
from starlette.background import BackgroundTask
from fastapi.responses import StreamingResponse
from fastapi.staticfiles import StaticFiles
from fastapi.routing import APIRoute, Mount
from fastapi.templating import Jinja2Templates
from httpx import URL, AsyncClient, RequestError, Timeout
import uvicorn

from cas_server import cas_router

script_dir = path.dirname(path.realpath(__file__))
config_file = path.join(script_dir,'gw_config.json')
host = "0.0.0.0"
port = 8080
conf = {}
engine_version = 4
templates = Jinja2Templates(directory="client/templates")
static = StaticFiles(directory="client/static")

BACKEND_SERVERS_STATUS: Dict[str, Dict[str, Dict[str, str]]] = {}
if path.exists(config_file):
    with open(config_file, 'r', encoding="utf-8") as conffile:
        conf = json.load(conffile)

client: AsyncClient
lifespan_manager = LifespanManager()

@lifespan_manager.add
async def routes_methods(app: FastAPI):
    # until https://github.com/fastapi/fastapi/issues/1773 is fixed
    for route in app.routes:
        if isinstance(route, APIRoute) and "GET" in route.methods:
            new_route = copy(route)
            new_route.methods = {"HEAD", "OPTIONS"}
            new_route.include_in_schema = False
            app.routes.append(new_route)
@lifespan_manager.add
async def httpx_client(app: FastAPI):
    global client
    async with AsyncClient(follow_redirects=True) as c:
        client = c
        yield

cas_gw = FastAPI(lifespan=lifespan_manager)

cas_gw.mount("/static", static, name="static")

    
def get_host_from_endpoint(endpoint):
    matched = [ host for host,endp in conf["BACKEND_SERVERS"].items() if endp == endpoint]
    if len(matched) == 1:
        return matched[0]
    else:
        return None


async def status_request(host: str, endpoint: URL):
    try:
        response = await client.get(url=endpoint.join("status"), timeout=5)
        return host, response.json()
    except (RequestError, json.JSONDecodeError) as e:
        return host, {"ERROR": str(e)}

async def get_status():
    ret = {}
    rev = {}
    mcp = {}
    
    rsp = await gather(*[status_request(host, URL(endpoint)) for host, endpoint in conf["BACKEND_SERVERS"].items()])
    
    for host, dbs in rsp:
        url = conf["BACKEND_SERVERS"][host]
        if dbs is not None:
            ret[url] = {}
            if dbs.get("ERROR", None) is not None:
                ret[url] = {"ERROR": dbs["ERROR"], host: host}
                continue
            for name, data in dbs.items():
                cast(Dict, data)["host"] = host
                ret[url][name] = data
                rev[name] = { "url": url, "host": host, **cast(Dict, data)}
                mcp[name] = { "url": cast(str, url).removesuffix("/") + "/mcp/http", "transport": "streamable-http" }
        else:
            ret[url] = None
    rev["timestamp"] = datetime.datetime.now()
    return ret, rev, { "mcpServers": mcp }

@cas_gw.get('/status')
async def status():
    return (await get_status())[0]

@cas_gw.get("/")
async def main_status(request: Request):
    backends = (await get_status())[0]
    not_responding = [ f"WARNING: {get_host_from_endpoint(x)} is not responding! " for x in backends if backends[x] is None ]
    return templates.TemplateResponse(name="gw_dbs.html", request=request, context={"backend": backends, "servers": conf["BACKEND_SERVERS"],
                                            "not_responding": not_responding, "engine": engine_version, "web_ctx": ctx + "/"})


@lifespan_manager.add
async def setup_mcp(app: FastAPI):
    if app.state.mcp is not None:
        task = asyncio.create_task(loop_mcp())
        yield
        task.cancel()

async def loop_mcp():
    current = update_mcp({"mcpServers": {}})
    try:
        while True:
            await asyncio.sleep(60) # only re-check MCP every minute
            current = update_mcp(current)
    except asyncio.CancelledError:
        pass
async def update_mcp(current: Dict[Literal["mcpServers"], Dict[str, Dict[str, str]]]):    
    new = (await get_status())[2]
    current_dbs = set([f"{k}_{v['url']}" for k,v in current["mcpServers"].items()])
    new_dbs = set([f"{k}_{v['url']}" for k,v in new["mcpServers"].items()])

    if current_dbs != new_dbs:
        new_proxy = FastMCP.as_proxy(new)
        
        http_mount = Mount("/mcp/http", new_proxy.http_app(transport="streamable-http"))
        sse_mount = Mount("/mcp/sse",  new_proxy.http_app(transport="sse"))
        removed_indexes = []
        for i, route in enumerate(cas_gw.routes):
            if route.matches("/mcp/http") or route.matches("/mcp/sse"):
                removed_indexes.append(i)
        for i in removed_indexes:
            cas_gw.routes.pop(i)
        cas_gw.routes.insert(1, http_mount)
        cas_gw.routes.insert(2, sse_mount)

def setup_proxy():
    proxy_app = FastAPI()
    @proxy_app.middleware("http")
    async def proxy_middleware(request: Request, call_next):
        global BACKEND_SERVERS_STATUS
        db = request.path_params.get("db", None)
        path = str(request.url).replace(str(request.base_url), "")
        if datetime.datetime.now() - cast(datetime.datetime, 
                                            BACKEND_SERVERS_STATUS.get("timestamp", 
                                                                        datetime.datetime(1,1,1,1,1,1)
                                                                    )
                                        ) > datetime.timedelta(seconds=5): 
            _, BACKEND_SERVERS_STATUS = await get_status()
        
        
        if db is not None and BACKEND_SERVERS_STATUS.get(db, None) is not None:
            proxied_url = URL(cast(str, BACKEND_SERVERS_STATUS[db]["url"]))
            headers = request.headers.mutablecopy()
            headers["X-Forwarded-For"] = request.headers.get("X-Forwarded-For", request.client.host if request.client else "")
            headers["X-Forwarded-Host"] = request.headers.get("X-Forwarded-Host", request.url.hostname or host)
            headers["X-Forwarded-Scheme"] = request.headers.get("X-Forwarded-Scheme", request.headers.get("X-Forwarded-Proto", "http"))
            headers["X-Forwarded-Proto"] = headers["X-Forwarded-Scheme"]
            req = client.build_request(
                url=proxied_url.join(path),
                method=request.method,
                content=request.stream(),
                cookies=request.cookies,
                headers=headers,
                timeout=Timeout(None),
            )
            resp = await client.send(req, stream=True, follow_redirects=True)
            return StreamingResponse(
                content=resp.aiter_raw(),
                status_code=resp.status_code,
                headers=resp.headers,
                background=BackgroundTask(resp.aclose),
            )
            
        return await call_next(request)
    proxy_app.include_router(cas_router, include_in_schema=True)
    cas_gw.mount("/{db}", proxy_app)
    cas_gw.include_router(cas_router, prefix="/{db}", )

a = argparse.Namespace()
ctx: str = ""
def get_app():
    global a
    global ctx
    arg_parser = argparse.ArgumentParser(description="CAS Gateway arguments")
    arg_parser.add_argument("--port", "-p", type=int, default=8080, help="server port")
    arg_parser.add_argument("--host", type=str, default="0.0.0.0", help="server address")
    arg_parser.add_argument("--ctx", type=str, default='/', help="Web context")
    arg_parser.add_argument("--debug", action="store_true", help="debug mode")
    arg_parser.add_argument("--workers", type=int, default=os.environ.get("WEB_CONCURRENCY", os.cpu_count() or 1), help="Number of worker processes to run")
    arg_parser.add_argument("--mcp", action="store_true", help="Enable CAS MCP server under `/mcp` and `/sse` endpoints")

    a = arg_parser.parse_args()

    ctx = cast(str, conf["WEB_CONTEXT"] if "WEB_CONTEXT" in conf else a.ctx).removesuffix("/")
    cas_gw.root_path = ctx
    setup_proxy()

    if a.mcp or bool(os.environ.get("CAS_MCP", False)):
        cas_gw.state.mcp = True
    if a.debug:
        print(cas_gw.routes)
    return cas_gw
if __name__ == '__main__':
    try:
        cas_gw = get_app()
        port = conf["PORT"] if "PORT" in conf else a.port
        host = conf["HOST"] if "HOST" in conf else a.host
        uvicorn.run("cas_api_gateway:get_app", factory=True, host=host, port=port, log_level="debug" if a.debug else None, workers=a.workers)
        print("Started CAS API Gateway")
            
    except KeyboardInterrupt:
        print("Shutting down CAS API Gateway")