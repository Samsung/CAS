import json
import os
import sys
from typing import Tuple, Dict
from types import ModuleType
from functools import lru_cache

@lru_cache(maxsize=1024)
def get_file_info(mode: int) -> Tuple[int, int, int]:
    """
    Function split opened file bit value ( 0emmmmxx ) into:
    - exist value bit 1
    - stat mode bits 2-5
    - open mode bits 6-7

    :param mode: bit value
    :type mode: int
    :return: tuple representing exist, stat_mode, open_mode values
    :rtype: tuple
    """
    exists = True if mode >> 6 & 0b00000001 == 1 else False
    stat_mode = mode >> 2 & 0b00001111
    open_mode = mode & 0b00000011
    # stat_modes = {
    #     0x8: "r",  # regular file
    #     0x4: "d",  # directory
    #     0x2: "c",  # character device
    #     0x6: "b",  # block device
    #     0xa: "l",  # symbolic link
    #     0x1: "f",  # FIFO (named pipe)
    #     0xc: "s"  # socket
    # }
    # open_modes = {
    #     0x0: "r",  # read only
    #     0x1: "w",  # write only
    #     0x2: "rw"  # read/write mode
    # }
    return exists, stat_mode, open_mode


def access_from_code(opn_value) -> str:
    """
    Function returns string representation of open value(s).
    `opn_value` can be single value or list of values.

    :param opn_value: value or values list
    :type opn_value: int | list[int]
    :return: string representation of open value(s)
    :rtype: str
    """
    if isinstance(opn_value, list):
        ret = ''
        if 0x0 in opn_value:
            ret += 'r'
        if 0x1 in opn_value:
            ret += 'w'
        if 0x2 in opn_value:
            return 'rw'
        return ret
    else:
        return {
            0x00: "r",  # read only
            0x01: "w",  # write only
            0x02: "rw"  # read/write mode
        }[opn_value]


def stat_from_code(stat_value: int) -> str:
    """
    Function returns string representation of stat value.

    :param stat_value: stat value
    :type stat_value: int
    :return: string representation of stat value
    :rtype: str
    """
    return {
        0x0: "nonexistent",
        0x8: "file",  # regular file
        0x4: "dir",  # directory
        0x2: "char",  # character device
        0x6: "block",  # block device
        0xa: "link",  # symbolic link
        0x1: "fifo",  # FIFO (named pipe)
        0xc: "sock"  # socket
    }[stat_value]


def fix_cmd(cmd: list, join: bool = True) -> str:
    """
    Function used to escape special char in command line and returns them in json-friendly version.

    :param cmd: command line to process
    :type cmd: list
    :param join: specifies if returned command should be joined as single string or preserved as list, defaults to True
    :type join: bool, optional
    :return: escaped string
    :rtype: str
    """
    if isinstance(cmd, list):
        if join:
            return json.dumps(" ".join([x.rstrip().replace("\\", "\\\\").replace("\"", "\\\"").replace(" ", "\\ ") for x in cmd]))
        else:
            return json.dumps([x.rstrip().replace("\\", "\\\\").replace("\"", "\\\"").replace(" ", "\\ ") for x in cmd])
    if isinstance(cmd, str):
        return json.dumps(cmd.rstrip().replace("\\", "\\\\").replace("\"", "\\\"").replace(" ", "\\ "))


def fix_cmd_makefile(cmd: list, static: bool = False) -> str:
    """
    Function used to escape special char in command line and returns them in makefile-friendly version.

    :param cmd: command line to process
    :type cmd: list
    :param static: _description_, defaults to False
    :type static: bool, optional
    :return: escaped string
    :rtype: str
    """
    if static:
        return " ".join([x.rstrip().replace("#", "\\#").replace("$", "\\$").replace("(", "\\(").replace(")", "\\)").replace("\"", "\\\"").replace(" ", "\\ ")
            if x != "" else "\\\"\\\"" for x in cmd[1:]]) if isinstance(cmd, list) else cmd
    else:
        return " ".join([x.rstrip().replace("#", "\\#").replace("$", "\\$").replace("(", "\\(").replace(")", "\\)").replace("\"", "\\\"").replace(" ", "\\ ")
            if x != "" else "\\\"\\\"" for x in cmd]) if isinstance(cmd, list) else cmd


def get_output_renderers() -> Dict[str, ModuleType]:
    """
    Function check for output renderers and returns map with names and Renderer object.

    :return: map with output name and Renderer object
    :rtype: dict
    """
    ret = dict()
    output_renderers_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), 'output_renderers')
    sys.path.append(output_renderers_dir)
    for name in os.listdir(output_renderers_dir):
        if name.startswith("output_") and name.endswith(".py"):
            module_name = name[:-3]
            ret[module_name.split("_")[1]] = __import__(module_name)
    return ret


def get_config_path(config_file:str) -> str:
    """
    Function attempt to return most probably config path for CASConfig.
    Order:
    - input file provided from arg
    - config from bas/ directory
    - config in any of PYTHONPATH dirs

    :param config_file: assumed config file path
    :type config_file: str
    :return: selected config file path
    :rtype: str
    """
    if os.path.exists(config_file):
        return config_file

    # try bas/.bas_config
    main_dir = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
    if os.path.exists(os.path.join(main_dir, 'bas/.bas_config')):
        return os.path.join(main_dir, 'bas/.bas_config')
    # try in PYTHONPATH
    if "PYTHONPATH" in os.environ:
        for pyth_path in os.environ["PYTHONPATH"].split(":"):
            if os.path.exists(f"{pyth_path}bas/.bas_config"):
                return f"{pyth_path}bas/.bas_config"
    assert False, "Config not found!"


def printdbg(msg, args, file=sys.stderr, flush=True):
    if args.debug:
        print(msg, file=file, flush=flush)


def printerr(msg, file=sys.stderr, flush=True):
    print(msg, file=file, flush=flush)
