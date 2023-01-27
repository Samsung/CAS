
args_mapping = {
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
        default=None,
        help='Filter results'
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
    "link-type": lambda x: x.add_argument(
        '--link-type', '-T',
        type=str,
        default=None,
        choices=['shared', 'static', 'exe'],
        help='Specify linking type'
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
    "raw-command": lambda x: x.add_argument(
        '--raw-command', '-R',
        action='store_true',
        default=False,
        help='Show command as list of arguments'
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
    )
}


def add_args(args: list, parser):
    for arg in args:
        args_mapping[arg](parser)
