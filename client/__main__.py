import sys
import os
from typing import Generator,  Iterator
from importlib.util import find_spec
import re

import libcas
from libft_db import FTDatabase
from client.cmdline import process_commandline
from client.exceptions import MessageException

def main():
    if len(sys.argv) > 1 and sys.argv[1] == "bash_complete":
        from client.argparser import get_bash_complete
        print(get_bash_complete())
        exit(0)

    cas_dir = os.path.dirname(os.path.realpath(__file__))
    # this should bypas problems with execve of etrace_parser
    if cas_dir not in os.environ["PATH"]:
        os.environ["PATH"] = os.environ["PATH"] + f":{cas_dir}"

    if not find_spec("libetrace"):
        print("CAS libetrace not found - provide libetrace.so in directory or export directory with library to PYTHONPATH")
        print("eg. export PYTHONPATH=/home/j.doe/CAS/:$PYTHONPATH")
        sys.exit(2)

    from signal import signal, SIGPIPE, SIG_DFL

    signal(SIGPIPE, SIG_DFL)

    cas_db = libcas.CASDatabase()
    ft_db = FTDatabase()
    args = sys.argv[1:]

    if "--proc-tree" in args or "--deps-tree" in args:
        import webbrowser
        import threading
        try:
            browser = webbrowser.get()
        except webbrowser.Error:
            print ("NO BROWSER")
        from cas_server import run, parse_server_args, translate_to_url
        server_args, cas_args = parse_server_args(args)
        url_args = translate_to_url([re.sub(r'--(proc|deps)-tree', r'\g<1>_tree', item) for item in cas_args])
        def browser_spawn():
            browser.open(f"http://{server_args.host}:{server_args.port}/{url_args}", new=2)

        threading.Thread(target=browser_spawn).start()
        run(server_args)
    else:
        try:
            ret = process_commandline(cas_db, ft_db=ft_db)
            if ret:
                if isinstance(ret, Iterator):
                    print(*ret, sep=os.linesep)
                elif isinstance(ret, Generator):
                    print(*ret, sep=os.linesep)
                else:
                    print(ret, flush=True)
        except MessageException as e :
            print("ERROR: " + e.message)
            sys.exit(2)

if __name__ == "__main__":
    main()