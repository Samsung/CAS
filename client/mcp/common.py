import argparse
from contextlib import asynccontextmanager
import os
import sys
from types import SimpleNamespace
from typing import Any, AsyncIterator, cast

from fastmcp import Context, FastMCP

from client.server import DBProvider


class DefaultNamespace(SimpleNamespace, argparse.Namespace):
    """SimpleNamespace that returns None instead of raising an exception on nonexistent attribute"""
    def __ini__(self, mapping_or_iterable=(), /, **kwargs):
        super(mapping_or_iterable, **kwargs)
    def __getattribute__(self, name: str) -> Any:
        try:
            return super().__getattribute__(name)
        except AttributeError:
            return None
def parse_server_args(argv = None):
    global args
    arg_parser = argparse.ArgumentParser(description="CAS server arguments")
    arg_parser.add_argument("--casdb", type=str, default=os.environ.get("CAS_DB", None), help="server nfsdb")
    arg_parser.add_argument("--ftdb", type=str, default=os.environ.get("CAS_FTDB", None), help="server ftdb")
    arg_parser.add_argument("--debug", action="store_true", help="debug mode")
    arg_parser.add_argument("--verbose", action="store_true", help="verbose output")
    arg_parser.add_argument("--dbs", type=str, default=os.environ.get("CAS_DBS", None), help="Databases directory (subdirs are database names)")
    arg_parser.add_argument("--ftd", type=str, default=os.environ.get("CAS_FTDBS", None), help="Ftdb directory (subdirs match database names)")
    arg_parser.add_argument("--host", type=str, default="localhost", help="Host address")
    arg_parser.add_argument("--port", type=int, default=8000, help="Port number")
    arg_parser.add_argument("--transport", type=str, default="stdio", choices=["stdio", "sse", "streamable-http"],  help="Transport protocol")
    args, unknown = arg_parser.parse_known_args(argv)
    return args, unknown

@asynccontextmanager
async def app_lifespan(server: FastMCP) -> AsyncIterator[DBProvider]:
    """Manage application lifecycle with type-safe context"""
    # Initialize DBs on startup
    dbs = DBProvider()
    args = parse_server_args(sys.argv)[0]
    dbs.lazy_init(args)
    yield {"dbs": dbs}


class DBType:
    DBs = "dbs"
    FTDB = "ftdb"
    NFSDB = "nfsdb"
def get_dbs(ctx: Context, type: DBType = DBType.DBs):
    dbs = cast(DBProvider, ctx.request_context.lifespan_context["dbs"])
    match type:
        case DBType.NFSDB:
            return dbs.get_nfsdb(ctx.request_context.request.path_params.get("db", ""))
        case DBType.FTDB:
            return dbs.get_ft_db(ctx.request_context.request.path_params.get("db", ""))
        case _:
            return dbs

