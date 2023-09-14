#!/usr/bin/env python3
from functools import lru_cache
from typing import List, Dict
from os import path
import json
import sys
import math
import argparse

from flask import Flask, render_template, send_from_directory, request
from flask.wrappers import Response, Request
from flask_cors import CORS
from libetrace import nfsdbEntry

from client.filtering import CommandFilter
from client.cmdline import process_commandline
from client.argparser import get_api_keywords
from client.misc import access_from_code, get_file_info
import libcas

try:
    import libetrace as _
except ModuleNotFoundError:
    print("CAS libetrace not found - provide libetrace.so in directory or export directory with library to PYTHONPATH")
    print("eg. export PYTHONPATH=/home/j.doe/CAS/:$PYTHONPATH")
    sys.exit(2)

app = Flask(__name__, template_folder='client/templates', static_folder='client/static')
CORS(app)
cas_db = libcas.CASDatabase()
root_pid = int(json.loads(process_commandline(cas_db, "root_pid --json"))["root_pid"])

allowed_modules = [keyw for keyw in get_api_keywords() if keyw not in ['parse', 'postprocess', 'pp', 'cache']]
bool_args = ["commands", "details", "cdm", "revdeps", "rdm", "recursive", "with-children", "direct", "raw-command",
             "extended", "dep-graph", "filerefs", "direct-global", "cached", "wrap-deps", "with-pipes", "original-path", "parent",
             "sorted", "sort", "reverse", "relative", "generate", "makefile", "all", "openrefs", "static", "cdb", "download", "proc-tree"]


def translate_to_cmdline(req: Request) -> List[str]:
    """
    Function translates url request to client argument list.
    """
    ret = [req.path.replace("/", "")]
    for parts in req.query_string.split(b"&"):
        k_v = parts.decode().split("=",maxsplit=1)
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


@app.route('/<module>', methods=['GET'])
def get_module(module: str) -> Response:
    if module not in allowed_modules:
        return Response(
            "Module not available",
            mimetype='text/plain')
    org_url = request.url
    commandline = translate_to_cmdline(request)
    if "entries-per-page" not in request.args and "n" not in request.args:
        commandline.append("--entries-per-page=10")
        org_url += "&entries-per-page=10"
    if "page" not in request.args:
        commandline.append("--page=0")
        org_url += "&page=0"

    if "--proc-tree" in commandline:
        e: Dict = process_commandline(cas_db, commandline)
        if isinstance(e, dict) and "entries" in e:
            e["origin_url"] = org_url
            return Response(render_template('proc_tree.html', exe=e, diable_search=True), mimetype='text/html')
        else:
            return Response(json.dumps({"ERROR":"Returned data is not executable - add '&commands=true'"}), mimetype='text/json')
    if "--cdb" in commandline:
        return Response(process_commandline(cas_db, commandline), mimetype='text/json', headers={"Content-disposition": "attachment; filename=compile_database.json"})
    elif "--makefile" in commandline:
        return Response(process_commandline(cas_db, commandline), mimetype='text/json', headers={"Content-disposition": f"attachment; filename={'build.mk' if '--all' in commandline else 'static.mk'}"})
    else:
        return Response(process_commandline(cas_db, commandline), mimetype='text/json')


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
        "etime": exe.etime,
        "children": [[c.eid.pid, c.eid.index] for c in exe.childs],
        "wpid": exe.wpid if exe.wpid else "",
        "openfiles": [[access_from_code(get_file_info(o.mode)[2]), o.path] for o in exe.opens[page*maxEntry:(page+1)*maxEntry]],
        "files_pages": math.ceil(len(exe.opens)/maxEntry),
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
            mimetype='text/json')
    return Response(
        json.dumps({"ERROR": "No such pid/idx!"}),
        mimetype='text/json')


@app.route('/proc_lookup', methods=['GET'])
def proc_lookup() -> Response:
    j = request.args
    if "pid" in j and "idx" in j:
        data = child_renderer(cas_db.get_exec(int(j["pid"]), int(j["idx"])))
        return Response(
            json.dumps(data),
            mimetype='text/json')
    return Response(
        json.dumps({"ERROR": "No such pid/idx!"}),
        mimetype='text/json')

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
        result = {"count": len(data), "pages": pages, "children": data[resultNum:resultNum+maxResults]}
    else:
        result = {"count": len(data), "pages": pages, "children": data[:maxResults]}
    return Response(
        json.dumps(result),
        mimetype='text/json', direct_passthrough=True)


@lru_cache(1)
def get_binaries(bin_path):
    return sorted([x for x in cas_db.db.bpaths() if bin_path in x])

@lru_cache(1)
def get_opened_files(file_path):
    return sorted([x for x in cas_db.db.fpaths() if file_path in x])


@lru_cache()
def get_ebins(binpath, sort_by_time):
    return sorted(cas_db.get_execs_using_binary(binpath),
                    key=lambda x: (x.etime if sort_by_time else x.eid.pid), reverse=sort_by_time)


@app.route('/search_bin', methods=['GET'])
def search_bin() -> Response:
    j = request.args
    matched_bins = get_binaries(j["bin"])
    etime_sort = True if "etime_sort" in j and j["etime_sort"] == "true" else False
    if len(matched_bins) > 0:
        execs = []
        for binPath in matched_bins:
            execs += get_ebins(binPath, etime_sort)
        if "idx" in j:
            if len(execs) > int(j["idx"]):
                exe = execs[int(j["idx"])]
            else:
                return Response(json.dumps({"INFO": "No more entries", "count": len(execs)}), mimetype='text/json')
        else:
            exe = execs[0]
        if "entries" in j and "page" in j:
            data= { 
                "count": len(execs),
                "execs": []
            }
            for exec in execs[int(j["page"])*int(j["entries"]):(int(j["page"])+1)*int(j["entries"])]:
                exeData={
                "exec": {"pid": exec.eid.pid, "idx": exec.eid.index},
                "parents": []
                }
                cur_exe = exec
                while cur_exe.eid.pid != root_pid:
                    exeData["parents"].append({"pid": cur_exe.parent.eid.pid, "idx": cur_exe.parent.eid.index})
                    cur_exe = cur_exe.parent

                exeData["parents"].reverse()
                data["execs"].append(exeData)
        else:
            data = {
                "exec": {"pid": exe.eid.pid, "idx": exe.eid.index},
                "count": len(execs),
                "parents": []
            }
            cur_exe = exe
            while cur_exe.eid.pid != root_pid:
                data["parents"].append({"pid": cur_exe.parent.eid.pid, "idx": cur_exe.parent.eid.index})
                cur_exe = cur_exe.parent

            data["parents"].reverse()

        return Response(
            json.dumps(data),
            mimetype='text/json')
    return Response(
        json.dumps({"ERROR": "No such binary!", "count": 0}),
        mimetype='text/json')

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
            mimetype='text/json')

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
        mimetype='text/json')



@app.route('/db_list', methods=['GET'])
def dblist() -> Response:
    return Response(
        json.dumps([]),
        mimetype='text/json')


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


def get_arpgparser():
    parser = argparse.ArgumentParser(description="CAS server arguments")
    parser.add_argument("--port", "-p", type=int, default=8080, help="server port")
    parser.add_argument("--host", "-h", type=str, default="0.0.0.0", help="server address")

    return parser

if __name__ == '__main__':
    if path.exists('config.json'):
        app.config.from_file('config.json', json.load)

    parser = get_arpgparser()
    args = parser.parse_args()
    app.run(args.host, args.port, use_reloader=True)
