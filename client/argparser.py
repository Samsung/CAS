"""
Argument parsing module - used to handle commandline arguments.
"""

import argparse
import os
import sys
import re
from typing import Dict, List, Tuple, Any
from client.misc import get_output_renderers
from client.ide_generator.project_generator import add_params as ide_add_params
from client.ftdb_generator.ftdb_generator import add_params as ftdb_add_params

def get_api_modules() -> Dict[str, Any]:
    """
    Function used to get api keyword-module mapping.

    :return: keyword - module map
    :rtype: dict
    """
    from client.mod_misc import SourceRoot, CompilerPattern, LinkerPattern, VersionInfo, RootPid, ShowConfig, ShowStat
    from client.mod_dbops import ParseDB, Postprocess, StoreCache
    from client.mod_compilation import Compiled, CompilationInfo, RevCompsFor
    from client.mod_dependencies import DepsFor,  RevDepsFor
    from client.mod_executables import Binaries, Commands
    from client.mod_modules import LinkedModules, ModDepsFor, RevModDepsFor
    from client.mod_opened_files import Faccess, ProcRef, RefFiles
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
        'revmoddeps_for': RevModDepsFor,
        'procref': ProcRef, 'pr': ProcRef,
        'ref_files': RefFiles, 'refs': RefFiles,
        'revcomps_for': RevCompsFor, 'rcomps': RevCompsFor,
        'revdeps_for': RevDepsFor, 'rdeps': RevDepsFor,
        # Show some info
        'compiler_pattern': CompilerPattern,
        'linker_pattern': LinkerPattern,
        'root_pid': RootPid, 'rpid': RootPid,
        'version': VersionInfo, 'ver': VersionInfo, 'dbversion': VersionInfo,
        'source_root': SourceRoot, 'sroot': SourceRoot,
        'config': ShowConfig, 'cfg': ShowConfig,
        'stat': ShowStat,
        # Build database
        'parse': ParseDB,
        'postprocess': Postprocess, 'pp': Postprocess,
        'cache': StoreCache
    }


def get_api_keywords() -> List[str]:
    """
    Function used to get api keyword list.

    :return: api keyword list
    :rtype: list
    """
    return [mdl for mdl in get_api_modules()]


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
    parser = argparse.ArgumentParser(description="CAS Client Arguments", add_help=False)
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
    common_group.add_argument('--count', '-l', action='store_true', default=False, help='Display only returned items count.')

    def range_type(arg_value, pat=re.compile(r"\[(?:\:?-?\d+\:?){1,3}\]")):
        if not pat.match(arg_value):
            raise argparse.ArgumentTypeError("malformed value")
        return arg_value

    common_group.add_argument('--range', type=range_type, default=None, help='Limit range of returned records - similar to python list index [idx] or slice [start:stop:step]')
    common_group.add_argument('--sorted', '--sort', '-s', action='store_true', default=False, help='Sort results')
    common_group.add_argument('--sorting-key', type=str, default=None, help='Sort results')
    common_group.add_argument('--reverse', action='store_true', default=False, help='Reverse sort')
    common_group.add_argument('--relative', '-R', action='store_true', default=False, help='Display file paths relative source root directory')
    common_group.add_argument('--original-path', action='store_true', default=False, help='Use original symlink path instead of target path.')
    common_group.add_argument('--raw-command', action='store_true', default=False, help='Show command as list of arguments')

    view_group = parser.add_argument_group("View modifiers arguments")

    view_group.add_argument('--generate', action='store_true', default=False)
    view_group.add_argument('--makefile', action='store_true', default=False, help='Transform generated list of commands to makefile (req. commands view --generate)')
    view_group.add_argument('--all', action='store_true', default=False, help='Generate entries of all commands not only compilations (req. commands view --generate)')
    view_group.add_argument('--openrefs', action='store_true', default=False, help='Adds opened file list to generated compilation entries (req. commands view --generate)')
    view_group.add_argument('--static', action='store_true', default=False, help='')

    output_renderers = get_output_renderers()
    output_group = parser.add_argument_group("Output selection arguments")
    output_group.add_argument('--proc-tree', action='store_true', default=False, help='')
    output_group.add_argument('--deps-tree', action='store_true', default=False, help='')
    output_group.add_argument('--port', type=int, default=8383, help='Local server port (used with --proc-tree)')
    output_group.add_argument('--host', type=str, default="0.0.0.0", help='Local server host (used with --proc-tree)')
    output_group.add_argument('--output-file', '-o', type=str, default=None, help='Store results to file')
    output_group.add_argument('--generate-zip', '-z', type=str, default=None, help='Generate zip archive with results')
    for name, module in output_renderers.items():
        output_group.add_argument('--{}-output'.format(name), '--{}'.format(name), dest=name, action='store_true', default=False, help=module.Renderer.help)
        module.Renderer.append_args(parser)

    ide_add_params(parser)
    ftdb_add_params(parser)

    will_run_api_key = len([x for x in get_api_keywords() if x in args]) > 0
    if not will_run_api_key:
        parser.add_argument('-h', '--help', action='help', help='show this help message and exit')

    return parser


def get_argparser_pipeline(args: "List[str] | None") -> Tuple[argparse.Namespace, List[argparse.Namespace], argparse.ArgumentParser]:
    """
    Function takes args and splits them into common args and list of pipeline args.

    :param args: optional arguments - used in test scenarios, defaults to None
    :type args: List[str] | None
    :return: tuple with organized arguments
    :rtype: Tuple[argparse.Namespace, List[argparse.Namespace], argparse.ArgumentParser]
    """

    parser = get_common_parser(args)
    common_args, remaining_args = parser.parse_known_args(args)

    pipelines: List[List[str]] = []
    pipeline_split = get_api_keywords()
    cmd = remaining_args
    buff: List[str] = []
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

    pipeline_args: List[argparse.Namespace] = []
    for pipeline in pipelines:
        module_name = pipeline.pop(0).replace("--", "")
        mdl = get_api_modules().get(module_name, None)
        if mdl is None:
            print("ERROR: unrecognized module '{}'.".format(mdl))
            sys.exit(2)
        module_args, rest = mdl.get_argparser().parse_known_args(pipeline)
        if len(rest) > 0:
            print("ERROR: commandline switches {} not recognized as arguments of module '{}'.".format(rest, module_name))
            sys.exit(2)
        module_args.module = mdl
        module_args.name = module_name
        module_args.is_piped = True if len(pipeline_args) > 0 else False
        pipeline_args.append(module_args)

    if os.environ.get("DB_DIR", False):
        common_args.dbdir = os.path.abspath(os.environ.get("DB_DIR", ""))
    else:
        common_args.dbdir = os.path.abspath(os.path.expanduser(common_args.dbdir))

    return common_args, pipeline_args, parser

def fix_arg(argument:str)-> List[str]:
    """
    Function used to split ":" separated string into lists.

    :param argument: parameter value
    :type argument: str
    :return: parameter values list
    :rtype: List[str]
    """
    return sum([p.replace("'", "").replace('"', '').split(":") for p in argument], [])

def get_args(commandline: "str | List[str] | None" = None) -> Tuple[argparse.Namespace, List[argparse.Namespace], argparse.ArgumentParser]:
    """
    Function gets commandline from `sys.argv` or `commandline` parameter and splits it to common and pipeline args.

    :param commandline: Optional command line args list. Defaults to None.
    :type commandline: str | List[str] | None
    :return: tuple with organized arguments
    :rtype: Tuple[argparse.Namespace, List[argparse.Namespace], argparse.ArgumentParser]
    """
    if commandline is None:
        commandline = sys.argv[1:]
    elif isinstance(commandline, str):
        commandline = commandline.split()

    common_args, pipeline_args, common_parser = get_argparser_pipeline(commandline)

    for args in pipeline_args:
        if "path" in args and args.path:
            args.path = fix_arg(args.path)
        if "select" in args and args.select:
            args.select = fix_arg(args.select)
        if "append" in args and args.append:
            args.append = fix_arg(args.append)
        if "pid" in args and args.pid:
            args.pid = fix_arg(args.pid)
            args.pid = [int(p) for p in args.pid]
        if "exclude" in args and args.exclude:
            args.exclude = fix_arg(args.exclude)
        if "cdm_exclude_patterns" in args and args.cdm_exclude_patterns:
            args.cdm_exclude_patterns = fix_arg(args.cdm_exclude_patterns)
        if "cdm_exclude_files" in args and args.cdm_exclude_files:
            args.cdm_exclude_files = fix_arg(args.cdm_exclude_files)

    return common_args, pipeline_args, common_parser


def get_bash_complete() -> str:
    """
    Function returns bash completion string

    :return: bash completion string
    :rtype: str
    """
    from client.mod_base import Module
    common_actions = [name for x in get_common_parser()._actions for name in x.option_strings]
    actions = []
    if len(sys.argv) > 2:
        last_module = [x for x in reversed(sys.argv) if x in get_api_keywords()]
        if  len(last_module) > 0:
            actions = [name for x in get_api_modules().get(last_module[0], Module).get_argparser()._actions for name in x.option_strings]
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


args_map = {
    "path": lambda x: x.add_argument(
        '--path',
        type=str,
        action='append',
        default=None,
        help='Parameter path(s) - can be used multiple times, or with ":" separator.'
    ),
    "pid": lambda x: x.add_argument(
        '--pid',
        type=str,
        action='append',
        default=None,
        help='Parameter pid(s) - can be used multiple times, or with ":" separator.'
    ),
    "idx": lambda x: x.add_argument(
        '--idx',
        type=str,
        action='append',
        default=None,
        help='Parameter idx(s) - can be used multiple times, or with ":" separator.'
    ),
    "select": lambda x: x.add_argument(
        '--select', '-S',
        type=str,
        action='append',
        default=None,
        help='Parameter select(s) - can be used multiple times, or with ":" separator.'
    ),
    "append": lambda x: x.add_argument(
        '--append', '-a',
        type=str,
        action='append',
        default=None,
        help='Manually add result(s) - can be used multiple times, or with ":" separator.'
    ),
    "filter": lambda x: x.add_argument(
        '--filter', '-f',
        type=str,
        dest='open_filter',
        default=None,
        help='Filter opens results'
    ),
    "command-filter": lambda x: x.add_argument(
        '--command-filter', '-fc',
        type=str,
        dest='command_filter',
        default=None,
        help='Filter commands results'
    ),
    "commands": lambda x: x.add_argument(
        '--commands', '--show-commands',
        dest='show_commands',
        action='store_true',
        default=False,
        help='Convert file view to commands view'
    ),
    "details": lambda x: x.add_argument(
        '--details', '-t',
        action='store_true',
        default=False,
        help='Changes simple entries list to more verbose format'
    ),
    "cdm": lambda x: x.add_argument(
        '--compilation-dependency-map', '--cdm',
        dest='cdm',
        action='store_true',
        default=False,
        help='Display compilation dependency map of results'
    ),
    "cdm-ex-pt": lambda x: x.add_argument(
        '--cdm-exclude-patterns',
        type=str,
        default=None,
        help='Exclude patterns for compilation-dependency-map'
    ),
    "cdm-ex-fl": lambda x: x.add_argument(
        '--cdm-exclude-files',
        type=str,
        default=None,
        help='Exclude files for compilation-dependency-map'
    ),
    "revdeps": lambda x: x.add_argument(
        '--revdeps', '-V',
        action='store_true',
        default=False,
        help='Display reverse dependencies of results'
    ),
    "rdm": lambda x: x.add_argument(
        '--reverse-dependency-map', '--rdm',
        dest='rdm',
        action='store_true',
        default=False,
        help='Display reversed dependency map of results'
    ),
    "recursive": lambda x: x.add_argument(
        '--recursive', '-r',
        action='store_true',
        default=False,
        help=''
    ),
    "with-children": lambda x: x.add_argument(
        '--with-children',
        action='store_true',
        default=False,
        help='Include process child opened files in results'
    ),
    "direct": lambda x: x.add_argument(
        '--direct',
        action='store_true',
        default=False,
        help='Use direct dependencies - dependency processing is stopped if file is module.'),

    "parent": lambda x: x.add_argument(
        '--parent',
        action='store_true',
        default=False,
        help=''
    ),
    "binary": lambda x: x.add_argument(
        '--binary', '-b',
        type=str,
        action='append',
        default=None,
        help='Parameter binary(s) - can be used multiple times, or with ":" separator.'
    ),
    "extended": lambda x: x.add_argument(
        '--extended', '-e',
        action='store_true',
        default=False,
        help='Include additional info about compilation and linking (CC, CXX, C++, AS, LD)'
    ),
    "filerefs": lambda x: x.add_argument(
        '--filerefs', '-F',
        action='store_true',
        default=False,
        help='UNSUPPORTED'
    ),
    "dep-graph": lambda x: x.add_argument(
        '--dep-graph', '-G',
        action='store_true',
        default=False,
        help='Display generated dependencies as graph'
    ),
    "wrap-deps": lambda x: x.add_argument(
        '--wrap-deps', '-w',
        action='store_true',
        default=False,
        help='Dependency processing with respect of wrapping process.'
    ),
    "with-pipes": lambda x: x.add_argument(
        '--with-pipes', '-P',
        action='store_true',
        default=False,
        help='Dependency processing with respect of piped process.'
    ),
    "timeout": lambda x: x.add_argument(
        '--timeout', '-tm',
        type=int,
        default=None,
        help='Set timeout on dependency processing'
    ),
    "direct-global": lambda x: x.add_argument(
        '--direct-global', '--Dg',
        action='store_true',
        default=False,
        help=''
    ),
    "dry-run": lambda x: x.add_argument(
        '--dry-run', '-N',
        action='store_true',
        help=''
    ),
    "debug-fd": lambda x: x.add_argument(
        '--debug-fd', '--df',
        action='store_true',
        default=False,
        help='Print debug information from dependency processing'
    ),
    "match": lambda x: x.add_argument(
        '--match', '-m',
        action='store_true',
        default=False,
        help='Add matched files to return view'
    ),
    "cached": lambda x: x.add_argument(
        '--cached',
        action='store_true',
        default=False,
        help='Use pre-generated dependency database instead of real-time deps generation.'
    ),
    "cdb": lambda x: x.add_argument(
        '--cdb',
        action='store_true',
        default=False,
        help='Output compilations as compilation database'
    ),
    "unique-output-list": lambda x: x.add_argument(
        '--unique-output-list',
        type=str,
        action='append',
        default=None,
        help='Exclude patterns for compilation-dependency-map'
    )
}
