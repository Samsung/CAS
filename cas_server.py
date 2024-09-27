#!/usr/bin/env python3
from functools import lru_cache
from typing import List, Dict
from gevent.pywsgi import WSGIServer
from os import path
import json
import sys
import math
import argparse
from shlex import split as shell_split

from flask import Flask, render_template, send_from_directory, request
from flask.wrappers import Response, Request
from flask_cors import CORS
from libetrace import nfsdbEntry

from client.filtering import CommandFilter
from client.cmdline import process_commandline
from client.argparser import get_api_keywords
from client.misc import access_from_code, get_file_info, get_config_path
from client.exceptions import MessageException, LibFtdbException
import libcas
import libft_db

try:
    import libetrace as _
except ModuleNotFoundError:
    print("CAS libetrace not found - provide libetrace.so in directory or export directory with library to PYTHONPATH")
    print("eg. export PYTHONPATH=/home/j.doe/CAS/:$PYTHONPATH")
    sys.exit(2)

app = Flask(__name__, template_folder='client/templates', static_folder='client/static')
CORS(app)
cas_db = libcas.CASDatabase()
ft_db = libft_db.FTDatabase()

allowed_modules = [keyw for keyw in get_api_keywords() if keyw not in ['parse', 'postprocess', 'pp', 'cache']]
bool_args = ["commands", "details", "cdm", "revdeps", "rdm", "recursive", "with-children", "direct", "raw-command",
             "extended", "dep-graph", "filerefs", "direct-global", "cached", "wrap-deps", "with-pipes", "original-path", "parent",
             "sorted", "sort", "reverse", "relative", "generate", "makefile", "all", "openrefs", "static", "cdb", "download", "proc-tree", "deps-tree",
             "body", "ubody", "declbody", "definition"]


def translate_to_cmdline(req: Request) -> List[str]:
    """
    Function translates url request to client argument list.
    """
    ret = [req.path.replace("/", "")]
    for parts in req.query_string.split(b"&"):
        k_v = parts.decode().replace('%22','').replace('%27','').split("=",maxsplit=1)
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
    print(ret)
    return ret

@app.route('/reload_ftdb')
def reload_ftdb():
    path = request.args.get('path', None)
    if path is None:
        return Response("Path to ftdb image not provided", mimetype='text/plain')
    ft_db.unload_db()
    ft_db.load_db(path)
    return Response("DB reloaded")

@app.route('/schema')
def schema():
    return Response(
        json.dumps({
            "modules": allowed_modules,
            "bool_args": bool_args
        }),
        mimetype='application/json')

@app.route('/raw_cmd')
def get_raw_cmd():
    org_url = request.url
    raw_cmd = request.args.get('cmd', None)
    if raw_cmd:
        cmd = shell_split(raw_cmd)
        if "--json" not in cmd:
            cmd.append("--json")
        try:
            return process_cmdline(cmd, org_url)
        except MessageException as exc:
            return Response(json.dumps( {
                "ERROR": exc.message
            }), mimetype='application/json')
    else:
        return Response(json.dumps({"ERROR":"Empty raw command"}),
        mimetype='application/json')

@app.route('/<module>', methods=['GET'])
def get_module(module: str) -> Response:
    org_url = request.url
    try:
        commandline = translate_to_cmdline(request)
        return process_cmdline(commandline, org_url)
    except MessageException as exc:
        return Response(json.dumps( {
            "ERROR": exc.message
        }), mimetype='application/json')

def process_cmdline(commandline:List[str], org_url):
    if not any([ x for x in commandline if x.startswith("-n=") or x.startswith("--entries-per-page=")]):
        commandline.append("-n=10")
        org_url += "&n=10"
    if not any([ x for x in commandline if x.startswith("-p=") or x.startswith("--page=")]):
        commandline.append("-p=0")
        org_url += "&p=0"

    try:
        e: Dict = process_commandline(cas_db, commandline, ft_db)
    except (argparse.ArgumentError, LibFtdbException) as er:
        return Response(json.dumps({"ERROR": er.message}), mimetype='text/json')

    if "--proc-tree" in commandline:
        if isinstance(e, dict) and "entries" in e:

            e["origin_url"] = org_url
            return Response(render_template('proc_tree.html', exe=e, diable_search=True), mimetype='text/html')
        else:
            return Response(json.dumps({"ERROR":"Returned data is not executable - add '&commands=true'"}), mimetype='application/json')
    if "--deps-tree" in commandline:
        
        if isinstance(e, dict) and "entries" in e:
            e["origin_url"] = org_url
            return Response(render_template('deps_tree.html', exe=e, diable_search=True), mimetype='text/html')
        else:
            return Response(json.dumps({"ERROR":"Returned data is not executable - add '&commands=true'"}), mimetype='application/json')

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
        "stime": exe.stime,
        "etime": exe.etime,
        "children": [[c.eid.pid, c.eid.index] for c in exe.childs],
        "wpid": exe.wpid if exe.wpid else "",
        "openfiles": [[access_from_code(get_file_info(o.mode)[2]), o.path] for o in exe.opens[page*maxEntry:(page+1)*maxEntry]],
        "files_pages": math.ceil(len(exe.opens)/maxEntry),
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
        "pipe_eids": [ f'[{o.pid},{o.index}]' for o in exe.pipe_eids],
        "stime": exe.stime,
        "etime": exe.etime,
        "children": len(cas_db.get_eids([(c.pid,) for c in exe.child_cids])),
        "wpid": exe.wpid if exe.wpid else "",
        "open_len": len(exe.opens),
    }
    return ret


@app.route('/proc_tree/', methods=['GET'])
@app.route('/proc_tree', methods=['GET'])
def proc_tree() -> Response:
    j = request.args
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
    return Response(render_template('proc_tree.html', exe=root_exe), mimetype="text/html")


@app.route('/proc', methods=['GET'])
def proc() -> Response:
    j = request.args
    page=0
    if "page" in j:
        page=int(j["page"])
    if "pid" in j and "idx" in j:
        data = process_info_renderer(cas_db.get_exec(int(j["pid"]), int(j["idx"])), page)
        return Response(
            json.dumps(data),
            mimetype='application/json')
    return Response(
        json.dumps({"ERROR": "No such pid/idx!"}),
        mimetype='application/json')


@app.route('/proc_lookup', methods=['GET'])
def proc_lookup() -> Response:
    j = request.args
    if "pid" in j and "idx" in j:
        data = child_renderer(cas_db.get_exec(int(j["pid"]), int(j["idx"])))
        return Response(
            json.dumps(data),
            mimetype='application/json')
    return Response(
        json.dumps({"ERROR": "No such pid/idx!"}),
        mimetype='application/json')

@app.route('/children', methods=['GET'])
def children_of() -> Response:
    maxResults = 20
    j = request.args
    etime_sort = True if "etime_sort" in j and j["etime_sort"] == "true" else False
    hide_empty = True if "hide_empty" in j and j["hide_empty"] == "true" else False
    print("children of " + j["pid"] + ":" + j["idx"])
    page = 0
    if "page" in j.keys():
        page = int(j["page"])
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
    pages = math.ceil(len(data)/maxResults)
    print("len = ", len(data))
    print("pages = ", pages)
    
    if page:
        resultNum = page*maxResults
        print(resultNum)
        result = {"count": len(data), "pages": pages, "page": page, "children": data[resultNum:resultNum+maxResults]}
    else:
        result = {"count": len(data), "pages": pages, "page": 0, "children": data[:maxResults]}
    return Response(
        json.dumps(result),
        mimetype='application/json', direct_passthrough=True)

@app.route('/ancestors_of', methods=['GET'])
def ancestors_of() -> Response:
    j = request.args
    e = cas_db.get_exec(int(j["pid"]), int(j["idx"]))
    path = [e]
    try:
        while (hasattr(e, "parent_eid")):
            e = cas_db.get_exec(e.parent_eid.pid, e.parent_eid.index)
            path.append(e)
    except:
        pass
    data = [
        child_renderer(ent)
        for ent in reversed(path)
    ]
    result = {"ancestors": data}
    return Response(
        json.dumps(result),
        mimetype='application/json',
        direct_passthrough=True
    )

@app.route('/deps_of', methods=['GET'])
def deps_of() -> Response:
    maxResults = 10
    j = request.args
    page = 0
    if "page" in j.keys():
        page = int(j["page"])
    if "path" in j.keys():
        path = j["path"]
        entries = [x
                   for x in sorted(cas_db.db.mdeps(path, direct=True))
                   if x.path!=path and x.path in cas_db.linked_module_paths()]
        result = {
                        "count": len(entries),
                        "page": page,
                        "page_max": page,
                        "num_entries": maxResults,
                        "entries": []
                }
        result["page_max"] = math.ceil(result["count"]/result["num_entries"])
        for entry in entries[(page*maxResults):(page*maxResults+maxResults)]:
            mdeps = {x.path for x in cas_db.db.mdeps(entry.path, direct=True)}
            h = len([x for x in mdeps if x in cas_db.linked_module_paths() and x!=entry.path])
            result["entries"].append({"path": entry.path, 
                                    "num_deps": h,
                                    "parent": str(entry.parent.eid.pid)+":"+str(entry.parent.eid.index)})
        result["entries"] = result["entries"]
        result["num_entries"] = len(result["entries"])
        return Response(
            json.dumps(result),
            mimetype='application/json', direct_passthrough=True)
    
    return Response(
        json.dumps({"ERROR": "No such path!"}),
        mimetype='application/json')


@lru_cache(1)
def get_binaries(bin_path):
    return sorted([x for x in cas_db.db.binary_paths() if bin_path in x])

@lru_cache(1)
def get_opened_files(file_path):
    return sorted([x for x in cas_db.db.opens_paths() if file_path in x])


@lru_cache()
def get_ebins(binpath, sort_by_time):
    return sorted(cas_db.get_execs_using_binary(binpath),
                    key=lambda x: (x.etime if sort_by_time else x.eid.pid), reverse=sort_by_time)

@lru_cache()
def get_opens(path, filter=False):
    if filter:
        return sorted([x for x in cas_db.db.opens_paths() if path in x])
    return sorted([x for x in cas_db.db.opens_paths() if path==x])

@lru_cache()
def get_rdeps(path):
    return sorted([ f
                for fp in cas_db.db.rdeps(path, recursive=False)
                for f in cas_db.linked_modules() if f.path == fp and f.path!=path
            ])

@app.route('/search', methods=['GET'])
def search_files() -> Response:
    j = request.args.to_dict()
    execs=[]
    etime_sort = True if "etime_sort" in j and j["etime_sort"] == "true" else False
    if "filter" in j.keys():
        cmd_filter = CommandFilter(j["filter"], None, cas_db.config, cas_db.source_root)
    else:
        cmd_filter = None

    if "filename" in j.keys():
        filepaths = get_opened_files(j["filename"])
        if cmd_filter:
            execs = [ open.parent
                for openedFile in filepaths
                for open in cas_db.get_opens_of_path(openedFile)
                if cmd_filter.resolve_filters(open.parent)]
        else:
            execs = [ open.parent
                        for openedFile in filepaths
                        for open in cas_db.get_opens_of_path(openedFile) ]
    elif cmd_filter:
        execs = cas_db.filtered_execs(cmd_filter.libetrace_filter)

    if len(execs) == 0:
        return Response(
            json.dumps({"ERROR": "No matches!", "count": 0}),
            mimetype='application/json')

    data= {
            "count": len(execs),
            "execs": []
        }
    if etime_sort:
        execs = sorted(execs, key=lambda x: x.etime, reverse=etime_sort)
    if "entries" in j.keys() and "page" in j.keys():
        execs = execs[int(j["page"])*int(j["entries"]):(int(j["page"])+1)*int(j["entries"])]

    for exec in execs:
        data["execs"].append(child_renderer(exec))

    return Response(
        json.dumps(data),
        mimetype='application/json')

@app.route('/deps_tree/', methods=['GET'])
@app.route('/deps_tree', methods=['GET'])
def deps_tree() -> Response:
    maxResults = 10
    j = dict(request.args)
    if "page" not in j:
        page = 0
    else:
        page = int(j["page"])
    if "path" in j:
        j["path"] = j["path"].replace(" ", "+")
        entries=[]
        entry=""
        for x in cas_db.db.mdeps(j["path"], direct=True):
            if x.path in cas_db.linked_module_paths():
                if x.path==j["path"]:
                    entry=x
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
                                             "parent": str(entry.parent.eid.pid)+":"+str(entry.parent.eid.index)
                                            }]
                        }
    elif "filter" in j and len(j["filter"])> 0:
        entries = [x for x in cas_db.linked_modules() if j["filter"] in x.path]
        first_modules = {
                                "count": len(entries),
                                "page": page,
                                "page_max": len(entries)/maxResults,
                                "entries_per_page": maxResults,
                                "num_entries": 0,
                                "origin_url": "/deps_tree?filter="+str(j["filter"])+"&page="+str(page),
                                "entries": []
                        }
        for mod in sorted(entries)[(page*maxResults):(page*maxResults+maxResults)]:
            mdeps = {x.path for x in cas_db.db.mdeps(mod.path, direct=True)}
            y = len([x for x in mdeps if x in cas_db.linked_module_paths() and x!=mod.path])
            first_modules["entries"].append({"path": mod.path, "num_deps": y, "parent": str(mod.parent.eid.pid)+":"+str(mod.parent.eid.index)})
    else:
        first_modules = {
                                "count": len(cas_db.linked_module_paths()),
                                "page": page,
                                "page_max": len(cas_db.linked_module_paths())/maxResults,
                                "entries_per_page": maxResults,
                                "num_entries": 0,
                                "origin_url": "/deps_tree?page="+str(page),
                                "entries": []
                        }
        for mod in sorted(cas_db.linked_modules())[(page*maxResults):(page*maxResults+maxResults)]:
            mdeps = {x.path for x in cas_db.db.mdeps(mod.path, direct=True)}
            y = len([x for x in mdeps if x in cas_db.linked_module_paths() and x!=mod.path])
            first_modules["entries"].append({"path": mod.path, "num_deps": y, "parent": str(mod.parent.eid.pid)+":"+str(mod.parent.eid.index)})

    first_modules["num_entries"]=len(first_modules["entries"])
    return Response(render_template('deps_tree.html', exe=first_modules), mimetype="text/html")

@app.route('/revdeps_tree/', methods=['GET'])
@app.route('/revdeps_tree', methods=['GET'])
def revdeps_tree() -> Response:
    maxResults = 15
    j = dict(request.args)
    if "page" not in j:
        page = 0
    else:
        page = int(j["page"])
    if "path" in j:
        if not cas_db.db.path_exists(j["path"]):
            return Response(
            json.dumps({"ERROR": "No matches!", "count": 0}),
            mimetype='application/json')
        j["path"] = j["path"].replace(" ", "+")
        entries = get_opens(j["path"])
        first_modules = {
                                "count": len(entries),
                                "page": page,
                                "page_max": len(entries)/maxResults,
                                "entries_per_page": maxResults,
                                "num_entries": 0,
                                "origin_url": None,
                                "entries": []
                        }
        for entry in entries[(page*maxResults):(page*maxResults+maxResults)]:
            rdeps = get_rdeps(entry)
            first_modules["entries"].append({"path": entry,
                                             "num_deps": len(rdeps),
                                             "class": entry in cas_db.linked_module_paths(),
                                             "parent": str(entry)
                                            })
        first_modules["num_entries"]=len(first_modules["entries"])

    elif "filter" in j and len(j["filter"])> 0:
        entries = get_opens(j["filter"], filter=True)
        first_modules = {
                                "count": len(entries),
                                "page": page,
                                "page_max": len(entries)/maxResults,
                                "entries_per_page": maxResults,
                                "num_entries": 0,
                                "origin_url": "/revdeps_tree?filter="+str(j["filter"])+"&page="+str(page),
                                "entries": []
                        }
        for mod in entries[(page*maxResults):(page*maxResults+maxResults)]:
            rdeps = get_rdeps(mod)
            y = len(rdeps)
            first_modules["entries"].append({"path": mod, "num_deps": y, "class": mod in cas_db.linked_module_paths(), "parent": mod})
        first_modules["num_entries"]=len(first_modules["entries"])
    else:
        first_modules = {
                                "count": 0,
                                "page": 0,
                                "page_max": 0,
                                "entries_per_page": 0,
                                "num_entries": 0,
                                "origin_url": "/revdeps_tree?page=0",
                                "entries": []
                        }
    return Response(render_template('revdeps_tree.html', exe=first_modules), mimetype="text/html")

@app.route('/revdeps_of', methods=['GET'])
def revdeps_of() -> Response:
    maxResults = 10
    j = request.args
    page = 0
    if "page" in j.keys():
        page = int(j["page"])
    if "path" in j.keys():
        entries = get_rdeps(j["path"])
        result = {
                        "count": len(entries),
                        "page": page,
                        "page_max": page,
                        "num_entries": maxResults,
                        "entries": []
                }
        result["page_max"] = math.ceil(result["count"]/result["num_entries"])
        for entry in entries[(page*maxResults):(page*maxResults+maxResults)]:
            h=len(get_rdeps(entry.path))
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

@app.route('/db_list', methods=['GET'])
def dblist() -> Response:
    return Response(
        json.dumps([]),
        mimetype='application/json')


@app.route('/', methods=['GET'])
def main() -> Response:
    return Response(
        "",
        mimetype='text/html')


@app.route('/favicon.ico')
@app.route('/favicon.ico/')
def favicon():
    return send_from_directory(
        path.join(app.root_path, 'static'), 'favicon.ico',
        mimetype='image/vnd.microsoft.icon')

@app.route('/status', methods=['GET'])
def status():
    return Response(
        json.dumps({
            "loaded_databases": {
                "cas": cas_db.db_path,
                "ftdb": ft_db.db_path,
            },
            "cas_ok": cas_db.db_loaded,
            "ftdb_ok": ft_db.db_loaded
        }),
        mimetype="application/json"
    )

if __name__ == '__main__':
    if path.exists('config.json'):
        app.config.from_file('config.json', json.load)

    arg_parser = argparse.ArgumentParser(description="CAS server arguments")
    arg_parser.add_argument("--port", "-p", type=int, default=8080, help="server port")
    arg_parser.add_argument("--host", type=str, default="127.0.0.1", help="server address")
    arg_parser.add_argument("--casdb", type=str, default=None, help="server nfsdb")
    arg_parser.add_argument("--ftdb", type=str, default=None, help="server ftdb")
    arg_parser.add_argument("--debug", action="store_true",  help="debug mode")
    arg_parser.add_argument("--verbose", action="store_true",  help="verbose output")

    args = arg_parser.parse_args()

    if args.casdb:
        db_file = path.normpath(args.casdb)
        
        if (path.isabs(db_file)  and path.isfile(db_file)):
            config_file = path.join(path.dirname(db_file), ".bas_config")
            config = libcas.CASConfig(get_config_path(config_file))
            cas_db.set_config(config)
            cas_db.load_db(db_file, debug=args.debug, quiet=not args.verbose)
            cas_db.load_deps_db(db_file.replace(".img",".deps.img"), debug=args.debug, quiet=not args.verbose)

        else:
            print("Provide absolute path to database.")

    if args.ftdb:
        ftdb_file = path.normpath(args.ftdb)
        if (path.isabs(ftdb_file)  and path.isfile(ftdb_file)):
            ft_db.load_db(ftdb_file, debug=args.debug, quiet=not args.verbose)

    try:
        print ("Starting CAS server ... ")
        if args.debug:
            print ("Debug mode ON")
            app.debug = args.debug
        http_server = WSGIServer((args.host, args.port), app, log="default" if args.debug else None)
        http_server.serve_forever()
    except KeyboardInterrupt:
        print ("Shutting down CAS server")
