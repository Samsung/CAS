#!/usr/bin/env python3
from typing import List
from os import path
import json
import sys

from flask import Flask, send_from_directory, request
from flask.wrappers import Response, Request
from flask_cors import CORS

from client.cmdline import process_commandline
from client.argparser import get_api_keywords
import libcas

try:
    import libetrace as _
except ModuleNotFoundError:
    print("CAS libetrace not found - provide libetrace.so in directory or export directory with library to PYTHONPATH")
    print("eg. export PYTHONPATH=/home/j.doe/CAS/:$PYTHONPATH")
    sys.exit(2)

app = Flask(__name__)
CORS(app)
cas_db = libcas.CASDatabase()

allowed_modules = [keyw for keyw in get_api_keywords() if keyw not in ['parse', 'postprocess', 'pp', 'cache']]
bool_args = ["commands", "details", "cdm", "revdeps", "rdm", "recursive", "with-children", "direct", "raw-command", "extended", "dep-graph", "filerefs", "direct-global", "cached", "wrap-deps", "with-pipes", "original-path", "parent",
            "sorted", "sort", "reverse", "relative", "generate", "makefile", "all", "openrefs", "static", "cdb", "download"]


def translate_to_cmdline(req: Request) -> List[str]:
    ret = []
    ret.append(req.path.replace("/", ""))
    for key, val in req.args.items(multi=True):
        if key in bool_args:
            if "true" in val:
                ret.append(f"--{key}")
        else:
            ret.append(f"--{key}={val}")
    ret.append("--json")
    print(req.query_string)
    print(" ".join(ret))
    return ret


@app.route('/<module>', methods=['GET'])
def get_module(module: str) -> Response:
    if module not in allowed_modules:
        return Response(
            "Module not available",
            mimetype='text/plain')
    commandline = translate_to_cmdline(request)
    if "--cdb" in commandline:
        return Response(process_commandline(cas_db, commandline), mimetype='text/json', headers={"Content-disposition": "attachment; filename=compile_database.json"})
    elif "--makefile" in commandline:
        return Response(process_commandline(cas_db, commandline), mimetype='text/json', headers={"Content-disposition": f"attachment; filename={'build.mk' if '--all' in commandline else 'static.mk'}"})
    else:
        return Response(process_commandline(cas_db, commandline), mimetype='text/plain')


@app.route('/db_list/', methods=['GET'])
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


if __name__ == '__main__':
    if path.exists('config.json'):
        app.config.from_file('config.json', json.load)

    app.run("0.0.0.0", 8080, use_reloader=True)
