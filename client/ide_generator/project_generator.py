from abc import abstractmethod
import re
import sys
import shutil
import os
import io
import json
import zipfile
import time
import subprocess

from typing import List, Set, Dict
import libetrace
from libcas import CASDatabase, progressbar
from .generator_eclipse import ProjectFiles


def add_params(parser):
    ide_group = parser.add_argument_group("IDE project generation arguments")

    ide_group.add_argument('--ide', '-g', type=str, default=None, choices=['eclipse', 'vscode', 'plain', 'all'], help='Generate IDE project based on results.')
    ide_group.add_argument('--metaname', type=str, default="cas_project", help='Project name')
    ide_group.add_argument('--output-dir', type=str, default="cas_project", help='Output directory')
    ide_group.add_argument('--output-zip', type=str, default=None, help='Output zip file')
    ide_group.add_argument('--remap-source-root', type=str, default=None, help='Change original source root')
    ide_group.add_argument('--include-source', action='store_true', default=True, help='Add source files to project file')
    ide_group.add_argument('--relative-cc', action='store_true', default=False, help='Store relative paths in compile commands')

    ide_group.add_argument('--skip-asm', action='store_true', default=False, help='Disable asm files')
    ide_group.add_argument('--skip-linked', action='store_true', default=False, help="Don't copy linked files to project")
    ide_group.add_argument('--skip-objects', action='store_true', default=False, help="Don't copy object files to project")
    ide_group.add_argument('--skip-pattern', type=str, default=None, help='Skip copy files matching pattern to project')
    ide_group.add_argument('--skip-wildcard', type=str, default=None, help='Skip copy files matching wildcard(s) to project')

    ide_group.add_argument('--def', dest="defs",  type=str, action='append', default=None, help='add  global defines')


class ProjectGenerator:
    def __init__(self, args, cas_db: CASDatabase) -> None:
        self.args = args
        self.db = cas_db
        self.data: List[libetrace.nfsdbEntryOpenfile] = []
        self.metaname: str = self.args.metaname
        self.compile_commands: List[Dict] = []
        self.defines: Dict[str, Dict[str, List]] = {"gcc": {}, "g++": {}, "others": {}}
        self.compilers: Dict[str, Dict[str, List]] = {}
        self.global_defines: List = []
        self.source_root = os.path.abspath(self.db.source_root)
        self.real_src_root = os.path.abspath(os.path.expanduser(os.path.expandvars(self.args.remap_source_root))) if self.args.remap_source_root else self.source_root
        if not self.real_src_root.endswith("/"):
            self.real_src_root += "/"
        self.output_dir = os.path.abspath(os.path.expanduser(os.path.expandvars(self.args.output_dir)))
        self.output_zip = os.path.abspath(os.path.expanduser(os.path.expandvars(self.args.output_zip))) if self.args.output_zip else None
        if not self.source_root.endswith("/"):
            self.source_root += "/"
        self.workspace_file = "unknown"
        self.exclude_pattern = []
        if self.args.skip_asm:
            self.exclude_pattern.append(".*[-/]asm[-/].*")
        if self.args.skip_pattern:
            self.exclude_pattern.append(self.args.skip_pattern)
        if self.args.skip_wildcard:
            import fnmatch
            for x in self.args.skip_wildcard.split(":"):
                self.exclude_pattern.append(fnmatch.translate(x))
        if len(self.exclude_pattern) > 0:
            if self.args.debug:
                print(self.exclude_pattern)
                print("|".join(self.exclude_pattern))
            self.pattern = re.compile("|".join(self.exclude_pattern))
        else:
            self.pattern = None
    
    @staticmethod
    def get_src_type(src_type: int):
        return {1: "gcc", 2: "g++", 3: "other"}[src_type]

    def add_cc(self, exe: libetrace.nfsdbEntry):
        if exe.compilation_info:
            binary = self.get_effective_path(exe.binary)
            if binary not in self.compilers:
                argv = [binary]
                for xv in exe.argv:
                    if xv.startswith("--target="):
                        argv.append(xv)
                        break
                argv.append("-v")
                argv.append("-P")
                argv.append("-E")
                argv.append("-dD")
                argv.append("-x")
                argv.append("c")
                argv.append("-")

                cdfL = list()
                pn = subprocess.Popen(argv, shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                out, err = pn.communicate()

                if out is not None and err is None:
                    for dv in str(out).split('\\n'):
                        if dv.startswith('#define'):
                            vn = dv[7:].strip().split()[0]
                            vv = dv[7:].strip()[len(vn):]
                            cdfL.append((vn, vv))

                argv = [binary]
                for xv in exe.argv:
                    if xv.startswith("--target="):
                        argv.append(xv)
                        break
                argv.append("-v")
                argv.append("-P")
                argv.append("-E")
                argv.append("-dD")
                argv.append("-x")
                argv.append("c++")
                argv.append("-")

                cppdfL = list()
                pn = subprocess.Popen(argv, shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                out, err = pn.communicate()

                if out is not None and err is None:
                    for dv in str(out).split('\\n'):
                        if dv.startswith('#define'):
                            vn = dv[7:].strip().split()[0]
                            vv = dv[7:].strip()[len(vn):]
                            cppdfL.append((vn, vv))

                self.compilers[binary] = {
                    "gcc": cdfL,
                    "g++": cppdfL
                }

            for ci in exe.compilation_info.file_paths:
                if self.args.relative_cc:
                    self.compile_commands.append({"file": ci.replace(self.source_root, './'),
                                                  "arguments": [arg.replace(self.source_root, './') for arg in exe.argv],
                                                  "directory": exe.cwd.replace(self.source_root, './')})
                else:
                    self.compile_commands.append({"file": ci.replace(self.source_root, self.output_dir+"/"),
                                                  "arguments": [arg.replace(self.source_root, self.output_dir+"/") for arg in exe.argv],
                                                  "directory": exe.cwd.replace(self.source_root, self.output_dir+"/")})
                self.defines[self.get_src_type(exe.compilation_info.type)][ci] = self.compilers[binary][self.get_src_type(exe.compilation_info.type)]

    def get_effective_path(self, path: str) -> str:
        return os.path.normpath(path.replace(self.source_root, self.real_src_root) if self.real_src_root else path)

    def add_global_defines(self):
        self.global_defines.append(["__BAS__IDE_PROJECT_GENERATOR__", ""])

        if self.args.defs:
            for ds in self.args.defs:
                d = ds.split(":", 1)
                self.global_defines.append([d[0], d[1]])

        if self.args.debug:
            print(f"Global defines: {self.global_defines}")

    def get_files(self):
        unique_paths: Set[str] = set()
        symlinks: Dict[str, Set] = {}
        execs: Set[libetrace.nfsdbEntry] = set()
        self.add_global_defines()

        for opn in progressbar(self.data, disable=self.args.debug):
            if opn.opaque and opn.opaque.compilation_info:
                execs.add(opn.opaque)

                if opn.parent.is_wrapped():
                    for wrapped_exe in self.db.get_entries_with_pid(opn.parent.wpid):
                        for child_open in wrapped_exe.opens_with_children:
                            if child_open.path.startswith(self.source_root):
                                unique_paths.add(child_open.path)
                else:
                    for child_open in opn.parent.opens_with_children:
                        if child_open.path.startswith(self.source_root):
                            unique_paths.add(child_open.path)
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
                        if self.args.debug:
                            print(f"Symlink {symlink} -> { target}")
            else:
                if target.startswith(self.source_root) and opn.exists():
                    unique_paths.add(target)

        for ex in execs:
            self.add_cc(ex)

        print(f"\nUnique paths : {len(unique_paths)}")
        unique_paths = {u for u in unique_paths if u not in symlinks.keys()}
        print(f"Unique paths without symlink paths: {len(unique_paths)}")
        unique_paths = self.exclude_paths(unique_paths)
        print(f"Unique paths after filtering: {len(unique_paths)}")
        print(f"Symlinks: {len(symlinks.keys())}")
        print(f"Compile commands : {len(self.compile_commands)}")
        return unique_paths, symlinks

    def is_excluded(self, path):
        if self.args.skip_linked:
            linked_paths = set(self.db.linked_module_paths())
            if path in linked_paths:
                return True
        if self.args.skip_objects:
            objects_paths  = set(self.db.object_files_paths())
            if path in objects_paths:
                return True
        if self.pattern is not None:
            return self.pattern.match(path)
        return True

    def exclude_paths(self, paths):
        unique_paths = paths
        if self.args.skip_linked:
            linked_paths = set(self.db.linked_module_paths())
            unique_paths = unique_paths - linked_paths
        if self.args.skip_objects:
            objects_paths = set(self.db.object_files_paths())
            unique_paths = unique_paths - objects_paths
        if self.pattern is not None:
            unique_paths = {up for up in unique_paths if not self.pattern.match(up)}
        return unique_paths

    def write_file(self, arch_name: str, contents: str):
        os.makedirs(os.path.join(self.output_dir, os.path.dirname(arch_name)), exist_ok=True)
        with open(os.path.join(self.output_dir, arch_name), "w") as of:
            of.write(contents)
            print(f"Written '{os.path.join(self.output_dir, arch_name)}'")

    def add_files(self, files):
        if os.path.exists(self.output_dir):
            print("WARNING: Output dir exists, consider deleting it first.")
        else:
            os.makedirs(self.output_dir, exist_ok=True)

        for fp in progressbar(files, disable=self.args.debug):
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
                shutil.copy(fp, abs_output_file, follow_symlinks=False)
            except FileNotFoundError:
                print(f"ERROR - can't find '{fp}'")

    def add_symlinks(self, symlinks:Dict[str,Set]):
        for symlink,target in progressbar(symlinks.items(), disable=self.args.debug):
            assert len(target) == 1 , "Multiple symlink targets!!!"
            target = target.pop()
            real_symlink = symlink.replace(self.source_root, self.real_src_root) if self.real_src_root else symlink
            real_target = target.replace(self.source_root, self.real_src_root) if self.real_src_root else target
            relat_symlink = symlink.replace(self.source_root, '')
            relat_target = target.replace(self.source_root, '')
            output_symlink = os.path.join(self.output_dir, relat_symlink)
            output_target = os.path.join(self.output_dir, relat_target)
            if self.args.debug:
                print ("real_symlink   : " + real_symlink)
                print ("real_target    : " + real_target)
                print ("relat_symlink  : " + relat_symlink)
                print ("relat_target   : " + relat_target)
                print ("output_symlink : " + output_symlink)
                print ("output_target  : " + output_target)
            if not os.path.exists(output_target):
                print(f"WARNING: symlink target not present in output dir {output_target}")
            if os.path.exists(output_target):
                os.makedirs(os.path.dirname(output_target), exist_ok=True)
                shutil.copy(real_target, output_target)
            symlink_rel_target = os.path.relpath(output_target, os.path.dirname(output_symlink))

            if  os.readlink(real_symlink) != symlink_rel_target :
                print("WARNING: Symlink mismatch")
            if not os.path.exists(real_target):
                print(f"WARNING: symlink target not present in source root {real_target}")
            if not os.path.exists(output_target):
                print(f"WARNING: symlink target not present in output dir {output_target}")
            if os.path.exists(output_symlink):
                print(f"WARNING: symlink already exists! {output_symlink}")
            try:
                if not os.path.exists(output_symlink):
                    os.makedirs(os.path.dirname(output_symlink), exist_ok=True)
                    os.symlink(dst=output_symlink, src=symlink_rel_target)
                if self.args.debug:
                    print(f"LINK {output_symlink} > points to > {os.readlink(output_symlink)} \n")
            except Exception as err:
                print(f"ERROR: {err}")
            assert os.path.normpath(os.readlink(output_symlink)) == os.path.normpath(symlink_rel_target)

    @abstractmethod
    def add_workspace(self, files, symlinks):
        pass

    def generate(self, data):
        if len(data) > 0:
            if isinstance(data[0], libetrace.nfsdbEntry):
                self.data = [c for exe in data for c in exe.compilation_info.files]
            elif isinstance(data[0], libetrace.nfsdbEntryOpenfile):
                self.data = data

        if not os.path.exists(self.source_root) and not self.args.remap_source_root:
            print("WARNING: Source root not found. If sources was moved use --remap-source-root argument to set source location.")
        if self.args.remap_source_root:
            print(f"INFO: Remapping source root from {self.source_root} to {self.args.remap_source_root}")

        if len(self.data) > 0:
            if not isinstance(self.data[0], libetrace.nfsdbEntryOpenfile) and not isinstance(self.data[0], libetrace.nfsdbEntry):
                print("ERROR: returned data incompatible. Use --details or --commands argument.")
                return 1

            print("-- Generating IDE project --")
            print("\n1/5 - Analyzing input opens")
            files, symlinks = self.get_files()
            print("\n2/5 - Adding files")
            if self.args.include_source:
                self.add_files(files)
            else:
                print ("Skipped including source")
            print("\n3/5 - Adding symlinks")
            if self.args.include_source:
                if len(symlinks) > 0:
                    self.add_symlinks(symlinks)
            else:
                print ("Skipped including source")
            
            print("\n4/5 - Adding workspace files")
            self.add_workspace(files, symlinks)
            
            print("\n5/5 - Writing compile_commands.json")
            if len(self.compile_commands) > 0:
                self.write_file("compile_commands.json", json.dumps(self.compile_commands, indent=4))
            else:
                print("WARNING:  no compile commands found!")
            if self.output_zip:
                if os.path.exists(self.output_dir):
                    from pathlib import Path
                    output_dir = Path(self.output_dir)
                    print (f"\nZipping '{self.output_dir}' to '{self.output_zip}' ...")
                    with zipfile.ZipFile(self.output_zip, "w", zipfile.ZIP_DEFLATED) as zip_file:
                        for fl in output_dir.rglob("*"):
                            zip_file.write(fl, fl.relative_to(os.path.dirname(output_dir)))
                    print ("Zipping done")
                else:
                    print("ERROR: Output dir is empty!")
        print ("\nProject generation - Done")
        print (f"Check '{self.output_dir}'")


class EclipseProjectGenerator(ProjectGenerator):
    def __init__(self, args, cas_db: CASDatabase) -> None:
        super().__init__(args, cas_db)
        self.workspace_file = f"{args.metaname}.project"

    def gen_defines(self):
        from xml.sax.saxutils import quoteattr as escape
        ret = ""
        for lang_type in ["gcc",  "g++"]:
            resources = ""
            entries = ""
            for d in self.global_defines:
                entries += ProjectFiles.USER_LANG_SETTINGS_ENTRY.format(key=escape(d[0]) , val=escape(d[1]))
            resources += ProjectFiles.USER_LANG_RESOURCE.format(path="", entries=entries)

            for file, defines in self.defines[lang_type].items():
                entries = ""
                for d in defines:
                    entries += ProjectFiles.USER_LANG_SETTINGS_ENTRY.format(key=escape(d[0]) , val=escape(d[1]))
                resources += ProjectFiles.USER_LANG_RESOURCE.format(path=file.replace(self.source_root, ''), entries=entries)
            ret += ProjectFiles.USER_LANG_TYPE.format(resources=resources, c_cpp = lang_type)
        return ret

    def add_workspace(self, files, symlinks):

        timestamp = int(time.time())
        cproject = ProjectFiles.CPROJECT.format(metaname=self.args.metaname, timestamp=timestamp)
        project = ProjectFiles.PROJECT.format(metaname=self.args.metaname, timestamp=timestamp)
        language = ProjectFiles.LANGUAGE.format(metaname=self.args.metaname, timestamp=timestamp, user_settings=self.gen_defines())

        self.write_file(f".cproject", cproject)
        self.write_file(f".project", project)
        self.write_file(f".settings/language.settings.xml", language)


class VSCodeProjectGenerator(ProjectGenerator):
    config = {
        "settings": {
            "C_Cpp.default.compileCommands": "./compile_commands.json",
            "C_Cpp.intelliSenseCachePath": "./.cache",
            "C_Cpp.loggingLevel": "information",
            "files.exclude": {
                "**/.git": True,
                "**/.svn": True,
                "**/.hg": True,
                "**/CVS": True,
                "**/.DS_Store": True,
                "**/Thumbs.db": True,
                "**/.cache": True
            },
            "sqltools.telemetry": False,
            "telemetry.enableCrashReporter": False,
            "telemetry.enableTelemetry": False,
            "textmarker.enableTelemetry": False,
            "gitlens.telemetry.enabled": False,
            "telemetry.telemetryLevel": "off"
        },
        "folders": [
            {"path": "./"}
        ]
    }

    def __init__(self, args, cas_db: CASDatabase) -> None:
        super().__init__(args, cas_db)
        self.workspace_file = f"{args.metaname}.code-workspace"

    def add_workspace(self, files, symlinks):
        self.write_file(f"{self.args.metaname}.code-workspace", json.dumps(self.config, indent=4))


class AllProjectGenerator(ProjectGenerator):
    def __init__(self, args, cas_db: CASDatabase) -> None:
        super().__init__(args, cas_db)

    def add_workspace(self, files, symlinks):
        # VS
        self.write_file(f"{self.args.metaname}/{self.args.metaname}.code-workspace", json.dumps(VSCodeProjectGenerator.config, indent=4))

        # Eclipse
        cproject = ProjectFiles.CPROJECT.format(metaname=self.args.metaname)
        project = ProjectFiles.PROJECT.format(metaname=self.args.metaname)
        language = ProjectFiles.LANGUAGE

        self.write_file(f"{self.args.metaname}/.cproject", cproject)
        self.write_file(f"{self.args.metaname}/.project", project)
        self.write_file(f"{self.args.metaname}/.settings/language.settings.xml", language)


class PlainProjectGenerator(ProjectGenerator):
    def __init__(self, args, cas_db: CASDatabase) -> None:
        super().__init__(args, cas_db)

    def add_workspace(self):
        if len(self.compile_commands) > 0:
            self.write_file(f"{self.args.metaname}/compile_commands.json", json.dumps(self.compile_commands, indent=4))


project_generator_map = {
    "vscode": VSCodeProjectGenerator,
    "eclipse": EclipseProjectGenerator,
    "plain": PlainProjectGenerator,
    "all": AllProjectGenerator
}
