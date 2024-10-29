# pylint: disable=missing-function-docstring missing-class-docstring too-many-lines missing-module-docstring line-too-long
from argparse import Namespace

from flask import Flask
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
    "casdb": None,
    "ftdb": None
})

client_single_ctx = get_app(args, is_test=True)

def is_entry_response(response) -> bool:
    return response.is_json and \
        "entries" in response.json and \
        "count" in response.json and \
        "page" in response.json and \
        "page_max" in response.json and \
        "entries_per_page" in response.json and \
        "num_entries" in response.json

def is_status_for(response, db_key) -> bool:
    return response.is_json and db_key in response.json and \
            "db_dir" in response.json[db_key] and \
            "nfsdb_path" in response.json[db_key] and \
            "deps_path" in response.json[db_key] and \
            "ftdb_paths" in response.json[db_key] and \
            "config_path" in response.json[db_key] and \
            "loaded_nfsdb" in response.json[db_key] and \
            "loaded_ftdb" in response.json[db_key] and \
            "loaded_config" in response.json[db_key] and \
            "last_access" in response.json[db_key] and \
            "image_version" in response.json[db_key] and \
            "db_verison" in response.json[db_key]

def check_opens(response):
    for i in response.json['entries']:
        assert isinstance(i, str)

def check_opens_details(response):
    for obj in response.json['entries']:
        assert "filename" in obj and isinstance(obj["filename"], str) and \
        "type" in obj and isinstance(obj["type"], str) and \
        "access" in obj and isinstance(obj["access"], str) and obj["access"] in ['r','w','rw'] and \
        "open_timestamp" in obj and isinstance(int(obj["open_timestamp"]),int) and \
        "close_timestamp" in obj and isinstance(int(obj["close_timestamp"]),int) and \
        "exists" in obj and isinstance(obj["exists"], int) and \
        "link" in obj and isinstance(obj["link"], int)

def check_command(response):
    for obj in response.json['entries']:
        assert "class" in obj and obj["class"] in ["linker", "compiler", "linked", "compilation", "command"] and \
        "pid" in obj and isinstance(obj["pid"], int) and \
        "idx" in obj and isinstance(obj["idx"], int) and \
        "ppid" in obj and isinstance(obj["ppid"], int) and \
        "pidx" in obj and isinstance(obj["pidx"], int) and \
        "bin" in obj and isinstance(obj["bin"], str) and \
        "cwd" in obj and isinstance(obj["cwd"], str) and \
        "command" in obj and isinstance(obj["command"], str)

def run_cmd(client:Flask, url: str, min_len=None, max_len=None, check=None):
    with client.test_client() as tc:
        response = tc.get(url)
        assert response.status_code == 200
        assert is_entry_response(response)
        if min_len and max_len:
            assert response.json and min_len <= len(response.json['entries']) <= max_len
        if check:
            check(response)

def test_main():
    with client_single_ctx.test_client() as tc:
        response = tc.get(f'{args.ctx}')
        assert response.status_code == 200
        assert b"Api commands" in response.data


def test_status():
    with client_single_ctx.test_client() as tc:
        response = tc.get(f'{args.ctx}status')
        assert response.status_code == 200
        assert is_status_for(response, "")


def test_lm():
    run_cmd(client_single_ctx, f'{args.ctx}lm?details&n=0', 800, 3000, check_opens_details)

def test_lm_deps():
    run_cmd(client_single_ctx, f'{args.ctx}lm?filter=[path=*common/vmlinux,type=wc]&deps&details&n=0', 30000, 80000, check_opens_details)

def test_ref_files():
    run_cmd(client_single_ctx, f'{args.ctx}ref_files?filter=[path=*.c,type=wc]&n=0', 100000, 200000, check_opens)

def test_ref_files_details():
    run_cmd(client_single_ctx, f'{args.ctx}ref_files?filter=[path=*.c,type=wc]&details&n=0', 100000, 300000, check_opens_details)

def test_lm_deps_commands():
    run_cmd(client_single_ctx, f'{args.ctx}lm?filter=[path=*common/vmlinux,type=wc]&deps&commands&n=0', 10000, 30000, check_command)

def test_bins():
    run_cmd(client_single_ctx, f'{args.ctx}bins?n=0', 1000, 3000)

def test_translator():
    cmd = ["lm", "--filter=[path=*vmlinux,type=wc]and[path=*compressed*,type=wc,negate=1]", "deps", "--commands", "--command-filter=[bin=/bin/sh,type=wc]", "vscode", "--skip-linked", "--skip-objects"]
    url = f"/bas/TEST_DB/{translate_to_url(cmd)}"
    assert cmd == translate_to_cmdline( url.split("?", maxsplit=1)[0], url.split("?", maxsplit=1)[1], "/bas", "TEST_DB")
