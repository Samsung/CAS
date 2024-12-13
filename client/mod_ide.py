import argparse
import re
import sys
import shutil
import os
import json
import uuid
import zipfile
import fnmatch
from pathlib import Path
from typing import Generator, Iterator, List, Optional, Set, Dict

from flask import send_file

from client.mod_modules import LinkedModules
import libcas
import libetrace
from client.exceptions import PipelineException,IDEGenerationException
from libcas import progressbar
from libft_db import FTDatabase
from client.mod_base import Module, ModulePipeline, PipedModule, FilterableModule
from client.output_renderers.output import DataTypes
from client.misc import access_from_code, get_file_info, printdbg, printcli, printerr, get_def_ip


class VSCodeProjectGenerator(Module, PipedModule, FilterableModule):

    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="Module used for cache creation.")
        arg_group = module_parser.add_argument_group("IDE project generation arguments")

        arg_group.add_argument('--remote', '-r', type=str, action='store', nargs='?', const=get_def_ip(), default=None, help='Generate IDE project with remote access')
        arg_group.add_argument('--remote-user', '-u', type=str, action='store', nargs='?', const=os.getlogin(), default=None, help='Remote access username')
        arg_group.add_argument('--remote-ws', '-w', type=str, default=None, help='Remote access workspace directory')
        arg_group.add_argument('--remote-cas', type=str, default=None, help='Remote bas server')
        arg_group.add_argument('--type', '-t', type=str, choices=["local", "p4", "repo", "package"], default="local", help='Specify type of workspace')
        arg_group.add_argument('--metaname', type=str, help='Project name')
        arg_group.add_argument('--output-dir', type=str, help='Output directory')
        arg_group.add_argument('--output-zip', type=str, help='Archive file path')

        arg_group.add_argument('--skip-pattern', type=str, default=None, help='Skip copy files matching pattern to project')
        arg_group.add_argument('--skip-wildcard', type=str, default=None, help='Skip copy files matching wildcard(s) to project')
        arg_group.add_argument('--def', dest="defs", type=str, default=None, help='add global defines as comma separated list')        
        arg_group.add_argument('--remap-source-root', type=str, default=None, help='Change original source root')
        arg_group.add_argument('--copy', action='store_true', default=False, help='Copy files instead of making symlinks')
        arg_group.add_argument('--archive', action='store_true', default=False, help='Store workspace as archive.')
        arg_group.add_argument('--add-db', action='store_true', default=False, help='Adds CAS database to archive.')
        arg_group.add_argument('--skip-asm', action='store_true', default=False, help="Don't copy Disable asm files to project")
        arg_group.add_argument('--skip-linked', action='store_true', default=False, help="Don't copy linked files to project")
        arg_group.add_argument('--skip-objects', action='store_true', default=False, help="Don't copy object files to project")

        return module_parser

    def set_piped_arg(self, data, data_type: type):
        if data_type == str:
            raise PipelineException("Wrong pipe input data {}.".format(data_type))
        if data_type == libetrace.nfsdbEntryOpenfile:
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.data = data
        if data_type == libetrace.nfsdbEntry:
            raise PipelineException("Wrong pipe input data {}.".format(data_type))

    def get_data(self):
        if self.data:
            return self.generate(), DataTypes.null_data, None, None
        else:
            data = {
                    "folders": [
                        {"path": "."}
                    ],
                    "settings": {
                        "cas.manifest": {
                            "basProvider": {
                                "path": self.nfsdb.db_path,
                                "type": "file"
                            },
                            "sourceRepo": {
                                "type": "local",
                                "sourceRoot": self.nfsdb.source_root
                            }
                        }
                    }
                }
            if self.args.remote:
                data["settings"]["cas.manifest"]["remote"] = {"hostname": self.args.remote}
                if self.args.remote_user:
                    data["settings"]["cas.manifest"]["remote"]["username"] = self.args.remote_user
            return data, DataTypes.config_part_data, None, None

    def __init__(self, args: argparse.Namespace, nfsdb: libcas.CASDatabase, config, ft_db: Optional[FTDatabase] = None) -> None:
        super().__init__(args, nfsdb, config, ft_db)
        self.data: List[libetrace.nfsdbEntryOpenfile] = []
        self.metaname: str = self.args.metaname if self.args.metaname else "cas_project"
        self.compile_commands: List[Dict] = []
        self.rcomps: Dict[str, Set] = {}
        self.defines: Dict[str, Dict[str, List]] = {"gcc": {}, "g++": {}, "others": {}}
        self.compilers: Dict[str, Dict[str, List]] = {}
        self.global_defines: List = []
        self.source_root = os.path.abspath(self.nfsdb.source_root)
        self.real_src_root = os.path.abspath(os.path.expanduser(os.path.expandvars(self.args.remap_source_root))) if self.args.remap_source_root else self.source_root
        if not self.real_src_root.endswith("/"):
            self.real_src_root += "/"
        project_dir_name = self.args.output_dir if self.args.output_dir else self.args.metaname if self.args.metaname else "cas_project"
        zip_name = self.args.output_zip if self.args.output_zip else self.args.metaname + ".zip" if self.args.metaname else "cas_project.zip"
        if self.args.is_server or self.args.archive:
            self.gen_dir = os.path.join("/tmp/",str(uuid.uuid4()))
            self.output_dir = os.path.join(self.gen_dir, project_dir_name)
            self.output_zip = os.path.join(self.gen_dir, zip_name)
        else:
            self.output_dir = os.path.abspath(os.path.expanduser(os.path.expandvars(project_dir_name)))
            self.output_zip = os.path.abspath(os.path.expanduser(os.path.expandvars(zip_name)))
        if not self.source_root.endswith("/"):
            self.source_root += "/"
        self.workspace_file = "unknown"
        self.exclude_pattern = []
        if self.args.skip_asm:
            self.exclude_pattern.append(".*[-/]asm[-/].*")
        if self.args.skip_pattern:
            self.exclude_pattern.append(self.args.skip_pattern)
        if self.args.skip_wildcard:
            for x in self.args.skip_wildcard.split(":"):
                self.exclude_pattern.append(fnmatch.translate(x))
        if len(self.exclude_pattern) > 0:
            printdbg(self.exclude_pattern, self.args)
            printdbg("|".join(self.exclude_pattern), self.args)
            self.pattern = re.compile("|".join(self.exclude_pattern))
        else:
            self.pattern = None
        self.workspace_file = f"{self.metaname}.code-workspace"

    @staticmethod
    def get_src_type(src_type: int):
        return {1: "gcc", 2: "g++", 3: "other"}[src_type]

    def add_cc(self, exe: libetrace.nfsdbEntry):
        if exe.compilation_info:
            for ci in exe.compilation_info.file_paths:
                if self.args.is_server or self.args.archive:
                    self.compile_commands.append({"file": ci.replace(self.source_root, '${workspaceFolder}'),
                                                  "arguments": [arg.replace(self.source_root, '${workspaceFolder}') for arg in exe.argv],
                                                  "directory": exe.cwd.replace(self.source_root, '${workspaceFolder}')})                    
                else:
                    self.compile_commands.append({"file": ci.replace(self.source_root, self.output_dir + "/"),
                                                  "arguments": [arg.replace(self.source_root, self.output_dir + "/") for arg in exe.argv],
                                                  "directory": exe.cwd.replace(self.source_root, self.output_dir + "/")})

    def get_effective_path(self, path: str) -> str:
        return os.path.normpath(path.replace(self.source_root, self.real_src_root) if self.real_src_root else path)

    def filter_rcomps(self, f:str):
        return f not in self.nfsdb.linked_module_paths() and f not in self.nfsdb.object_files_paths() and \
            not f.endswith(".o.d") and not f.endswith(".o.tmp")

    def get_files(self):
        unique_paths: Set[str] = set()
        symlinks: Dict[str, Set] = {}
        execs: Set[libetrace.nfsdbEntry] = set()

        for opn in progressbar(self.data, disable=self.args.debug or self.args.is_server):
            if opn.opaque and opn.opaque.compilation_info:
                execs.add(opn.opaque)
                for f in opn.opaque.openpaths_with_children:
                    if self.filter_rcomps(f):
                        no_sr_f = f.replace(self.source_root, "")
                        for cf in opn.opaque.compilation_info.file_paths:
                            no_sr_cf = cf.replace(self.source_root, "")
                            if no_sr_f != no_sr_cf:
                                if f.startswith(self.source_root):
                                    if no_sr_f not in self.rcomps:
                                        self.rcomps[no_sr_f] = {no_sr_cf}
                                    else:
                                        self.rcomps[no_sr_f].add(no_sr_cf)

            symlink = os.path.normpath(opn.original_path)
            target = os.path.normpath(opn.path)
            if opn.is_symlink():
                if target.startswith(self.source_root) and symlink.startswith(self.source_root):
                    if not self.is_excluded(os.path.normpath(symlink)):
                        if symlink not in symlinks:
                            symlinks[symlink] = {target}
                        else:
                            symlinks[symlink].add(target)
                        unique_paths.add(target)
                        printdbg(f"Symlink {symlink} -> { target}", self.args)
            else:
                if target.startswith(self.source_root) and opn.exists():
                    unique_paths.add(target)
        try:
            for ex in execs:
                self.add_cc(ex)
        except FileNotFoundError as err:
            raise IDEGenerationException(f"Compiler executable not found: {err.filename}") from err

        unique_paths = {u for u in unique_paths if u not in symlinks.keys()}
        unique_paths = self.exclude_paths(unique_paths)
        if not self.args.is_server:
            print()
            print(f"Unique paths:                       {len(unique_paths)}")
            print(f"Unique paths without symlink paths: {len(unique_paths)}")
            print(f"Unique paths after filtering:       {len(unique_paths)}")
            print(f"Symlinks:                           {len(symlinks.keys())}")
            print(f"Compile commands :                  {len(self.compile_commands)}")
        return unique_paths, symlinks

    def is_excluded(self, path: str):
        if not path.startswith(self.source_root):
            return True
        if self.args.skip_linked:
            if path in self.nfsdb.linked_module_paths():
                return True
        if self.args.skip_objects:
            objects_paths = set(self.nfsdb.object_files_paths())
            if path in objects_paths:
                return True
        if self.pattern is not None:
            return self.pattern.match(path)
        return False

    def exclude_paths(self, paths):
        unique_paths = paths
        if self.args.skip_linked:
            unique_paths = unique_paths - set(self.nfsdb.linked_module_paths())
        if self.args.skip_objects:
            objects_paths = set(self.nfsdb.object_files_paths())
            unique_paths = unique_paths - objects_paths
        if self.pattern is not None:
            unique_paths = {up for up in unique_paths if not self.pattern.match(up)}
        return unique_paths

    def write_file(self, arch_name: str, contents: str):
        os.makedirs(os.path.join(self.output_dir, os.path.dirname(arch_name)), exist_ok=True)
        with open(os.path.join(self.output_dir, arch_name), "w", encoding=sys.getdefaultencoding()) as of:
            of.write(contents)
            printdbg(f"Written '{os.path.join(self.output_dir, arch_name)}'", self.args)

    def add_files(self, files):
        if os.path.exists(self.output_dir):
            printcli("WARNING: Output dir exists, consider deleting it first.", self.args)
        else:
            os.makedirs(self.output_dir, exist_ok=True)

        for fp in progressbar(files, disable=self.args.debug or self.args.is_server):
            fp = self.get_effective_path(fp)
            output_file = fp.replace(self.real_src_root, '')
            abs_output_file = os.path.join(self.output_dir, output_file)
            try:
                os.makedirs(os.path.dirname(abs_output_file), exist_ok=True)
                if os.path.islink(fp):
                    print("WARNING: Tried to copy symlink ")
                    continue
                if os.path.isdir(fp):
                    # Ignore directories
                    continue
                # if os.path.exists(abs_output_file):
                    # print(f"Removing {abs_output_file}")
                    # os.unlink(abs_output_file)

                if self.args.copy or self.args.archive or self.args.is_server:
                    printdbg(f"Copying {fp} to {abs_output_file}", self.args)
                    shutil.copy(fp, abs_output_file, follow_symlinks=False)
                else:
                    printdbg(f"Linking {fp} to {abs_output_file}", self.args)
                    try:
                        os.symlink(src=fp, dst=abs_output_file)
                    except FileExistsError:
                        try:
                            os.unlink(abs_output_file)
                            os.symlink(src=fp, dst=abs_output_file)
                        except IsADirectoryError:
                            pass
            except FileNotFoundError as err:
                raise IDEGenerationException(str(err)) from err

    def add_symlinks(self, symlinks: Dict[str, Set]):
        for symlink, target in progressbar(symlinks.items(), disable=self.args.debug or self.args.is_server):
            if len(target) > 1:
                raise IDEGenerationException("Multiple symlink targets!!!")
            target = target.pop()
            real_symlink = symlink.replace(self.source_root, self.real_src_root) if self.real_src_root else symlink
            real_target = target.replace(self.source_root, self.real_src_root) if self.real_src_root else target
            relat_symlink = symlink.replace(self.source_root, '')
            relat_target = target.replace(self.source_root, '')
            output_symlink = os.path.join(self.output_dir, relat_symlink)
            output_target = os.path.join(self.output_dir, relat_target)
            if self.args.debug:
                print("real_symlink   : " + real_symlink)
                print("real_target    : " + real_target)
                print("relat_symlink  : " + relat_symlink)
                print("relat_target   : " + relat_target)
                print("output_symlink : " + output_symlink)
                print("output_target  : " + output_target)
            if not os.path.exists(output_target):
                printcli(f"WARNING: symlink target not present in output dir {output_target}", self.args)
            if os.path.exists(output_target):
                os.makedirs(os.path.dirname(output_target), exist_ok=True)
                shutil.copy(real_target, output_target)
            symlink_rel_target = os.path.relpath(output_target, os.path.dirname(output_symlink))

            if os.readlink(real_symlink) != symlink_rel_target:
                printcli("WARNING: Symlink mismatch", self.args)
            if not os.path.exists(real_target):
                printcli(f"WARNING: symlink target not present in source root {real_target}", self.args)
            if not os.path.exists(output_target):
                printcli(f"WARNING: symlink target not present in output dir {output_target}", self.args)
            if os.path.exists(output_symlink):
                printcli(f"WARNING: symlink already exists! {output_symlink}", self.args)
            try:
                if not os.path.exists(output_symlink):
                    os.makedirs(os.path.dirname(output_symlink), exist_ok=True)
                    os.symlink(src=symlink_rel_target, dst=output_symlink)
                printdbg(f"LINK {output_symlink} > points to > {os.readlink(output_symlink)} \n", self.args)
            except Exception as err:
                printerr(f"ERROR: {err}")
            assert os.path.normpath(os.readlink(output_symlink)) == os.path.normpath(symlink_rel_target)

    def get_template_sr(self) -> Dict:
        return {
            "local": {
                    "type": "local",
                    "source_root": self.source_root
            },
            "p4": {
                "server": "",
                "syncMap": [
                    {
                        "clSync": 0,
                        "clPartial": [],
                        "syncMapping": {},
                        "rootPath": ""
                    }
                ],
                "cleanSourceList": "",
                "intermediateArchive": "",
            },
            "repo": {
                "server": "<string/url>",
                "repoBranch": "<string>",
                "sourceGitMapping": {"<repoMapping>"},
                "intermediateArchive": "<string/url>"
            },
            "package": {
                "sourceArchive": "",
                "intermediateArchive": ""
            }
        }[self.args.type]

    def get_template_bas_prov(self) -> Dict:
        nfsdb_path = '${workspaceFolder}/.nfsdb.img' if self.args.add_db or self.args.is_server else self.nfsdb.db_path
        return {
                    "path": self.args.remote_cas,
                    "scheme": "url"
                } if self.args.remote_cas else {
                    "path": nfsdb_path,
                    "scheme": "bas_file"
                }

    def add_workspace(self):
        printcli(f"Writing {self.metaname}.code-workspace", self.args)
        ws = {
            "folders": [
                {"path": "./"}
            ],
            "settings": {
                "cas.basDatabase": self.get_template_bas_prov(),
                "cas.basDatabases": [
                    self.get_template_bas_prov()
                ],
                "cas.ftdbDatabase": None,
                "cas.ftdbDatabases": [],
                "cas.baseDir": self.source_root,
                "cas.genCmd": " ".join(self.args.cmd[:self.args.cmd.index("vscode")]),
                "cas.projectName": self.metaname,
                "cas.depsFolders": [ "${workspaceFolder}" ],
                "C_Cpp.default.compileCommands": "${workspaceFolder}/compile_commands.json",
                "C_Cpp.default.includePath": [ "${workspaceFolder}" ],
                "C_Cpp.intelliSenseCachePath": "${workspaceFolder}/.cache/"
            }
        }
        self.write_file(f"{self.metaname}.code-workspace", json.dumps(ws, indent=4))

    def add_deps_file(self):
        printcli("Writing deps.json", self.args)
        out = {
            "count": len(self.data),
            "page": 0,
            "page_max": 0,
            "entries_per_page": 0,
            "num_entries": len(self.data),
            "entries": [
                self.gen_dep_record(o) for o in self.data
            ]
        }
        self.write_file("deps.json", json.dumps(out, indent=4))

    def add_rcomp_file(self):
        printcli("Writing rcm.json", self.args)
        def serialize_set(obj):
            if isinstance(obj, set):
                return list(obj)
        self.write_file("rcm.json", json.dumps(self.rcomps, indent=4, default=serialize_set))

    def gen_dep_record(self, o: libetrace.nfsdbEntryOpenfile):
        fp = self.get_effective_path(o.path)
        output_file = fp.replace(self.real_src_root, '')
        abs_output_file = os.path.join(self.output_dir, output_file)
        return {
                        "filename": fp.replace(self.source_root, ""),
                        "original_path": abs_output_file,
                        "open_timestamp": o.open_timestamp,
                        "close_timestamp": o.close_timestamp,
                        "ppid": o.parent.eid.pid,
                        "type": "linked" if o.opaque and o.opaque.is_linking() and o.opaque.linked_path == o.path else "compiled" if o.opaque and o.opaque.compilation_info and o.path in o.opaque.compilation_info.file_paths else "plain",
                        "access": access_from_code(get_file_info(o.mode)[2]),
                        "exists": 1 if o.exists() else 0,
                        "link": 1 if o.is_symlink() else 0  
                    }

    def write_modules(self):
        printcli("Writing modules.json", self.args)
        if self.module_pipeline:
            sub_pipeline = []
            for i, mod in enumerate(self.module_pipeline.modules):
                if isinstance(mod, LinkedModules):
                    sub_pipeline = self.module_pipeline.modules[:i+1]
            pipeline_output = ModulePipeline(sub_pipeline).render()
            if isinstance(pipeline_output, Iterator) or isinstance(pipeline_output, Generator):
                pipeline_output = list(pipeline_output)
            if isinstance(pipeline_output, List):
                if len(pipeline_output) > 0:
                    self.write_file("modules.json", json.dumps(pipeline_output, indent=4))

    def generate(self):
        if not os.path.exists(self.source_root) and not self.args.remap_source_root:
            printcli("WARNING: Source root not found. If sources was moved use --remap-source-root argument to set source location.", self.args)
        if self.args.remap_source_root:
            printcli(f"INFO: Remapping source root from {self.source_root} to {self.args.remap_source_root}", self.args)

        if len(self.data) > 0:
            if isinstance(self.data, libetrace.nfsdbOpensIter) or isinstance(self.data[0], libetrace.nfsdbEntryOpenfile):
                self.data = [d for d in self.data if not self.is_excluded(os.path.normpath(d.path))]
            else:
                raise PipelineException("Returned data incompatible. Use --details argument.")

            printcli("-- Generating IDE project --", self.args)
            printcli("\n1/4 - Analyzing input opens", self.args)
            files, symlinks = self.get_files()

            printcli("\n2/4 - Adding files", self.args)
            self.add_files(files)

            printcli("\n3/4 - Adding symlinks", self.args)
            if len(symlinks) > 0:
                self.add_symlinks(symlinks)

            printcli("\n4/4 - Adding workspace files", self.args)
            self.write_modules()
            if not self.args.is_server:
                self.add_deps_file()
                self.add_rcomp_file()
            self.add_workspace()

            if len(self.compile_commands) > 0:
                printcli(f"Writing compile_commands.json", self.args)
                self.write_file("compile_commands.json", json.dumps(self.compile_commands, indent=4))
            else:
                printcli("WARNING: Compile commands not found!", self.args)

            if self.args.add_db or self.args.is_server:
                printcli("Adding CAS database files:", self.args)
                if self.nfsdb.db_path:
                    nfsdb_copy_path = os.path.join(self.output_dir, os.path.basename(self.nfsdb.db_path))
                    printcli((" - " + nfsdb_copy_path) , self.args)
                    shutil.copy(self.nfsdb.db_path, nfsdb_copy_path, follow_symlinks=False)
                if self.nfsdb.cache_db_path:
                    nfsdb_cache_copy_path = os.path.join(self.output_dir, os.path.basename(self.nfsdb.cache_db_path))
                    printcli((" - " + nfsdb_cache_copy_path) , self.args)
                    shutil.copy(self.nfsdb.cache_db_path, nfsdb_cache_copy_path, follow_symlinks=False)
                if self.nfsdb.config.config_file:
                    nfsdb_config_copy_path = os.path.join(self.output_dir, os.path.basename(self.nfsdb.config.config_file))
                    printcli((" - " + nfsdb_config_copy_path) , self.args)
                    shutil.copy(self.nfsdb.config.config_file, nfsdb_config_copy_path, follow_symlinks=False)

            if self.args.archive or self.args.is_server:
                if os.path.exists(self.output_dir):
                    output_dir = Path(self.output_dir)
                    printcli(f"\nZipping '{self.output_dir}' to '{self.output_zip}'",self.args, end=" ... ")
                    try:
                        with zipfile.ZipFile(self.output_zip, "w", zipfile.ZIP_DEFLATED) as zip_file:
                            for fl in output_dir.rglob("*"):
                                zip_file.write(fl, fl.relative_to(os.path.dirname(output_dir)))
                    except FileNotFoundError as err:
                        raise IDEGenerationException(str(err)) from err
                    printcli("- Done" , self.args)
                    if self.args.is_server:
                        return send_file(self.output_zip,
                            mimetype='application/zip',
                            download_name=os.path.basename(self.output_zip))

                else:
                    printcli("ERROR: Output dir is empty!" , self.args)

            printcli("\nProject generation - Done" , self.args)
            if self.args.archive:
                printcli(f"Check '{self.output_zip}'" , self.args)
            else:
                printcli(f"Check '{self.output_dir}'" , self.args)
        else:
            raise IDEGenerationException("empty file list")
