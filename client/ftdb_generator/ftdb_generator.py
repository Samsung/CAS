import importlib
import json
import os
from typing import List
from argparse import Namespace
import libftdb
import libetrace
import libcas
from client import mod_modules



def add_params(parser):
    ftdb_group = parser.add_argument_group("FTDB generation arguments")

    ftdb_group.add_argument('--ftdb-create', action='store_true', default=False, help='Create ftdb for modules')
    ftdb_group.add_argument('--aot', action='store_true', default=False, help='Create ftdb for aot (creating cdm and using default macro replacements filenames)')
    ftdb_group.add_argument('--ftdb-output', type=str, default="db.json", help='Path and name of output file for ftdb (.img)')

    ftdb_group.add_argument('-m', '--ftdb-metaname', required=False, action="store", help='Name of the output FTDB image')
    ftdb_group.add_argument('--version', required=False, action="store", help='Push a new FTDB version')
    ftdb_group.add_argument('-mr', '--macro-replacement-file', required=False, action="store", help='Path to definitions of macro replacement')
    ftdb_group.add_argument('-dd', '--additional-defines-file', required=False, action="store", help='Path to additional macro file for parsing')
    ftdb_group.add_argument('-me', '--track-macro-expansions', required=False, action="store_true", help='Path to definitions of macro replacement')
    ftdb_group.add_argument('--preserve-cta', action='store_false', default=True, help='Do not merge compile time asserts into one declaration')
    ftdb_group.add_argument("-cdm", "--compilation-dependency-map", required=False, action="store_true", help="Switch for creating compilation dependency map and using it in ftdb creation (For modules output. Not dependencies)")
    ftdb_group.add_argument('-cj', '--json-create', action='store_false', default=True, help='Create JSON database')
    ftdb_group.add_argument('--make-unique-compile-commands', action='store_true', default=False, help='Making compile commands to be unique')


def json_command(cmd):
    return json.dumps(" ".join([x.rstrip().replace("\\", "\\\\").replace("\"", "\\\"").replace(" ", "\\ ") for x in cmd]))


class FtdbGenerator:
    def __init__(self, args, cas_db: libcas.CASDatabase, latest_module: mod_modules.Module) -> None:
        self.nfsdb = cas_db
        self.args = args
        self.latest_module = latest_module

    def create_ftdb(self, deps: List[libetrace.nfsdbEntryOpenfile]):
        comps_idx = set()
        are_modules = False
        ftdb_dirname = os.path.dirname(self.args.ftdb_output)
        if ftdb_dirname != "":
            ftdb_dirname += "/"
        if isinstance(self.latest_module, mod_modules.LinkedModules) or \
                isinstance(self.latest_module, mod_modules.ModDepsFor) or \
                isinstance(self.latest_module, mod_modules.RevModDepsFor):
            warning_flag = True
            are_modules = True
            if len(deps) == 1 and self.args.ftdb_metaname is None:
                self.args.ftdb_metaname = deps[0].path.split("/")[-1]
            if self.args.ftdb_output == "db.json":
                self.args.ftdb_output = f"{self.args.ftdb_metaname}.json"
            deps_paths = [x.path for x in deps]
            deps = set()
            for x in self.nfsdb.db.mdeps(deps_paths):
                if x.path in deps and warning_flag and not self.args.make_unique_compile_commands:
                    print("\nWARNING: Compile commands not unique")
                    warning_flag = False
                elif x.path in deps and self.args.make_unique_compile_commands:
                    continue
                if x.opaque is not None and x.opaque.compilation_info is not None \
                        and x.path in x.opaque.compilation_info.file_paths:
                    comps_idx.add(x.opaque.ptr)
                deps.add(x.path)
            # Generate compilation dependency map
            if self.args.compilation_dependency_map or self.args.aot:
                cdm = {}
                mdeps = {}
                for m in deps_paths:
                    mdeps[m] = list(set([x.path for x in self.nfsdb.db.mdeps(m)]))
                for m, deplist in mdeps.items():
                    cdm[m] = []
                    for f in deplist:
                        if f in deps:
                            cdm[m].append(f)
                with open(f"{ftdb_dirname}cdm_{self.args.ftdb_metaname}.json", "w") as f:
                    f.write(json.dumps(cdm, indent=4, sort_keys=False))
                print(f"\nINFO: Created compilation dependency map file ({ftdb_dirname}cdm_{self.args.ftdb_metaname}.json)")

        else:
            deps = [x for x in deps if (x.opaque is not None and x.opaque.compilation_info is not None and x.path in x.opaque.compilation_info.file_paths)]
            for x in deps:
                comps_idx.add(x.opaque.ptr)
        clang_binary = os.path.dirname(os.path.abspath(__file__)).split("client")[0] + "clang-proc/clang-proc"
        if self.args.aot:
            self.args.macro_replacement_file = f"{os.getcwd()}/{self.args.ftdb_metaname}-macro_replacement.json"
            self.args.additional_defines_file = f"{os.getcwd()}/{self.args.ftdb_metaname}-additional_defs.json"

        print(f"\nINFO: Number of compilations: {len(comps_idx)}")
        json_vals = list()
        json_vals_openrefs = list()
        for comp_ptr in comps_idx:
            e: libetrace.nfsdbEntry = self.nfsdb.db[comp_ptr]
            if isinstance(e, list) and len(e) == 1:
                e = e[0]
                if ".cpp" in e.compilation_info.files[0].path:
                    print("\nERROR: C++ files are not supported for now\n")
                    return
            json_vals.append("{\"directory\":%s,\"command\":%s,\"file\":%s}" % (json.dumps(e.cwd), json_command(e.argv), json.dumps(e.compilation_info.files[0].path)))
            json_vals_openrefs.append("{\"directory\":%s,\"command\":%s,\"file\":%s,\"openfiles\":%s}" % (json.dumps(e.cwd), json_command(e.argv), json.dumps(e.compilation_info.files[0].path), json.dumps(sorted([o for o in e.parent.openpaths_with_children]))))
        with open(f"{ftdb_dirname}compile_commands.json", "w") as f:
            f.write(json.dumps(json.loads("[%s]" % ",".join(json_vals)), indent=4, sort_keys=False))
        print(f"\nINFO: Created compilation database file ({ftdb_dirname}compile_commands.json) \n")

        args = Namespace(phase='db', proc_binary=clang_binary, jobs=self.args.jobs, unique_cdb=False,
                         new_merge=False, fast_merge=False, legacy_merge=False, quiet=False, debug=False,
                         verbose=False, file_logs=None, output=self.args.ftdb_output, forward_output=None,
                         clean_slate=False, sw_version=self.args.version, module_info=self.args.ftdb_metaname,
                         compilation_database=f"{ftdb_dirname}compile_commands.json",
                         compilation_dependency_map=f"{ftdb_dirname}cdm_{self.args.ftdb_metaname}.json" if ((self.args.compilation_dependency_map or self.args.aot) and are_modules) else None,
                         skip_body=False, skip_switches=False, skip_defs=False, with_cta=self.args.preserve_cta,
                         taint=None, field_usage=False, include_path=False, additional_include_paths=None,
                         processor_error=False, additional_defines=self.args.additional_defines_file,
                         macro_replacement=self.args.macro_replacement_file, macro_expansion=self.args.track_macro_expansions,
                         enable_static_assert=False, test=False, job_number=None, range=None, compilation_list=None,
                         unmerged_temp=None, only_merge=None, save_intermediates=False, save_chunk_merge_temporary=False,
                         exit_on_error=False, record_database=None, fops_database=None, parallel_merge=None,
                         only_embed_fops=False, fops_record=None)

        jsonast = importlib.import_module("clang-proc.jsonast")
        ret, dbj = jsonast.create_json_db_main(args, self.args.json_create)
        base, _ = os.path.splitext(args.output)
        dbimg_path = base + ".img"
        print("\nINFO: Creating ftdb\n")
        libftdb.create_ftdb(dbj, dbimg_path, True)
        print("\nFTDB created in: ", dbimg_path, "\n")
        return
