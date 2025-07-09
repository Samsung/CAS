# pylint: disable=missing-function-docstring missing-class-docstring too-many-lines missing-module-docstring line-too-long
from argparse import Namespace
import os

from fastapi import FastAPI
from fastapi.testclient import TestClient
from httpx import Response
from cas_server import get_app, translate_to_cmdline, translate_to_url



args = Namespace(**{
    "debug": False,
    "verbose": False,
    "dbs": None,
    "ctx": "/bas/",
    "ftd": None,
    "multi_instance": False,
    "port": 8080,
    "host": "0.0.0.0",
    "casdb": os.environ.get("CAS_DB", None),
    "ftdb": os.environ.get("CAS_FTDB", None)
})

app = get_app(args, is_test=True)

print(app.routes)

def is_entry_response(response: Response) -> bool:
    return response.headers.get('Content-Type').startswith('application/json') and \
        "entries" in response.json() and \
        "count" in response.json() and \
        "page" in response.json() and \
        "page_max" in response.json() and \
        "entries_per_page" in response.json() and \
        "num_entries" in response.json()

def is_status_for(response: Response, db_key) -> bool:
    return response.headers.get('Content-Type').startswith('application/json') and db_key in response.json() and \
            "db_dir" in response.json()[db_key] and \
            "nfsdb_path" in response.json()[db_key] and \
            "deps_path" in response.json()[db_key] and \
            "ftdb_paths" in response.json()[db_key] and \
            "config_path" in response.json()[db_key] and \
            "loaded_nfsdb" in response.json()[db_key] and \
            "loaded_ftdb" in response.json()[db_key] and \
            "loaded_config" in response.json()[db_key] and \
            "last_access" in response.json()[db_key] and \
            "image_version" in response.json()[db_key] and \
            "db_version" in response.json()[db_key]

def check_opens(response: Response):
    for i in response.json()['entries']:
        assert isinstance(i, str)

def check_opens_details(response: Response):
    for obj in response.json()['entries']:
        assert "filename" in obj and isinstance(obj["filename"], str) and \
        "type" in obj and isinstance(obj["type"], str) and \
        "access" in obj and isinstance(obj["access"], str) and obj["access"] in ['r','w','rw'] and \
        "open_timestamp" in obj and isinstance(int(obj["open_timestamp"]),int) and \
        "close_timestamp" in obj and isinstance(int(obj["close_timestamp"]),int) and \
        "exists" in obj and isinstance(obj["exists"], int) and \
        "link" in obj and isinstance(obj["link"], int)

def check_command(response: Response):
    for obj in response.json()['entries']:
        assert "class" in obj and obj["class"] in ["linker", "compiler", "linked", "compilation", "command"] and \
        "pid" in obj and isinstance(obj["pid"], int) and \
        "idx" in obj and isinstance(obj["idx"], int) and \
        "ppid" in obj and isinstance(obj["ppid"], int) and \
        "pidx" in obj and isinstance(obj["pidx"], int) and \
        "bin" in obj and isinstance(obj["bin"], str) and \
        "cwd" in obj and isinstance(obj["cwd"], str) and \
        "command" in obj and isinstance(obj["command"], str)

def run_cmd(app: FastAPI, url: str, min_len=None, max_len=None, check=None):
    with TestClient(app) as tc:
        response = tc.get(url)
        assert response.status_code == 200
        assert is_entry_response(response)
        if min_len and max_len:
            assert response.json() and min_len <= len(response.json()['entries']) <= max_len
        if check:
            check(response)

def test_main():
    with TestClient(app) as tc:
        print(args.ctx)
        response = tc.get(f'{args.ctx}')
        assert response.status_code == 200
        assert "Api commands" in response.text


def test_status():
    with TestClient(app) as tc:
        response = tc.get(f'{args.ctx}/status')
        assert response.status_code == 200
        assert is_status_for(response, "")


def test_lm():
    run_cmd(app, f'{args.ctx}/lm?details&n=0', 10, 20, check_opens_details)

def test_lm_deps():
    run_cmd(app, f'{args.ctx}/lm?filter=[path=*etrace_parser,type=wc]&deps&details&n=0', 400, 500, check_opens_details)

def test_ref_files():
    run_cmd(app, f'{args.ctx}/ref_files?filter=[path=*pyetrace.c,type=wc]&n=0', 1, 2, check_opens)

def test_ref_files_details():
    run_cmd(app, f'{args.ctx}/ref_files?filter=[path=*pyetrace.c,type=wc]&details&n=0', 1, 2, check_opens_details)

def test_lm_deps_commands():
    run_cmd(app, f'{args.ctx}/lm?filter=[path=*etrace_parser,type=wc]&deps&commands&n=0', 3, 5, check_command)

def test_bins():
    run_cmd(app, f'{args.ctx}/bins?filter=[path=*clang-proc*,type=wc]&n=0', 0, 1)

def test_translator():
    cmd = ["lm", "--filter=[path=*etrace_parser,type=wc]", "deps", "--commands", "--command-filter=[bin=/bin/ld,type=wc]", "vscode", "--skip-linked", "--skip-objects"]
    url = f"/bas/TEST_DB/{translate_to_url(cmd)}"
    assert cmd == translate_to_cmdline( url.split("?", maxsplit=1)[0], url.split("?", maxsplit=1)[1], "/bas", "TEST_DB")
