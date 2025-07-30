import asyncio
import sys
from fastmcp.server import FastMCP
from client.mcp.common import app_lifespan, parse_server_args
from client.mcp.bas import bas_mcp
from pathlib import Path

project_root = Path(__file__).absolute()

LOG_FILE = project_root / "custom-mcp.log"

mcp = FastMCP("CAS Server", lifespan=app_lifespan)

async def setup():
    await mcp.import_server(server=bas_mcp, prefix="bas")

if __name__ == "__main__":
    asyncio.run(setup())
    args = parse_server_args(sys.argv)[0]
    if args.transport != "stdio":
        mcp.run(transport=args.transport, host=args.host, port=args.port)
else:
    try:
        loop = asyncio.get_running_loop()
        loop.create_task(setup())
    except RuntimeError: # no event loop
        asyncio.run(setup())
