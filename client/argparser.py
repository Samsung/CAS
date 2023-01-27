"""
Argument parsing module - used to handle commandline arguments.
"""

import argparse
import os
import sys
import re
from client.misc import get_output_renderers
from client.mod_misc import SourceRoot, CompilerPattern, LinkerPattern, VersionInfo, RootPid, ShowConfig, ShowStat
from client.mod_dbops import ParseDB, Postprocess, StoreCache
from client.mod_compilation import Compiled, CompilationInfo, RevCompsFor
from client.mod_dependencies import DepsFor,  RevDepsFor
from client.mod_executables import Binaries, Commands
from client.mod_linked_modules import LinkedModules, ModDepsFor
from client.mod_opened_files import Faccess, ProcRef, RefFiles


def get_api_modules() -> dict:
    """
    Function used to get api keyword-module mapping.

    :return: keyword - module map
    :rtype: dict
    """
    return {
        # Modules using database
        'binaries': Binaries, 'bins': Binaries, 'b': Binaries,
        'commands': Commands, 'cmds': Commands,
        'compilation_info_for': CompilationInfo, 'ci': CompilationInfo,
        'compiled': Compiled, 'cc': Compiled,
        'deps_for': DepsFor, 'deps': DepsFor,
        'faccess': Faccess, 'fa': Faccess,
        'linked_modules': LinkedModules, 'lm': LinkedModules, 'm': LinkedModules,
        'moddeps_for': ModDepsFor,
        'procref': ProcRef, 'pr': ProcRef,
        'ref_files': RefFiles, 'refs': RefFiles,
        'revcomps_for': RevCompsFor, 'rcomps': RevCompsFor,
        'revdeps_for': RevDepsFor, 'rdeps': RevDepsFor,
        # Show some info
        'compiler_pattern': CompilerPattern,
        'linker_pattern': LinkerPattern,
        'root_pid': RootPid, 'rpid': RootPid,
        'version': VersionInfo, 'ver': VersionInfo,
        'source_root': SourceRoot, 'sroot': SourceRoot,
        'config': ShowConfig, 'cfg': ShowConfig,
        'stat': ShowStat,
        # Build database
        'parse': ParseDB,
        'postprocess': Postprocess, 'pp': Postprocess,
        'cache': StoreCache
    }


def get_api_keywords() -> list:
    """
    Function used to get api keyword list.

    :return: api keyword list
    :rtype: list
    """
    return [m for m in get_api_modules()]


def get_common_parser(args=None) -> argparse.ArgumentParser:
    """
    Function generates argparse.ArgumentParser instance with common arguments.

    :param args: optional arguments - used in test scenarios, defaults to None
    :type args: list, optional
    :raises argparse.ArgumentTypeError: arguments exceptions handling
    :return: common argument parser
    :rtype: argparse.ArgumentParser
    """
    if args is None:
        args = sys.argv[1:]
    else:
        args = list(args)
    parser = argparse.ArgumentParser(description="TODO DESCRIPTION ", epilog="TODO EPILOG", add_help=False)
    common_group = parser.add_argument_group("Common arguments")

    common_group.add_argument("--verbose", "-v", action="store_true", help="Verbalize action")
    common_group.add_argument('--debug', action='store_true', default=False, help='Additional info.')

    common_group.add_argument("--config", "-cf", type=str, default=".bas_config", help="Path to configuration file (default: \".bas_config\")")
    common_group.add_argument("--config-add", type=str, default=None, help="Add string or table to provided config key path.")
    common_group.add_argument("--config-clear", type=str, default=None, help="Remove string or table from provided config key path.")
    common_group.add_argument("--dbdir", "--dir", type=str, default=".", help="Path to data file (default: \".\")")
    common_group.add_argument("--json-database", type=str, default=".nfsdb.json", help="Path to json data file (default: \".nfsdb.json\")")
    common_group.add_argument('--tracer-database', '-id', default=".nfsdb", type=str, help="Path to tracer data file (default: \".nfsdb\")")
    common_group.add_argument("--database", type=str, default=".nfsdb.img", help="Path to data file (default: \".nfsdb.img\")")
    common_group.add_argument("--deps-database", type=str, default=".nfsdb.deps.img", help="Path to data file (default: \".nfsdb.deps.img\")")
    common_group.add_argument('--set-root', type=str, default=None,  help='Specify root dir - used during database generation')

    common_group.add_argument('--mp-safe', action='store_true', default=True, help='Loads cache file in read-only mode (dedicated for parallel processing)')
    common_group.add_argument('--jobs', '-j', type=int, default=None, help='')

    common_group.add_argument('--entries-per-page', '-n', type=int, default=None, help='Number of records to display (default: depends on output renderer). Passing 0 will display all.')
    common_group.add_argument('--page', '-p', type=int, default=0, help='')

    def range_type(arg_value, pat=re.compile(r"\[(?:\:?-?\d+\:?){1,3}\]")):
        if not pat.match(arg_value):
            raise argparse.ArgumentTypeError("malformed value")
        return arg_value

    common_group.add_argument('--range', type=range_type, default=None, help='Limit range of returned records - exact copy of python list index [idx] or slice [start:stop:step]')

    common_group.add_argument('--sorted', '--sort', '-s', action='store_true', default=False, help='Sort results')
    common_group.add_argument('--reverse', action='store_true', default=False, help='Reverse sort')
    common_group.add_argument('--relative', '-R', action='store_true', default=False, help='Display file paths relative source root directory')
    common_group.add_argument('--original-path', action='store_true', default=False, help='Use original symlink path instead of target path.')

    view_group = parser.add_argument_group("View modifiers arguments")

    view_group.add_argument('--generate', action='store_true', default=False)
    view_group.add_argument('--makefile', action='store_true', default=False, help='Transform generated list of commands to makefile (req. commands view --generate)')
    view_group.add_argument('--all', action='store_true', default=False, help='Generate entries of all commands not only compilations (req. commands view --generate)')
    view_group.add_argument('--openrefs', action='store_true', default=False, help='Adds opened file list to generated compilation entries (req. commands view --generate)')
    view_group.add_argument('--static', action='store_true', default=False, help='')

    output_renderers = get_output_renderers()
    output_group = parser.add_argument_group("Output selection arguments")

    output_group.add_argument('--output-file', '-o', type=str, default=None, help='Store results to file')
    output_group.add_argument('--generate-zip', '-z', type=str, default=None, help='Generate zip archive with results')
    for name, module in output_renderers.items():
        output_group.add_argument('--{}-output'.format(name), '--{}'.format(name), dest=name, action='store_true', default=False, help=module.Renderer.help)
        module.Renderer.append_args(parser)

    will_run_api_key = len([x for x in get_api_keywords() if x in args]) > 0
    if not will_run_api_key:
        parser.add_argument('-h', '--help', action='help', help='show this help message and exit')

    return parser


def get_argparser_pipeline(args=None) -> tuple:
    """
    Function takes args and splits them into common args and list of pipeline args.

    :param args: optional arguments - used in test scenarios, defaults to None
    :type args: list, optional
    :return: tuple with organized arguments
    :rtype: tuple
    """
    if args is None:
        args = sys.argv[1:]
    else:
        args = list(args)
    parser = get_common_parser(args)
    common_args, remaining_args = parser.parse_known_args(args)

    pipelines = []
    pipeline_split = get_api_keywords()
    cmd = remaining_args
    buff = []
    for x in cmd:
        if len(buff) == 0 and x not in pipeline_split:
            print("ERROR Unknown module '{}'! use one of {}".format(x, pipeline_split))
            sys.exit(2)
        if x in pipeline_split:
            if len(buff) > 0:
                pipelines.append(buff)
                buff = []
        buff.append(x)
    if len(buff) > 0:
        pipelines.append(buff)

    pipeline_args = []
    for p in pipelines:
        module_name = p.pop(0).replace("--", "")
        m = get_api_modules().get(module_name, None)
        if m is None:
            print("ERROR: unrecognized module module '{}'.".format(m))
            sys.exit(2)
        module_args, rest = m.get_argparser().parse_known_args(p)
        if len(rest) > 0:
            print("ERROR: commandline switches {} not recognized as arguments of module '{}'.".format(rest, module_name))
            sys.exit(2)
        module_args.module = m
        module_args.name = module_name
        module_args.is_piped = True if len(pipeline_args) > 0 else False
        pipeline_args.append(module_args)

    if os.environ.get("DB_DIR", False):
        common_args.dbdir = os.path.abspath(os.environ.get("DB_DIR", ""))
    else:
        common_args.dbdir = os.path.abspath(os.path.expanduser(common_args.dbdir))

    return common_args, pipeline_args, parser


def get_args(commandline=None) -> tuple:
    """
    Function gets commandline from `sys.argv` or `commandline` and splits it to common and pipeline args.

    Args:
        commandline (list, optional): Optional command line args list. Defaults to None.

    Returns:
        tuple: prepared argparsers
    """
    if commandline:
        common_args, pipeline_args, common_parser = get_argparser_pipeline(commandline if isinstance(commandline, list) else commandline.split())
    else:
        common_args, pipeline_args, common_parser = get_argparser_pipeline()

    for args in pipeline_args:
        if "path" in args and args.path:
            args.path = sum([p.replace("'", "").replace('"', '').split(":") for p in args.path], [])
        if "select" in args and args.select:
            args.select = sum([p.replace("'", "").replace('"', '').split(":") for p in args.select], [])
        if "pid" in args and args.pid:
            args.pid = sum([p.replace("'", "").replace('"', '').split(",") for p in args.pid], [])
            args.pid = [int(p) for p in args.pid]
        if "exclude" in args and args.exclude:
            args.exclude = sum([p.replace("'", "").replace('"', '').split(":") for p in args.exclude], [])
        if "cdm_exclude_patterns" in args and args.cdm_exclude_patterns:
            args.cdm_exclude_patterns = sum([p.replace("'", "").replace('"', '').split(":") for p in args.cdm_exclude_patterns], [])
        if "cdm_exclude_files" in args and args.cdm_exclude_files:
            args.cdm_exclude_files = sum([p.replace("'", "").replace('"', '').split(":") for p in args.cdm_exclude_files], [])
    return common_args, pipeline_args, common_parser


def get_bash_complete() -> str:
    """
    Function returns bash completion string

    Returns:
        str: bash completion string
    """
    common_actions = [name for x in get_common_parser()._actions for name in x.option_strings]
    actions = []
    if len(sys.argv) > 2:
        try:
            last_module = [x for x in reversed(sys.argv) if x in get_api_keywords()]
            actions = [name for x in get_api_modules().get(last_module[0]).get_argparser()._actions for name in x.option_strings]
        except:
            pass
    return "\n".join(get_api_keywords() + common_actions + actions)


def merge_args(arg1: argparse.Namespace, arg2: argparse.Namespace) -> argparse.Namespace:
    """
    Function used to merge two Namespaces

    :param arg1: first args
    :type arg1: argparse.Namespace
    :param arg2: second args
    :type arg2: argparse.Namespace
    :return: merged args
    :rtype: argparse.Namespace
    """
    return argparse.Namespace(**vars(arg1), **vars(arg2))
