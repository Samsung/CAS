#!/usr/bin/env python3
from argparse import Namespace
from functools import lru_cache
from typing import List, Dict

from os import path
import json
import sys
import math
import argparse
from shlex import split as shell_split

from flask import Flask, Blueprint, render_template, send_from_directory, request
from flask.wrappers import Response, Request
from flask_cors import CORS
from urllib.parse import unquote
from libetrace import nfsdbEntry
import libetrace
from gevent.pywsgi import WSGIServer
from client.server import DBProvider
from client.filtering import CommandFilter
from client.argparser import get_api_keywords
from client.misc import access_from_code, get_file_info
from client.exceptions import MessageException, LibFtdbException

try:
    import libetrace as _
except ModuleNotFoundError:
    print("CAS libetrace not found - provide libetrace.so in directory or export directory with library to PYTHONPATH")
    print("eg. export PYTHONPATH=/home/j.doe/CAS/:$PYTHONPATH")
    sys.exit(2)

cas_single = Blueprint('cas', __name__, template_folder='client/templates', static_folder='client/static')
cas_multi = Blueprint('cas', __name__, template_folder='client/templates', static_folder='client/static')

allowed_modules = [keyw for keyw in get_api_keywords() if keyw not in ['parse', 'postprocess', 'pp', 'cache']]
bool_args = ["commands", "details", "cdm", "revdeps", "rdm", "recursive", "with-children", "direct", "raw-command",
             "extended", "dep-graph", "filerefs", "direct-global", "cached", "wrap-deps", "with-pipes", "original-path", "parent",
             "sorted", "sort", "reverse", "relative", "generate", "makefile", "all", "openrefs", "static", "cdb", "download", "proc-tree", "deps-tree",
             "body", "ubody", "declbody", "definition"]

args = Namespace()
dbs: DBProvider
bp: Blueprint


def translate_to_cmdline(req: Request, ctx: str, db: str) -> List[str]:
    """
    Function translates url request to client argument list.
    """
    url_path = req.path.lstrip("/").removeprefix(ctx.lstrip("/")).lstrip("/").removeprefix(db).lstrip("/")

    ret = [url_path]

    for parts in unquote(req.query_string.decode()).split("&"):
        k_v = parts.replace('%22', '').replace('%27', '').split("=", maxsplit=1)
        if len(k_v) == 2:
            if k_v[0] in bool_args:
                if "true" in k_v[1]:
                    ret.append(f"--{k_v[0]}" if len(k_v[0]) > 1 else f"-{k_v[0]}")
            else:
                ret.append(f"--{k_v[0]}={k_v[1]}" if len(k_v[0]) > 1 else f"-{k_v[0]}={k_v[1]}")

        elif len(k_v) == 1:
            if k_v[0] in bool_args:
                ret.append(f"--{k_v[0]}" if len(k_v[0]) > 1 else f"-{k_v[0]}")
            elif k_v[0] in allowed_modules:
                ret.append(k_v[0])

    ret.append("--json")
    return ret


def translate_to_url(commandline: List[str]) -> str:
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


def process_request(db: str, commandline: List[str], org_url):
    if not any([x for x in commandline if x.startswith("-n=") or x.startswith("--entries-per-page=")]):
        commandline.append("-n=10")
        org_url += "&n=10"
    if not any([x for x in commandline if x.startswith("-p=") or x.startswith("--page=")]):
        commandline.append("-p=0")
        org_url += "&p=0"

    try:
        e: Dict = dbs.process_command(db, commandline)
    except (argparse.ArgumentError, LibFtdbException) as er:
        return Response(json.dumps({"ERROR": er.message}), mimetype='text/json')

    if "--proc-tree" in commandline:
        if isinstance(e, dict) and "entries" in e:

            e["origin_url"] = org_url
            return Response(render_template('proc_tree.html', exe=e, web_ctx=get_webctx(args.ctx, db), diable_search=True), mimetype='text/html')
        else:
            return Response(json.dumps({"ERROR": "Returned data is not executable - add '&commands=true'"}), mimetype='application/json')
    if "--deps-tree" in commandline:

        if isinstance(e, dict) and "entries" in e:
            e["origin_url"] = org_url
            return Response(render_template('deps_tree.html', exe=e, web_ctx=get_webctx(args.ctx, db), diable_search=True), mimetype='text/html')
        else:
            return Response(json.dumps({"ERROR": "Returned data is not executable - add '&commands=true'"}), mimetype='application/json')

    if "--cdb" in commandline:
        return Response(e, mimetype='application/json', headers={"Content-disposition": "attachment; filename=compile_database.json"})
    elif "--makefile" in commandline:
        return Response(e, mimetype='text/plain', headers={"Content-disposition": f"attachment; filename={'build.mk' if '--all' in commandline else 'static.mk'}"})
    else:
        return Response(e, mimetype='application/json')


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


def child_renderer(exe: nfsdbEntry):
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
        "children": len(exe.child_cids),
        "wpid": exe.wpid if exe.wpid else "",
        "open_len": len(exe.opens),
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
def get_opens(db, path, filter=False):
    cas_db = dbs.get_nfsdb(db)
    if filter:
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
def get_webctx(ctx, db):
    ret = path.join(ctx, db)
    return ret if ret != '/' else ''


@cas_single.route('/reload_ftdb/', defaults={'db': ""}, methods=['GET'], strict_slashes=False)
@cas_multi.route('/<db>/reload_ftdb/', methods=['GET'], strict_slashes=False)
def reload_ftdb(db: str):
    path = request.args.get('path', None)
    if path is None:
        return Response("Path to ftdb image not provided", mimetype='text/plain')
    dbs.switch_ftdb(db, path)
    return Response("DB reloaded")


@cas_single.route('/raw_cmd/', defaults={'db': ""}, methods=['GET'], strict_slashes=False)
@cas_multi.route('/<db>/raw_cmd/', methods=['GET'], strict_slashes=False)
def get_raw_cmd(db: str):
    org_url = request.url
    raw_cmd = request.args.get('cmd', None)
    if raw_cmd:
        cmd = shell_split(raw_cmd)
        if "--json" not in cmd:
            cmd.append("--json")
        try:
            return process_request(db, cmd, org_url)
        except MessageException as exc:
            return Response(json.dumps({
                "ERROR": exc.message
            }), mimetype='text/json')
    else:
        return Response(json.dumps({"ERROR": "Empty raw command"}),
                        mimetype='text/json')


@cas_single.route('/proc_tree/', defaults={'db': ""}, methods=['GET'], strict_slashes=False)
@cas_multi.route('/<db>/proc_tree/', methods=['GET'], strict_slashes=False)
def proc_tree(db: str) -> Response:
    j = request.args
    cas_db = dbs.get_nfsdb(db)
    if "pid" in j and "idx" not in j:
        entries = cas_db.get_entries_with_pid(int(j["pid"]))
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
            "entries": [child_renderer(cas_db.get_exec(int(j["pid"]), int(j["idx"])))]
            if "pid" in j and "idx" in j
            else [child_renderer(cas_db.get_exec_at_pos(0))]
        }
    return Response(render_template('proc_tree.html', exe=root_exe, web_ctx=get_webctx(args.ctx, db)), mimetype="text/html")


@cas_single.route('/proc/', defaults={'db': ""}, methods=['GET'], strict_slashes=False)
@cas_multi.route('/<db>/proc/', methods=['GET'], strict_slashes=False)
def proc(db: str) -> Response:
    j = request.args
    page = 0
    if "page" in j:
        page = int(j["page"])
    if "pid" in j and "idx" in j:
        cas_db = dbs.get_nfsdb(db)
        data = process_info_renderer(cas_db.get_exec(int(j["pid"]), int(j["idx"])), page)
        return Response(
            json.dumps(data),
            mimetype='application/json')
    return Response(
        json.dumps({"ERROR": "No such pid/idx!"}),
        mimetype='application/json')


@cas_single.route('/proc_lookup', defaults={'db': ""}, methods=['GET'])
@cas_multi.route('/<db>/proc_lookup', methods=['GET'])
def proc_lookup(db: str) -> Response:
    j = request.args
    if "pid" in j and "idx" in j:
        cas_db = dbs.get_nfsdb(db)
        data = child_renderer(cas_db.get_exec(int(j["pid"]), int(j["idx"])))
        return Response(
            json.dumps(data),
            mimetype='application/json')
    return Response(
        json.dumps({"ERROR": "No such pid/idx!"}),
        mimetype='application/json')


@cas_single.route('/children', defaults={'db': ""}, methods=['GET'])
@cas_multi.route('/<db>/children', methods=['GET'])
def children_of(db: str) -> Response:
    maxResults = 20
    j = request.args
    etime_sort = True if "etime_sort" in j and j["etime_sort"] == "true" else False
    hide_empty = True if "hide_empty" in j and j["hide_empty"] == "true" else False
    page = 0
    if "page" in j.keys():
        page = int(j["page"])
    cas_db = dbs.get_nfsdb(db)
    e = cas_db.get_exec(int(j["pid"]), int(j["idx"]))
    execs = cas_db.get_eids([(c.pid,) for c in e.child_cids])
    if etime_sort:
        execs = sorted(execs, key=lambda x: x.etime, reverse=True)
    if hide_empty:
        execs = sorted(execs, key=lambda x: (len(x.child_cids), len(x.binary), len(x.opens)), reverse=True)
    data = [
        child_renderer(ent)
        for ent in execs
    ]
    pages = math.ceil(len(data) / maxResults)

    if page:
        resultNum = page * maxResults
        result = {"count": len(data), "pages": pages, "page": page, "children": data[resultNum:resultNum + maxResults]}
    else:
        result = {"count": len(data), "pages": pages, "page": 0, "children": data[:maxResults]}
    return Response(
        json.dumps(result),
        mimetype='application/json', direct_passthrough=True)


@cas_single.route('/ancestors_of/', defaults={'db': ""}, methods=['GET'], strict_slashes=False)
@cas_multi.route('/<db>/ancestors_of/', methods=['GET'], strict_slashes=False)
def ancestors_of(db: str) -> Response:
    j = request.args
    cas_db = dbs.get_nfsdb(db)
    e = cas_db.get_exec(int(j["pid"]), int(j["idx"]))
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
    return Response(
        json.dumps({"ancestors": data}),
        mimetype='application/json',
        direct_passthrough=True
    )


@cas_single.route('/deps_of/', defaults={'db': ""}, methods=['GET'], strict_slashes=False)
@cas_multi.route('/<db>/deps_of/', methods=['GET'], strict_slashes=False)
def deps_of(db: str) -> Response:
    maxResults = 10
    j = request.args
    page = 0
    cas_db = dbs.get_nfsdb(db)
    if "page" in j.keys():
        page = int(j["page"])
    if "path" in j.keys():
        path = j["path"]
        entries = [x
                   for x in sorted(cas_db.db.mdeps(path, direct=True))
                   if x.path != path and x.path in cas_db.linked_module_paths()]
        result = {
            "count": len(entries),
            "page": page,
            "page_max": page,
            "num_entries": maxResults,
            "entries": []
        }
        result["page_max"] = math.ceil(result["count"] / result["num_entries"])
        for entry in entries[(page * maxResults):(page * maxResults + maxResults)]:
            mdeps = {x.path for x in cas_db.db.mdeps(entry.path, direct=True)}
            h = len([x for x in mdeps if x in cas_db.linked_module_paths() and x != entry.path])
            result["entries"].append({"path": entry.path,
                                      "num_deps": h,
                                      "parent": str(entry.parent.eid.pid) + ":" + str(entry.parent.eid.index)})
        result["entries"] = result["entries"]
        result["num_entries"] = len(result["entries"])
        return Response(
            json.dumps(result),
            mimetype='application/json', direct_passthrough=True)

    return Response(
        json.dumps({"ERROR": "No such path!"}),
        mimetype='application/json')


@cas_single.route('/search/', defaults={'db': ""}, methods=['GET'], strict_slashes=False)
@cas_multi.route('/<db>/search/', methods=['GET'], strict_slashes=False)
def search_files(db) -> Response:
    j = request.args.to_dict()
    execs = []
    cas_db = dbs.get_nfsdb(db)
    etime_sort = True if "etime_sort" in j and j["etime_sort"] == "true" else False
    if "filter" in j.keys():
        cmd_filter = CommandFilter(j["filter"], None, cas_db.config, cas_db.source_root)
    else:
        cmd_filter = None

    if "filename" in j.keys():
        filepaths = get_opened_files(db, j["filename"])
        if cmd_filter:
            execs = [open.parent
                     for openedFile in filepaths
                     for open in cas_db.get_opens_of_path(openedFile)
                     if cmd_filter.resolve_filters(open.parent)]
        else:
            execs = [open.parent
                     for openedFile in filepaths
                     for open in cas_db.get_opens_of_path(openedFile)]
    elif cmd_filter:
        execs = cas_db.filtered_execs(cmd_filter.libetrace_filter)

    if len(execs) == 0:
        return Response(
            json.dumps({"ERROR": "No matches!", "count": 0}),
            mimetype='application/json')

    data = {
        "count": len(execs),
        "execs": []
    }
    if etime_sort:
        execs = sorted(execs, key=lambda x: x.etime, reverse=etime_sort)
    if "entries" in j.keys() and "page" in j.keys():
        execs = execs[int(j["page"]) * int(j["entries"]):(int(j["page"]) + 1) * int(j["entries"])]

    for exec in execs:
        data["execs"].append(child_renderer(exec))

    return Response(
        json.dumps(data),
        mimetype='application/json')


@cas_single.route('/deps_tree/', defaults={'db': ""}, methods=['GET'], strict_slashes=False)
@cas_multi.route('/<db>/deps_tree/', methods=['GET'], strict_slashes=False)
def deps_tree(db: str) -> Response:
    maxResults = 10
    j = dict(request.args)
    if "page" not in j:
        page = 0
    else:
        page = int(j["page"])
    cas_db = dbs.get_nfsdb(db)
    if "path" in j:
        j["path"] = j["path"].replace(" ", "+")
        entries = []
        entry: libetrace.nfsdbEntryOpenfile
        for x in cas_db.db.mdeps(j["path"], direct=True):
            if x.path in cas_db.linked_module_paths():
                if x.path == j["path"]:
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
            "entries": [{"path": j["path"],
                         "num_deps": len(entries),
                         "parent": str(entry.parent.eid.pid) + ":" + str(entry.parent.eid.index)
                         }]
        }
    elif "filter" in j and len(j["filter"]) > 0:
        entries = [x for x in cas_db.linked_modules() if j["filter"] in x.path]
        first_modules = {
            "count": len(entries),
            "page": page,
            "page_max": len(entries) / maxResults,
            "entries_per_page": maxResults,
            "num_entries": 0,
            "origin_url": f"{get_webctx(args.ctx,db)}/deps_tree?filter=" + str(j["filter"]) + "&page=" + str(page),
            "entries": []
        }
        for mod in sorted(entries)[(page * maxResults):(page * maxResults + maxResults)]:
            mdeps = {x.path for x in cas_db.db.mdeps(mod.path, direct=True)}
            y = len([x for x in mdeps if x in cas_db.linked_module_paths() and x != mod.path])
            first_modules["entries"].append({"path": mod.path, "num_deps": y, "parent": str(mod.parent.eid.pid) + ":" + str(mod.parent.eid.index)})
    else:
        first_modules = {
            "count": len(cas_db.linked_module_paths()),
            "page": page,
            "page_max": len(cas_db.linked_module_paths()) / maxResults,
            "entries_per_page": maxResults,
            "num_entries": 0,
            "origin_url": f"{get_webctx(args.ctx,db)}/deps_tree?page=" + str(page),
            "entries": []
        }
        for mod in sorted(cas_db.linked_modules())[(page * maxResults):(page * maxResults + maxResults)]:
            mdeps = {x.path for x in cas_db.db.mdeps(mod.path, direct=True)}
            y = len([x for x in mdeps if x in cas_db.linked_module_paths() and x != mod.path])
            first_modules["entries"].append({"path": mod.path, "num_deps": y, "parent": str(mod.parent.eid.pid) + ":" + str(mod.parent.eid.index)})

    first_modules["num_entries"] = len(first_modules["entries"])
    return Response(render_template('deps_tree.html', exe=first_modules, web_ctx=get_webctx(args.ctx, db)), mimetype="text/html")


@cas_single.route('/revdeps_tree/', defaults={'db': ""}, methods=['GET'], strict_slashes=False)
@cas_multi.route('/<db>/revdeps_tree/', methods=['GET'], strict_slashes=False)
def revdeps_tree(db: str) -> Response:
    maxResults = 15
    j = dict(request.args)
    if "page" not in j:
        page = 0
    else:
        page = int(j["page"])
    cas_db = dbs.get_nfsdb(db)
    if "path" in j:
        if not cas_db.db.path_exists(j["path"]):
            return Response(
                json.dumps({"ERROR": "No matches!", "count": 0}),
                mimetype='application/json')
        j["path"] = j["path"].replace(" ", "+")
        entries = get_opens(db, j["path"])
        first_modules = {
            "count": len(entries),
            "page": page,
            "page_max": len(entries) / maxResults,
            "entries_per_page": maxResults,
            "num_entries": 0,
            "origin_url": None,
            "entries": []
        }
        for entry in entries[(page * maxResults):(page * maxResults + maxResults)]:
            rdeps = get_rdeps(db, entry)
            first_modules["entries"].append({"path": entry,
                                             "num_deps": len(rdeps),
                                             "class": entry in cas_db.linked_module_paths(),
                                             "parent": str(entry)
                                             })
        first_modules["num_entries"] = len(first_modules["entries"])

    elif "filter" in j and len(j["filter"]) > 0:
        entries = get_opens(db, j["filter"], filter=True)
        first_modules = {
            "count": len(entries),
            "page": page,
            "page_max": len(entries) / maxResults,
            "entries_per_page": maxResults,
            "num_entries": 0,
            "origin_url": f"{get_webctx(args.ctx,db)}/revdeps_tree?filter=" + str(j["filter"]) + "&page=" + str(page),
            "entries": []
        }
        for mod in entries[(page * maxResults):(page * maxResults + maxResults)]:
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
    return Response(render_template('revdeps_tree.html', exe=first_modules, web_ctx=get_webctx(args.ctx, db)), mimetype="text/html")


@cas_single.route('/revdeps_of', defaults={'db': ""}, methods=['GET'], strict_slashes=False)
@cas_multi.route('/<db>/revdeps_of', methods=['GET'], strict_slashes=False)
def revdeps_of(db: str) -> Response:
    maxResults = 10
    j = request.args
    page = 0
    if "page" in j.keys():
        page = int(j["page"])
    cas_db = dbs.get_nfsdb(db)
    if "path" in j.keys():
        entries = get_rdeps(db, j["path"])
        result = {
            "count": len(entries),
            "page": page,
            "page_max": page,
            "num_entries": maxResults,
            "entries": []
        }
        result["page_max"] = math.ceil(result["count"] / result["num_entries"])
        for entry in entries[(page * maxResults):(page * maxResults + maxResults)]:
            h = len(get_rdeps(db, entry.path))
            result["entries"].append({"path": entry.path,
                                      "num_deps": h,
                                      "class": entry.path in cas_db.linked_module_paths(),
                                      "parent": str(entry.parent.eid.pid)
                                      })
        result["entries"] = result["entries"]
        result["num_entries"] = len(result["entries"])
        return Response(
            json.dumps(result),
            mimetype='application/json', direct_passthrough=True)

    return Response(
        json.dumps({"ERROR": "No such path!"}),
        mimetype='application/json')


@cas_single.route('/favicon.ico/', strict_slashes=False)
@cas_multi.route('/favicon.ico/', strict_slashes=False)
def favicon():
    return send_from_directory(
        path.join(cas_single.root_path, 'static'), 'favicon.ico',
        mimetype='image/vnd.microsoft.icon')


@cas_single.route('/status/', defaults={'db': ""}, methods=['GET'], strict_slashes=False)
@cas_multi.route('/<db>/status/', methods=['GET'], strict_slashes=False)
@cas_multi.route('/status/', defaults={'db': ""}, methods=['GET'], strict_slashes=False)
def status(db: str):
    return Response(
        json.dumps(dbs.json(db), indent=2),
        mimetype="application/json"
    )


@cas_single.route('/<module>/', defaults={'db': ""}, strict_slashes=False)
@cas_multi.route('/<db>/<module>/', strict_slashes=False)
def get_module(db: str, module: str) -> Response:
    org_url = request.url
    try:
        commandline = translate_to_cmdline(request, args.ctx, db)
        return process_request(db, commandline, org_url)
    except MessageException as exc:
        return Response(json.dumps({
            "ERROR": exc.message
        }), mimetype='text/json')
    except libetrace.error as exc:
        return Response(json.dumps({
            "ERROR": str(exc)
        }), mimetype='text/json')


@cas_multi.route('/<db>/', methods=['GET'], strict_slashes=False)
def print_api(db: str) -> Response:
    return Response(render_template('api_list.html', dbs=dbs.db_map, web_ctx=get_webctx(args.ctx, db)), mimetype="text/html")


@cas_single.route('/', methods=['GET'], strict_slashes=False)
@cas_multi.route('/', methods=['GET'], strict_slashes=False)
def main() -> Response:
    if dbs and dbs.multi_instance:
        return Response(render_template('dbs.html', dbs=dbs.db_map, web_ctx=args.ctx), mimetype="text/html")
    else:
        return Response(render_template('api_list.html', dbs=dbs.db_map, web_ctx=args.ctx), mimetype="text/html")

if __name__ == '__main__':

    arg_parser = argparse.ArgumentParser(description="CAS server arguments")
    arg_parser.add_argument("--port", "-p", type=int, default=8080, help="server port")
    arg_parser.add_argument("--host", type=str, default="127.0.0.1", help="server address")
    arg_parser.add_argument("--casdb", type=str, default=None, help="server nfsdb")
    arg_parser.add_argument("--ftdb", type=str, default=None, help="server ftdb")
    arg_parser.add_argument("--debug", action="store_true", help="debug mode")
    arg_parser.add_argument("--verbose", action="store_true", help="verbose output")
    arg_parser.add_argument("--dbs", type=str, default=None, help="Databases directory (subdirs are database names)")
    arg_parser.add_argument("--ftd", type=str, default=None, help="Ftdb directory (subdirs match database names)")
    arg_parser.add_argument("--ctx", type=str, default='/', help="Web context")

    args = arg_parser.parse_args()

    dbs = DBProvider(args)

    try:
        app = Flask(__name__, template_folder='client/templates', static_folder='client/static', static_url_path=path.join(args.ctx, 'static'))
        if path.exists('config.json'):
            app.config.from_file('config.json', json.load)
        CORS(app)
        if args.debug:
            print("Debug mode ON")
            app.debug = args.debug
        args.ctx = args.ctx if args.ctx.endswith("/") else args.ctx + "/"
        bp = cas_single if not args.dbs else cas_multi
        app.register_blueprint(bp, url_prefix=args.ctx)

        http_server = WSGIServer((args.host, args.port), app, log="default" if args.debug else None)
        if args.debug:
            print(app.url_map)
        print("Started CAS Server")
        http_server.serve_forever()
    except KeyboardInterrupt:
        print("Shutting down CAS Server")
