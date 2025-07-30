#!/usr/bin/env python3
from contextlib import asynccontextmanager
from copy import copy
from enum import Enum
import os
from enum import Enum
import os
import sys

from fastapi.routing import APIRoute
from pydantic import BaseModel, Field, RootModel
from pydantic_settings import BaseSettings, SettingsConfigDict

from pydantic import BaseModel
from pydantic_settings import BaseSettings, SettingsConfigDict
from fastapi_lifespan_manager import LifespanManager, State

try:
    import libetrace as _
except ModuleNotFoundError:
    print("CAS libetrace not found - provide libetrace.so in directory or export directory with library to PYTHONPATH")
    print("eg. export PYTHONPATH=/home/j.doe/CAS/:$PYTHONPATH")
    sys.exit(2)

from argparse import Namespace
from functools import lru_cache
from typing import Annotated, Any, Dict, List, Optional, Literal, AsyncIterator
from typing_extensions import TypedDict

from os import path
import math
import argparse
from shlex import split as shell_split
from urllib.parse import parse_qsl

from libetrace import nfsdbEntry
import libetrace
from libcas import CASDatabase
from client.server import DBProvider
from client.filtering import CommandFilter
from client.argparser import get_api_keywords
from client.misc import access_from_code, get_file_info
from client.exceptions import MessageException, LibFtdbException

from fastapi import FastAPI, APIRouter, HTTPException, Query, Request, Response
from fastapi.responses import FileResponse, HTMLResponse, JSONResponse, ORJSONResponse, PlainTextResponse
from fastapi.staticfiles import StaticFiles
from fastapi.templating import Jinja2Templates
from fastapi.middleware.cors import CORSMiddleware

import uvicorn

args: Namespace = Namespace()
dbs: DBProvider = DBProvider()

lifespan_manager = LifespanManager()
app = FastAPI(lifespan=lifespan_manager)

static = StaticFiles(directory="client/static")
app.mount("/static", static, name="static")

@lifespan_manager.add
async def lifespan(app: FastAPI) -> AsyncIterator[State]:
    # until https://github.com/fastapi/fastapi/issues/1773 is fixed
    for route in app.routes:
        if isinstance(route, APIRoute) and "GET" in route.methods:
            new_route = copy(route)
            new_route.methods = {"HEAD", "OPTIONS"}
            new_route.include_in_schema = False
            app.routes.append(new_route)
    yield {}

@lifespan_manager.add
async def lifespan(server) -> AsyncIterator[State]:
    yield {"dbs": dbs}

cas_router = APIRouter(lifespan=lifespan)

templates = Jinja2Templates(directory="client/templates")


allowed_modules = [keyw for keyw in get_api_keywords() if keyw not in ['parse', 'postprocess', 'pp', 'cache']]
bool_args = [ "commands", "show-commands", "details", "t", "cdm", "compilation-dependency-map", "revdeps", "V", "reverse-dependency-map", "rdm", "recursive", "r", "with-children", "direct", "raw-command",
             "extended", "e", "dep-graph", "G", "filerefs", "F", "direct-global", "Dg", "cached", "wrap-deps", "w", "with-pipes", "P", "original-path", "parent", "match", "m"
             "sorted", "sort", "s", "reverse", "relative", "R", "generate", "makefile", "all", "openrefs", "static", "cdb", "download", "proc-tree", "deps-tree",
             "body", "ubody", "declbody", "definition", "skip-linked", "skip-objects", "skip-asm", "deep-comps" ]



def translate_to_cmdline(url: str, query_str:str, ctx: str, db: str) -> List[str]:
    """
    Function translates url to client argument list.

    :param url: url string
    :type url: str
    :param query_str: url query part
    :type query_str: str
    :param ctx: web context
    :type ctx: str
    :param db: databse name
    :type db: str
    :return: list of arguments
    :rtype: List[str]
    """

    url_path = url.lstrip("/")
    if url_path.startswith(ctx.lstrip("/")):
        url_path = url_path.replace(ctx.lstrip("/"),"").lstrip("/")
    if url_path.startswith(db):
        url_path = url_path.replace(db,"").lstrip("/")

    ret = [url_path]
    q = parse_qsl(query_str, keep_blank_values=True)
    for k_v in q:
        if len(k_v) == 2:
            if k_v[0] in bool_args:
                if "true" in k_v[1] or k_v[1] == '':
                    ret.append(f"--{k_v[0]}" if len(k_v[0]) > 1 else f"-{k_v[0]}")
            else:
                if k_v[1] == '':
                    ret.append(k_v[0])
                else:
                    ret.append(f"--{k_v[0]}={k_v[1]}" if len(k_v[0]) > 1 else f"-{k_v[0]}={k_v[1]}")
    return ret


def translate_to_url(commandline: List[str]) -> str:
    """
    Function translates commandline list to url string

    :param commandline: list of command items
    :type commandline: List[str]
    :return: generated url
    :rtype: str
    """
    print(commandline)
    ret = commandline[0] + "?"

    def add(r, cmd_part):
        if r[-1] == "?":
            return cmd_part
        else:
            return "&" + cmd_part

    for c in commandline[1:]:
        if c in allowed_modules:
            ret += add(ret, c)
        else:
            if c.startswith("--"):
                c = c.replace("--", "", 1)
            if c.startswith("-"):
                c = c.replace("-", "", 1)
            if c in bool_args:
                ret += add(ret, c + "=true")
            else:
                cv = c.split("=", 1)
                if len(cv) == 2:
                    ret += add(ret, cv[0] + "=" + cv[1])
    return ret


def process_request(db: str, commandline: List[str], org_url: str, request: Request | None = None):
    if not any([x for x in commandline if x.startswith("-n=") or x.startswith("--entries-per-page=")]):
        commandline.append("-n=10")
        org_url += "&n=10"
    if not any([x for x in commandline if x.startswith("-p=") or x.startswith("--page=")]):
        commandline.append("-p=0")
        org_url += "&p=0"

    try:
        e = dbs.process_command(db, commandline)
    except (argparse.ArgumentError, LibFtdbException) as er:
        raise HTTPException(status_code=500, detail={"ERROR": er.message})

    if isinstance(e, Response):
        return e

    if "--proc-tree" in commandline and request:
        if isinstance(e, dict) and "entries" in e:
            e["origin_url"] = org_url
            return templates.TemplateResponse(name='proc_tree.html', request=request, context={"exe": e, "web_ctx": get_webctx(args.ctx, db), "disable_search": True})
        else:
            return JSONResponse({"ERROR": "Returned data is not executable - add '&commands=true'"})
    if "--deps-tree" in commandline and request:
        if isinstance(e, dict) and "entries" in e:
            e["origin_url"] = org_url
            return templates.TemplateResponse(name='deps_tree.html', request=request, context={"exe": e, "web_ctx": get_webctx(args.ctx, db), "disable_search": True})
        else:
            return JSONResponse({"ERROR": "Returned data is not executable - add '&commands=true'"})

    if "--cdb" in commandline:
        return Response(e, media_type="application/json", headers={"Content-disposition": "attachment; filename=compile_database.json"})
    elif "--makefile" in commandline:
        return PlainTextResponse(e, headers={"Content-disposition": f"attachment; filename={'build.mk' if '--all' in commandline else 'static.mk'}"})
    else:
        return Response(e, media_type="application/json")


def process_info_renderer(exe: nfsdbEntry, page=0, maxEntry=50):
    ret = {
        "class": "compiler" if exe.compilation_info is not None
        else "linker" if exe.linked_file is not None
        else "command",
        "pid": exe.eid.pid,
        "idx": exe.eid.index,
        "ppid": exe.parent_eid.pid if hasattr(exe, "parent_eid") else -1,
        "pidx": exe.parent_eid.index if hasattr(exe, "parent_eid") else -1,
        "bin": exe.bpath,
        "cmd": exe.argv,
        "cwd": exe.cwd,
        "pipe_eids": [f'[{o.pid},{o.index}]' for o in exe.pipe_eids],
        "stime": exe.stime if hasattr(exe, "stime") else "",
        "etime": exe.etime,
        "children": [[c.eid.pid, c.eid.index] for c in exe.childs],
        "wpid": exe.wpid if exe.wpid else "",
        "openfiles": [[access_from_code(get_file_info(o.mode)[2]), o.path] for o in exe.opens[page * maxEntry:(page + 1) * maxEntry]],
        "files_pages": math.ceil(len(exe.opens) / maxEntry),
        "open_len": len(exe.opens),
    }

    if ret['class'] == "compiler":
        ret.update({
            "src_type": {1: "c", 2: "c++", 3: "other"}[exe.compilation_info.type],
            "compiled": [{o.path: {1: "c", 2: "c++", 3: "other"}[exe.compilation_info.type]} for o in exe.compilation_info.files],
            "objects": [o.path for o in exe.compilation_info.objects],
            "headers": exe.compilation_info.header_paths,
            "ipaths": exe.compilation_info.ipaths,
            "defs": exe.compilation_info.defs,
            "undefs": exe.compilation_info.undefs
        })
    if ret['class'] == "linker":
        ret.update({
            "linked": exe.linked_file.path
        })
    return ret

ChildData = TypedDict("ChildData", {
    "class": Literal["compiler", "linker", "command"],
    "pid": int,
    "idx": int,
    "ppid": int,
    "pidx": int,
    "bin": str,
    "cmd": List[str],
    "cwd": str,
    "pipe_eids": List[str],
    "stime": int  | Literal[""],
    "etime": int,
    "children": int | List["ChildData"],
    "wpid": int | Literal[""],
    "open_len": int
})
    

def child_renderer(exe: nfsdbEntry, cas_db: CASDatabase | None = None, depth: int = 0) -> ChildData:
    ret: ChildData = {
        "class": "compiler" if exe.compilation_info is not None
        else "linker" if exe.linked_file is not None
        else "command",
        "pid": exe.eid.pid,
        "idx": exe.eid.index,
        "ppid": exe.parent_eid.pid if hasattr(exe, "parent_eid") else -1,
        "pidx": exe.parent_eid.index if hasattr(exe, "parent_eid") else -1,
        "bin": exe.bpath,
        "cmd": exe.argv,
        "cwd": exe.cwd,
        "pipe_eids": [f'[{o.pid},{o.index}]' for o in exe.pipe_eids],
        "stime": exe.stime if hasattr(exe, "stime") else "",
        "etime": exe.etime,
        "children": [child_renderer(child, cas_db, depth - 1) for child in cas_db.get_eids([(c.pid,) for c in exe.child_cids])] if cas_db is not None and depth > 0 and len(exe.child_cids) > 0 else len(exe.child_cids),
        "wpid": exe.wpid if exe.wpid else "",
        "open_len": len(exe.opens),
        "cpus": [cputime.json() for cputime in exe.cpus or []],
    }
    return ret


@lru_cache(1)
def get_binaries(db, bin_path):
    cas_db = dbs.get_nfsdb(db)
    return sorted([x for x in cas_db.db.binary_paths() if bin_path in x])


@lru_cache(1)
def get_opened_files(db, file_path):
    cas_db = dbs.get_nfsdb(db)
    return sorted([x for x in cas_db.db.opens_paths() if file_path in x])


@lru_cache()
def get_ebins(db, binpath, sort_by_time):
    cas_db = dbs.get_nfsdb(db)
    return sorted(cas_db.get_execs_using_binary(binpath),
                  key=lambda x: (x.etime if sort_by_time else x.eid.pid), reverse=sort_by_time)


@lru_cache()
def get_opens(db, path, opn_filter=False):
    cas_db = dbs.get_nfsdb(db)
    if opn_filter:
        return sorted([x for x in cas_db.db.opens_paths() if path in x])
    return sorted([x for x in cas_db.db.opens_paths() if path == x])


@lru_cache()
def get_rdeps(db, path):
    cas_db = dbs.get_nfsdb(db)
    return sorted([f
                   for fp in cas_db.db.rdeps(path, recursive=False)
                   for f in cas_db.linked_modules() if f.path == fp and f.path != path
                   ])


@lru_cache()
def get_webctx(ctx: str, db: str) -> str:
    ret = path.join(ctx, db).removeprefix("/").removesuffix("/")
    return f"/{ret}/".replace("//", "/")

def relative_url_for(request: Request, name: str, **path_params: Any) -> str:
    url = request.url_for(name, **path_params)
    return url.path
templates.env.globals["relative_url_for"] = relative_url_for


@cas_router.get('/reload_ftdb')
def reload_ftdb(path: Annotated[str, Query(description="Path of the database file to load")], db: str = ""):
    """Reloads the ftdb database from a new image file"""
    dbs.switch_ftdb(db, path)
    return "DB reloaded"


class RawCmd(BaseModel):
    cmd: str

@cas_router.get('/raw_cmd')
@cas_router.post('/raw_cmd')
def get_raw_cmd(
                request: Request,
                db: str = "", 
                cmd: Annotated[str | None, Query(description="Command to execute")] = None,
                cmd_body: RawCmd | None = None,
                ):
    """Executes a CAS command or pipeline from a string, just like CLI would"""
    org_url = request.url
    raw_cmd = cmd or (cmd_body and cmd_body.cmd)
    if raw_cmd:
        split_cmd = shell_split(raw_cmd)
        if "--json" not in split_cmd:
            split_cmd.append("--json")
        try:
            return process_request(db, split_cmd, str(org_url), request)
        except MessageException as exc:
            raise HTTPException(status_code=500, detail={"ERROR": exc.message})
    else:
        raise HTTPException(status_code=400, detail={"ERROR": "Empty raw command or invalid content type (remember to set it to application/json if using JSON in the POST request body)"})


@cas_router.get('/proc_tree', response_class=HTMLResponse)
def proc_tree(request: Request, pid: int | None = None, idx: int | None = None, db: str = ""):
    """Process tree web UI"""
    cas_db = dbs.get_nfsdb(db)
    if pid is not None and idx is None:
        entries = cas_db.get_entries_with_pid(pid)
        root_exe = {
            "count": len(entries),
            "page": 0,
            "page_max": 1,
            "entries_per_page": len(entries),
            "num_entries": len(entries),
            "origin_url": None,
            "entries": [child_renderer(x) for x in entries]
        }
    else:
        root_exe = {
            "count": 1,
            "page": 0,
            "page_max": 1,
            "entries_per_page": 1,
            "num_entries": 1,
            "origin_url": None,
            "entries": [child_renderer(cas_db.get_exec(pid, idx))]
            if pid is not None and idx is not None
            else [child_renderer(cas_db.get_exec_at_pos(0))]
        }
    return templates.TemplateResponse(name='proc_tree.html', request=request, context={"exe": root_exe, "web_ctx": get_webctx(args.ctx, db)})

@cas_router.get('/proc', response_class=ORJSONResponse)
def proc(
        pid: Annotated[int, Query(description="Process ID")],
        idx: Annotated[int, Query(description="Process index, for discriminating processes with same pid")],
        db: str = "",
        page: Annotated[int, Query(ge=0, description="Page number for open files results")] = 0,
        ) -> Response:
    """Returns detailed process information including:
    - Process metadata (PID, index, parent info)
    - Command and working directory
    - Open files (paginated)
    - Compilation/linking details if applicable
    """
    
    cas_db = dbs.get_nfsdb(db)
    data = process_info_renderer(cas_db.get_exec(pid, idx), page)
    return ORJSONResponse(data)


@cas_router.get('/proc_lookup', response_class=ORJSONResponse)
def proc_lookup(
        pid: Annotated[int, Query(description="Process ID")],
        idx: Annotated[int, Query(description="Process index, for discriminating processes with same pid")],
        db: str = "",
        ) -> ChildData:
    """Returns basic process information including:
    - Process ID and index
    - Parent process info (if available)
    - Command and working directory
    - Number of child processes
    Returns data in same format as children_of endpoint
    """
    cas_db = dbs.get_nfsdb(db)
    data = child_renderer(cas_db.get_exec(pid, idx))
    return data

class ChildResponse(BaseModel):
    count: int
    pages: int
    page: int
    children: List[ChildData]

@cas_router.get('/children', response_class=ORJSONResponse, response_model=ChildResponse)
def children_of(
        pid: Annotated[int, Query(ge=0, description="Parent process ID")],
        idx: Annotated[int, Query(ge=0, description="Parent process index")],
        db: str = "",
        etime_sort: Annotated[bool, Query(description="Sort children by execution time instead of PID")] = False,
        hide_empty: Annotated[bool, Query(description="Hide processes with no children/files/commands")] = False,
        max_results: Annotated[int, Query(ge=0, description="Maximum number of results per page")] = 20,
        depth: Annotated[int, Query(ge=0, description="Recursion depth (0 for direct children only)")] = 0,
        page: Annotated[int, Query(ge=0, description="Page number for paginated results")] = 0,
        ) -> ORJSONResponse:
    """Returns a paginated list of child processes with options to:
    - Sort by execution time or process ID
    - Filter out empty processes
    - Control recursion depth
    - Paginate results
    Returns data in same format as proc_lookup endpoint
    """
    cas_db = dbs.get_nfsdb(db)
    e = cas_db.get_exec(pid, idx)
    execs = cas_db.get_eids([(c.pid,) for c in e.child_cids])
    if etime_sort:
        execs = sorted(execs, key=lambda x: x.etime, reverse=True)
    if hide_empty:
        execs = sorted(execs, key=lambda x: (len(x.child_cids), len(x.binary), len(x.opens)), reverse=True)
    pages = math.ceil(len(execs) / max_results) if max_results > 0 else -1
    resultNum = page * max_results if max_results > 0 else 0

    data = [child_renderer(exe, cas_db, depth) for exe in execs[resultNum:(resultNum+max_results if max_results > 0 else None)]]
    result = {"count": len(execs), "pages": pages, "page": page, "children": data}
    return ORJSONResponse(result)

class AncestorsResponse(BaseModel):
    ancestors: List[ChildData]

@cas_router.get('/ancestors_of', response_class=ORJSONResponse, response_model=AncestorsResponse)
def ancestors_of(
        pid: Annotated[int, Query(ge=0, description="Process ID to find ancestors for")],
        idx: Annotated[int, Query(ge=0, description="Process index to find ancestors for")],
        db: str = "",
        ) -> ORJSONResponse:
    """Returns a list of ancestor processes including:
    - Direct parent process
    - Grandparent processes
    - All the way up to root process
    Returns data in same format as proc_lookup endpoint
    """
    cas_db = dbs.get_nfsdb(db)
    e = cas_db.get_exec(pid, idx)
    a_path = [e]
    try:
        while hasattr(e, "parent_eid"):
            e = cas_db.get_exec(e.parent_eid.pid, e.parent_eid.index)
            a_path.append(e)
    except Exception:
        pass
    data = [
        child_renderer(ent)
        for ent in reversed(a_path)
    ]
    return ORJSONResponse({"ancestors": data})


class DepsEntry(BaseModel):
    path: str
    num_deps: str
    parent: str

class DepsResponse(BaseModel):
    count: int
    page: int
    page_max: int
    num_entries: int
    entries: List[DepsEntry]

@cas_router.get('/deps_of', response_class=ORJSONResponse, response_model=DepsResponse)
def deps_of(
        path: Annotated[str, Query(description="Path to module to find dependencies for")],
        db: str = "",
        max_results: Annotated[int, Query(ge=0, description="Maximum number of results per page")] = 0,
        page: Annotated[int, Query(ge=0, description="Page number for paginated results")] = 0,
        ) -> ORJSONResponse:
    """Returns a paginated list of modules that directly depend on the given module, including:
    - Module path
    - Number of dependencies each module has
    - Parent process information
    """
    cas_db = dbs.get_nfsdb(db)
    entries = [x
                for x in sorted(cas_db.db.mdeps(path, direct=True))
                if x.path != path and x.path in cas_db.linked_module_paths()]
    result = {
        "count": len(entries),
        "page": page,
        "page_max": page,
        "num_entries": max_results,
        "entries": []
    }
    result["page_max"] = math.ceil(result["count"] / result["num_entries"])
    for entry in entries[(page * max_results):(page * max_results + max_results)]:
        mdeps = {x.path for x in cas_db.db.mdeps(entry.path, direct=True)}
        h = len([x for x in mdeps if x in cas_db.linked_module_paths() and x != entry.path])
        result["entries"].append({"path": entry.path,
                                    "num_deps": h,
                                    "parent": str(entry.parent.eid.pid) + ":" + str(entry.parent.eid.index)})
    result["entries"] = result["entries"]
    result["num_entries"] = len(result["entries"])
    return ORJSONResponse(result)


class SearchResponse(BaseModel):
    count: int
    execs: List[ChildData]

@cas_router.get('/search', response_class=ORJSONResponse, response_model=SearchResponse)
def search_files(
            filename: str | None = None,
            cmd_filter: str | None = None,
            db: str = "",
            etime_sort: Annotated[bool | None, Query(description="Sort results by how long they ran for")] = None,
            entries: Annotated[int, Query(ge=0, description="Number of entries per page")] = 0,
            page: Annotated[int, Query(ge=0, description="Page number to return, 0-indexed")] = 0,
            ) -> ORJSONResponse:
    """Find processes that opened a given file"""
    execs = []
    cas_db = dbs.get_nfsdb(db)
    command_filter = CommandFilter(cmd_filter, None, cas_db.config, cas_db.source_root) if cmd_filter is not None else None

    if filename:
        filepaths = get_opened_files(db, filename)
        if command_filter:
            execs = [open.parent
                     for openedFile in filepaths
                     for open in cas_db.get_opens_of_path(openedFile)
                     if command_filter.resolve_filters(open.parent)]
        else:
            execs = [open.parent
                     for openedFile in filepaths
                     for open in cas_db.get_opens_of_path(openedFile)]
    elif command_filter:
        execs = cas_db.filtered_execs(command_filter.libetrace_filter)

    if len(execs) == 0:
        raise HTTPException(status_code=404, detail={"ERROR": "No matches!", "count": 0})

    data = {
        "count": len(execs),
        "execs": []
    }
    if etime_sort is not None:
        execs = sorted(execs, key=lambda x: x.etime, reverse=etime_sort)
    if entries and page:
        execs = execs[page * entries:(page + 1) * entries]

    for exec in execs:
        data["execs"].append(child_renderer(exec))

    return ORJSONResponse(data)


@cas_router.get('/deps_tree', response_class=HTMLResponse)
def deps_tree(
        request: Request,
        db: str = "",
        path: Annotated[str | None, Query(description="Path to module to show dependencies for")] = None,
        filter: Annotated[str | None, Query(description="Filter pattern for module paths")] = None,
        max_results: Annotated[int, Query(ge=0, description="Maximum number of results per page")] = 10,
        page: Annotated[int, Query(ge=0, description="Page number for paginated results")] = 0,
        ) -> HTMLResponse:
    """Dependency tree web UI showing:
    - Module dependencies
    - Number of dependencies per module
    - Parent process information
    Supports filtering and pagination
    """
    cas_db = dbs.get_nfsdb(db)
    if path is not None:
        path = path.replace(" ", "+")
        entries = []
        entry: "libetrace.nfsdbEntryOpenfile | None" = None
        for x in cas_db.db.mdeps(path, direct=True):
            if x.path in cas_db.linked_module_paths():
                if x.path == path:
                    entry = x
                else:
                    entries.append(x)

        first_modules = {
            "count": 1,
            "page": 0,
            "page_max": 0,
            "entries_per_page": 1,
            "num_entries": 1,
            "origin_url": None,
            "entries": [{"path": path,
                         "num_deps": len(entries),
                         "parent": f"{entry.parent.eid.pid}:{entry.parent.eid.index}" if entry else ":"
                         }]
        }
    elif filter is not None:
        entries = [x for x in cas_db.linked_modules() if filter in x.path]
        first_modules = {
            "count": len(entries),
            "page": page,
            "page_max": len(entries) / max_results,
            "entries_per_page": max_results,
            "num_entries": 0,
            "origin_url": f"{get_webctx(args.ctx,db)}/deps_tree?filter=" + filter + "&page=" + str(page),
            "entries": []
        }
        for mod in sorted(entries)[(page * max_results):(page * max_results + max_results)]:
            mdeps = {x.path for x in cas_db.db.mdeps(mod.path, direct=True)}
            y = len([x for x in mdeps if x in cas_db.linked_module_paths() and x != mod.path])
            first_modules["entries"].append({"path": mod.path, "num_deps": y, "parent": str(mod.parent.eid.pid) + ":" + str(mod.parent.eid.index)})
    else:
        first_modules = {
            "count": len(cas_db.linked_module_paths()),
            "page": page,
            "page_max": len(cas_db.linked_module_paths()) / max_results,
            "entries_per_page": max_results,
            "num_entries": 0,
            "origin_url": f"{get_webctx(args.ctx,db)}/deps_tree?page=" + str(page),
            "entries": []
        }
        for mod in sorted(cas_db.linked_modules())[(page * max_results):(page * max_results + max_results)]:
            mdeps = {x.path for x in cas_db.db.mdeps(mod.path, direct=True)}
            y = len([x for x in mdeps if x in cas_db.linked_module_paths() and x != mod.path])
            first_modules["entries"].append({"path": mod.path, "num_deps": y, "parent": str(mod.parent.eid.pid) + ":" + str(mod.parent.eid.index)})

    first_modules["num_entries"] = len(first_modules["entries"])
    return templates.TemplateResponse(name='deps_tree.html', request=request, context={"exe": first_modules, "web_ctx": get_webctx(args.ctx, db)})


@cas_router.get('/revdeps_tree', response_class=HTMLResponse)
def revdeps_tree(
        request: Request,
        db: str = "",
        path: Annotated[str | None, Query(description="Path to file to show reverse dependencies for")] = None,
        filter: Annotated[str | None, Query(min_length=1, description="Filter pattern for file paths")] = None,
        max_results: Annotated[int, Query(ge=0, description="Maximum number of results per page")] = 15,
        page: Annotated[int, Query(ge=0, description="Page number for paginated results")] = 0,
        ) -> HTMLResponse:
    """Reverse dependency tree web UI showing:
    - Files that depend on the given file
    - Number of dependencies per file
    - File type information
    Supports filtering and pagination
    """
    max_results = 15
    cas_db = dbs.get_nfsdb(db)
    if path is not None:
        if not cas_db.db.path_exists(path):
            raise HTTPException(status_code=404, detail={"ERROR": "No matches!", "count": 0})
        path = path.replace(" ", "+")
        entries = get_opens(db, path)
        first_modules = {
            "count": len(entries),
            "page": page,
            "page_max": len(entries) / max_results,
            "entries_per_page": max_results,
            "num_entries": 0,
            "origin_url": None,
            "entries": []
        }
        for entry in entries[(page * max_results):(page * max_results + max_results)]:
            rdeps = get_rdeps(db, entry)
            first_modules["entries"].append({"path": entry,
                                             "num_deps": len(rdeps),
                                             "class": entry in cas_db.linked_module_paths(),
                                             "parent": str(entry)
                                             })
        first_modules["num_entries"] = len(first_modules["entries"])

    elif filter is not None:
        entries = get_opens(db, filter, filter=True)
        first_modules = {
            "count": len(entries),
            "page": page,
            "page_max": len(entries) / max_results,
            "entries_per_page": max_results,
            "num_entries": 0,
            "origin_url": f"{get_webctx(args.ctx,db)}/revdeps_tree?filter=" + filter + "&page=" + str(page),
            "entries": []
        }
        for mod in entries[(page * max_results):(page * max_results + max_results)]:
            rdeps = get_rdeps(db, mod)
            y = len(rdeps)
            first_modules["entries"].append({"path": mod, "num_deps": y, "class": mod in cas_db.linked_module_paths(), "parent": mod})
        first_modules["num_entries"] = len(first_modules["entries"])
    else:
        first_modules = {
            "count": 0,
            "page": 0,
            "page_max": 0,
            "entries_per_page": 0,
            "num_entries": 0,
            "origin_url": f"{get_webctx(args.ctx,db)}/revdeps_tree?page=0",
            "entries": []
        }
    return templates.TemplateResponse(name='revdeps_tree.html', request=request, context={"exe": first_modules, "web_ctx": get_webctx(args.ctx, db)})


class RevDepsEntry(BaseModel):
    path: str
    num_deps: int
    class_: bool = Field(alias="class")
    parent: str

class RevDepsResponse(BaseModel):
    count: int
    page: int
    page_max: int
    num_entries: int
    entries: List[RevDepsEntry]

@cas_router.get('/revdeps_of', response_class=ORJSONResponse, response_model=RevDepsResponse)
def revdeps_of(
            path: Annotated[str, Query(description="Path to a file")],
            db: str = "",
            max_results: Annotated[int, Query(ge=0, description="How many results can each page have")] = 10,
            page: Annotated[int, Query(ge=0, description="Paget o return")] = 0,
            ) -> ORJSONResponse:
    """Returns a list of files that depend on a given file"""
    cas_db = dbs.get_nfsdb(db)
    entries = get_rdeps(db, path)
    result = {
        "count": len(entries),
        "page": page,
        "page_max": page,
        "num_entries": max_results,
        "entries": []
    }
    result["page_max"] = math.ceil(result["count"] / result["num_entries"])
    for entry in entries[(page * max_results):(page * max_results + max_results)]:
        h = len(get_rdeps(db, entry.path))
        result["entries"].append({"path": entry.path,
                                    "num_deps": h,
                                    "class": entry.path in cas_db.linked_module_paths(),
                                    "parent": str(entry.parent.eid.pid)
                                    })
    result["entries"] = result["entries"]
    result["num_entries"] = len(result["entries"])
    return ORJSONResponse(result)

@cas_router.get('/threads', response_class=ORJSONResponse)
def threads(db: str = ""):
    cas_db = dbs.get_nfsdb(db)
    return ORJSONResponse({
            "count": cas_db.db.thread_count,
            "threads": list(cas_db.db.threads),
        })

@cas_router.get('/favicon.ico', response_class=FileResponse, include_in_schema=False)
def favicon(request: Request):
    path, stat_result = static.lookup_path('favicon.ico')
    if not stat_result:
        raise HTTPException(404)
    resp = static.file_response(path, stat_result, scope=request.scope)
    resp.media_type = "image/vnd.microsoft.icon"
    return resp

class DBStatus(BaseModel):
    db_dir: str
    nfsdb_path: str
    deps_path: str
    ftdb_paths: List[str]
    config_path: str
    loaded_nfsdb: Optional[str]
    loaded_ftdb: Optional[str]
    loaded_config: Optional[str]
    last_access: Optional[float]
    image_version: int
    db_version: str

StatusResponse = RootModel[Dict[str, DBStatus]]

@cas_router.get('/status', response_class=ORJSONResponse, response_model=StatusResponse)
@app.get('/status/', response_class=ORJSONResponse, response_model=StatusResponse)
def status(db: Optional[str] = None):
    """Returns detailed database information including:
    - Database directory and file paths
    - Load status of nfsdb, ftdb and config
    - Last access timestamp
    - Version information
    """
    try:
        return dbs.json(db)
    except MessageException as exc:
        raise HTTPException(status_code=500, detail={"ERROR": exc.message})

AllowedModules = Enum("AllowedModules", dict(map(lambda m: (m, m), allowed_modules)))

@cas_router.get('/{module}')
def get_module(request: Request, module: AllowedModules, db: str = ""):
    """Calls any supported CAS module taking the query string as arguments"""
    org_url = request.url
    query = request.query_params
    try:
        commandline = translate_to_cmdline(module.value, str(query), args.ctx, db)
        if "--json" not in commandline:
            commandline.append("--json")
        return process_request(db, commandline, str(org_url), request)
    except MessageException as exc:
        raise HTTPException(status_code=500, detail={"ERROR": exc.message})
    except libetrace.error as exc:
        raise HTTPException(status_code=500, detail={"ERROR": str(exc)})


@cas_router.get('/')
def print_api(request: Request, db: str) -> HTMLResponse:
    """Lists and links the available endpoints with simple descriptions"""
    if db in dbs.get_dbs():
        return templates.TemplateResponse(name='api_list.html', request=request, context={"dbs": dbs.get_dbs(), "web_ctx": get_webctx(args.ctx, db)})
    else:
        raise HTTPException(status_code=404, detail={"ERROR": f"Endpoint database '{db}' does not exists!" })

@app.get('/')
def main(request: Request) -> HTMLResponse:
    if dbs and dbs.multi_instance:
        return templates.TemplateResponse(name='dbs.html', request=request, context={"dbs": dbs.get_dbs(), "web_ctx": args.ctx})
    else:
        return templates.TemplateResponse(name='api_list.html', request=request, context={"dbs": dbs.get_dbs(), "web_ctx": get_webctx(args.ctx, "")})


class Config(BaseSettings):
    model_config = SettingsConfigDict(json_file="config.json", env_file=".env")

def get_app(arg: Namespace | None = None, is_test=False):
    if arg is None:
        arg = parse_server_args()[0]
    if is_test:
        # pytests require global access to args 
        # pylint: disable-next=global-statement
        global args
        args = arg
    dbs.lazy_init(args)
    app.add_middleware(CORSMiddleware, 
                       allow_origins=["*"],
                       allow_methods=["*"],
                       allow_headers=["*"],
                       allow_credentials=True)
    if args.debug:
        print("Debug mode ON")
        app.debug = args.debug
    if args.mcp or bool(os.environ.get("CAS_MCP", False)):
        from cas_mcp import mcp
        streamable = mcp.http_app(transport="streamable-http", path="/")
        lifespan_manager.add(streamable.lifespan)
        sse = mcp.http_app(transport="sse", path="/")
        lifespan_manager.add(sse.lifespan)

        app.mount("/mcp/http", streamable)
        app.mount("/mcp/sse", sse)
        if args.dbs:
            app.mount("/{db}/mcp/http", streamable)
            app.mount("/{db}/mcp/sse", sse)
    args.ctx  = args.ctx.removesuffix("/")
    app.root_path = args.ctx
    app.include_router(cas_router, prefix="/{db}" if args.dbs else "")
    return app

def parse_server_args(argv = None):
    global args
    arg_parser = argparse.ArgumentParser(description="CAS server arguments")
    arg_parser.add_argument("--port", "-p", type=int, default=os.environ.get("CAS_PORT", 8080), help="server port")
    arg_parser.add_argument("--host", type=str, default=os.environ.get("CAS_HOST", "127.0.0.1"), help="server address")
    arg_parser.add_argument("--casdb", type=str, default=os.environ.get("CAS_DB", None), help="server nfsdb")
    arg_parser.add_argument("--ftdb", type=str, default=os.environ.get("CAS_FTDB", None), help="server ftdb")
    arg_parser.add_argument("--debug", action="store_true", help="debug mode")
    arg_parser.add_argument("--verbose", action="store_true", help="verbose output")
    arg_parser.add_argument("--dbs", type=str, default=os.environ.get("CAS_DBS", None), help="Databases directory (subdirs are database names)")
    arg_parser.add_argument("--ftd", type=str, default=os.environ.get("CAS_FTDBS", None), help="Ftdb directory (subdirs match database names)")
    arg_parser.add_argument("--ctx", type=str, default=os.environ.get("CAS_CTX", "/"), help="Web context")
    arg_parser.add_argument("--workers", type=int, default=os.environ.get("WEB_CONCURRENCY", os.cpu_count() or 1), help="Number of worker processes to run")
    arg_parser.add_argument("--mcp", action="store_true", help="Enable CAS MCP server under `/mcp` and `/sse` endpoints")
    args, unknown = arg_parser.parse_known_args(argv)
    return args, unknown
def run(args=None):
    args = args or parse_server_args()[0]
    
    # TODO: remove once https://github.com/encode/uvicorn/issues/2506 is fixed
    original_uvicorn_is_alive = uvicorn.supervisors.multiprocess.Process.is_alive # type: ignore
    def patched_is_alive(self) -> bool:
        timeout = 300
        return original_uvicorn_is_alive(self, timeout)
    uvicorn.supervisors.multiprocess.Process.is_alive = patched_is_alive # type: ignore
    
    try:
        app = get_app(args)
        if args.debug:
            print(app.routes)
        uvicorn.run("cas_server:get_app", factory=True, host=args.host, port=args.port, log_level="debug" if args.debug else None, workers=args.workers, forwarded_allow_ips="*")
        print("Started CAS Server")
    except KeyboardInterrupt:
        print("Shutting down CAS Server")

if __name__ == '__main__':
    run()