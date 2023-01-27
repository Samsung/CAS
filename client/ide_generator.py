

def append_args(parser):
    parser.add_argument('--ide','-g', type=str, default=None, help='', choices=['eclipse'])
    parser.add_argument('--metaname', type=str, default=None, help='')
    parser.add_argument('--remap_source_root', type=str, default=None, help='')
    parser.add_argument('--include_source', action='store_true', default=False)
    parser.add_argument('--nasm', action='store_true', default=False)
    parser.add_argument('--keep-going', action='store_true', default=False)