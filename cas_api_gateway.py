import argparse
from asyncio import gather
from contextlib import asynccontextmanager
from copy import copy
import datetime
from functools import lru_cache
import json
from os import path
import os
from typing import Dict, cast
from fastapi import APIRouter, FastAPI, Request, Response
from starlette.background import BackgroundTask
from fastapi.responses import StreamingResponse
from fastapi.staticfiles import StaticFiles
from fastapi.routing import APIRoute
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

@asynccontextmanager
async def lifespan(app: FastAPI):
    global client
    # until https://github.com/fastapi/fastapi/issues/1773 is fixed
    for route in app.routes:
        if isinstance(route, APIRoute) and "GET" in route.methods:
            new_route = copy(route)
            new_route.methods = {"HEAD", "OPTIONS"}
            new_route.include_in_schema = False
            app.routes.append(new_route)
    async with AsyncClient(follow_redirects=True) as c:
        client = c
        yield

cas_gw = FastAPI(lifespan=lifespan)

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
        else:
            ret[url] = None
    rev["timestamp"] = datetime.datetime.now()
    return ret, rev

@cas_gw.get('/status')
async def status():
    return (await get_status())[0]

@cas_gw.get("/")
async def main_status(request: Request):
    backends = (await get_status())[0]
    not_responding = [ f"WARNING: {get_host_from_endpoint(x)} is not responding! " for x in backends if backends[x] is None ]
    return templates.TemplateResponse(name="gw_dbs.html", request=request, context={"backend": backends, "servers": conf["BACKEND_SERVERS"],
                                            "not_responding": not_responding, "engine": engine_version, "web_ctx": ctx + "/"})


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

    a = arg_parser.parse_args()

    ctx = cast(str, conf["WEB_CONTEXT"] if "WEB_CONTEXT" in conf else a.ctx).removesuffix("/")
    cas_gw.root_path = ctx
    setup_proxy()
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