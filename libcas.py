"""
Module contains set of useful libetrace binding functions.
"""
from typing import List, Dict, Set, Tuple, Optional
import fnmatch
import multiprocessing
import json
import sys
import os
import time
import shlex
import re
import subprocess
from functools import lru_cache
import shutil
import psutil
import libetrace
from bas import gcc
from bas import clang
from bas import exec_worker

try:
    # If tqdm is available use it for nice progressbar
    from tqdm import tqdm as progressbar
except ModuleNotFoundError:
    class progressbar():
        n = 0
        x = None

        def __init__(self, x=None, total=0, disable=None):
            self.x = x
            self.total = total
            if disable == None:
                self.disable = not sys.stdout.isatty()

        def refresh(self):
            if not self.disable:
                print(f'\r\033[KProcessing {self.n} / {self.total}', end='')

        def close(self):
            pass

class DepsParam:
    """
    Extended parameter used in deps generation. Enables use per-path excludes.

    :param file: path to process
    :param direct: return only direct dependencies of this file
    :param exclude_cmd: list of commands to exclude while generating dependencies of this file
    :param exclude_pattern: list of file patterns to exclude while generating dependencies of this file
    :param negate_pattern: reversed pattern - exclude is included
    """

    def __init__(self, file, direct=None, exclude_cmd=None, exclude_pattern=None, negate_pattern=False):
        self.file = file
        self.direct = direct
        self.negate_pattern = negate_pattern
        if isinstance(exclude_cmd, list):
            self.exclude_cmd = exclude_cmd
        elif isinstance(exclude_cmd, str):
            self.exclude_cmd = [exclude_cmd]
        else:
            self.exclude_cmd = []

        if isinstance(exclude_pattern, list):
            self.exclude_pattern = exclude_pattern
        elif isinstance(exclude_pattern, str):
            self.exclude_pattern = [exclude_pattern]
        else:
            self.exclude_pattern = []


class CASConfig:
    def __init__(self, config_file):
        self.config_info = {}
        self.config_file = config_file
        self.linker_spec = []
        self.ld_spec = []
        self.ar_spec = []
        self.gcc_spec = []
        self.gpp_spec = []
        self.clang_spec = []
        self.clangpp_spec = []
        self.armcc_spec = []
        self.clang_tailopts = []
        self.dependency_exclude_patterns = []
        self.additional_module_exclude_patterns = []
        self.additional_module_exclude_pattern_variants = {}
        self.exclude_command_variants = {}
        self.exclude_command_variants_index = {}
        self.string_table = []
        self.integrated_clang_compilers = []
        self.rbm_wrapping_binaries = []
        self.paths = {}

        with open(self.config_file, "r", encoding=sys.getfilesystemencoding()) as fil:
            self.config_info = json.load(fil)

        for key, val in self.config_info.items():
            setattr(self, key, val)
        # Recompute `exclude_command_variants_index` based on `exclude_command_variants`
        pattern_map = {}
        for ptr in [v for cv in self.exclude_command_variants.values() for v in cv]:
            if ptr not in pattern_map:
                pattern_map[ptr] = len(pattern_map)

        for wildc, pattern_list in self.exclude_command_variants.items():
            self.exclude_command_variants_index[wildc] = [pattern_map[x] for x in pattern_list]

        self.string_table = [u[0] for u in sorted(pattern_map.items(), key=lambda x: x[1])]

    def apply_source_root(self, source_root=""):
        # If `integrated_clang_compilers` contains relative paths to compilers, normalize it to absolute paths
        for i, icc_path in enumerate(self.integrated_clang_compilers):
            if not os.path.isabs(icc_path):
                self.integrated_clang_compilers[i] = os.path.realpath(os.path.join(source_root, self.integrated_clang_compilers[i]))


def print_mem_usage():
    # Check psutil._pslinux.Process.memory_info() for details
    mi = psutil.Process().memory_info()

    def fmt(by):
        return f"{by / (1024 * 1024):.2f} MB"

    print(f"RAM || rss {fmt(mi.rss)} || vms {fmt(mi.vms)} || "
          f"shared {fmt(mi.shared)} || text {fmt(mi.text)} || "
          f"lib {fmt(mi.lib)} || data {fmt(mi.data)} "
          f"|| dirty {fmt(mi.dirty)}")


class CASDatabase:
    """
    Wrapping object for libetrace.nfsdb interface.
    Used for most database interaction.
    """

    db: libetrace.nfsdb
    """Main database object - direct wrapper to `libetrace.nfsdb`"""
    source_root: str
    """Directory where tracing process started execution"""
    db_loaded: bool
    """Database load status"""
    cache_db_loaded: bool
    """Cache database load status"""
    config: CASConfig
    """CASConfig object"""

    def __init__(self) -> None:
        self.db = libetrace.nfsdb()
        self.db_loaded = False
        self.cache_db_loaded = False

    def set_config(self, config: CASConfig):
        """
        Function used to assign CASConfig to database

        :param config: config object
        :type config: CASConfig
        """
        self.config = config

    def load_db(self, db_file, debug=False, quiet=True, mp_safe=True, no_map_memory=False):
        self.db_loaded = self.db.load(db_file, debug=debug, quiet=quiet, mp_safe=mp_safe, no_map_memory=no_map_memory)
        self.source_root = self.db.source_root
        assert self.config is not None, "Please set config first. Use CASDatabase.set_config()"
        self.config.apply_source_root(self.source_root)
        return self.db_loaded

    def load_deps_db(self, db_file, debug=False, quiet=True, mp_safe=True):
        self.cache_db_loaded = self.db.load_deps(db_file, debug=debug, quiet=quiet, mp_safe=mp_safe)
        return self.cache_db_loaded

    def linked_modules(self):
        return list({x[0] for x in self.db.linked_modules()})

    def get_pid(self, pid: int) -> List[libetrace.nfsdbEntry]:
        return self.db[tuple([pid])]

    def get_pids(self, pidlist: List[Tuple[int,Optional[int]]]) -> List[libetrace.nfsdbEntry]:
        return self.db[pidlist]

    def get_exec(self, pid: int, index: int) -> libetrace.nfsdbEntry:
        return self.db[(pid, index)][0]

    def get_exec_at_pos(self, pos: int) -> libetrace.nfsdbEntry:
        return self.db[pos]

    def get_eid(self, eid: tuple):
        return self.db[eid]

    def get_eids(self, eids: list):
        return self.db[eids]

    def opens_iter(self):
        return self.db.opens_iter()

    def opens_list(self):
        return self.db.opens_paths()

    def opens_num(self):
        return len(self.db.opens_paths())

    def execs_num(self):
        return len(self.db)

    def get_execs(self):
        return self.db

    def get_version(self):
        return self.db.dbversion

    def get_opens_of_path(self, filename: str):
        return self.db.filemap[filename]

    def get_execs_using_binary(self, binary: str):
        ret = []
        try:
            ret = self.db[binary]
        except:
            pass
        return ret

    def get_execs_filtered(self, **kwargs):
        return self.db.filtered(**kwargs)

    def get_compilations(self):
        return set(self.db.filtered(has_comp_info=True))

    def get_compilations_map(self) -> Dict[str, List[libetrace.nfsdbEntry]]:
        ret = {}
        for ent in self.db.filtered(has_comp_info=True):
            if ent.compilation_info.file_paths[0] not in ret:
                ret[ent.compilation_info.file_paths[0]] = []
            ret[ent.compilation_info.file_paths[0]].append(ent)
        return ret

    def get_linked_files(self):
        return {ent.linked_path for ent in self.db.filtered(has_linked_file=True)}

    def get_reverse_dependencies(self, file_paths: "str|list[str]", recursive=False):
        return list(self.db.rdeps(file_paths, recursive=recursive))

    def get_module_dependencies(self, module_path, direct=False):
        if isinstance(module_path, DepsParam):
            module_path = module_path.file
        return self.db.mdeps(module_path, direct=direct)

    def gen_excludes_for_path(self, path) -> tuple:
        excl_patterns = [x for x in self.config.dependency_exclude_patterns]
        excl_commands = []
        excl_commands_index = []

        for pattern_variant in self.config.additional_module_exclude_pattern_variants.keys():
            if fnmatch.fnmatch(path, pattern_variant):
                excl_patterns += self.config.additional_module_exclude_pattern_variants[pattern_variant]

        for command_variant in self.config.exclude_command_variants_index.keys():
            if fnmatch.fnmatch(path, command_variant):
                excl_commands_index += self.config.exclude_command_variants_index[command_variant]

        return excl_patterns, excl_commands, excl_commands_index

    def get_deps(self, epath, direct_global=False, dep_graph=False, debug=False, debug_fd=False, use_pipes=False,
        wrap_deps=False, all_modules=None):

        direct = direct_global

        if isinstance(epath, str):  # simple path - no excludes
            excl_patterns, excl_commands, excl_commands_index = self.gen_excludes_for_path(epath)
            if all_modules is not None:
                return self.db.fdeps(epath, debug=debug, debug_fd=debug_fd, use_pipes=use_pipes,
                                     wrap_deps=wrap_deps, direct=direct, dep_graph=dep_graph, exclude_patterns=excl_patterns,
                                     exclude_commands=excl_commands, exclude_commands_index=excl_commands_index, all_modules=all_modules)
            else:
                return self.db.fdeps(epath, debug=debug, debug_fd=debug_fd, use_pipes=use_pipes,
                                     wrap_deps=wrap_deps, direct=direct, dep_graph=dep_graph, exclude_patterns=excl_patterns,
                                     exclude_commands=excl_commands, exclude_commands_index=excl_commands_index)
        else:
            if epath.direct is not None:  # If extended path has direct it will overwrite global direct args
                direct = epath.direct

            excl_patterns, excl_commands, excl_commands_index = self.gen_excludes_for_path(epath.file)

            for e_c in epath.exclude_cmd:
                excl_commands.append(e_c)

            for e_p in epath.exclude_pattern:
                excl_patterns.append(e_p)
            if all_modules is not None:
                return self.db.fdeps(epath.file, debug=debug, debug_fd=debug_fd, use_pipes=use_pipes,
                    wrap_deps=wrap_deps, direct=direct, dep_graph=dep_graph, exclude_patterns=excl_patterns,
                    exclude_commands=excl_commands, negate_pattern=epath.negate_pattern,
                    exclude_commands_index=excl_commands_index, all_modules=all_modules)
            else:
                return self.db.fdeps(epath.file, debug=debug, debug_fd=debug_fd, use_pipes=use_pipes,
                                     wrap_deps=wrap_deps, direct=direct, dep_graph=dep_graph, exclude_patterns=excl_patterns,
                                     exclude_commands=excl_commands, negate_pattern=epath.negate_pattern,
                                     exclude_commands_index=excl_commands_index)

    def get_multi_deps_cached(self, epaths: list, direct_global=False):
        ret = []
        for epath in epaths:
            ret += [self.get_module_dependencies(epath, direct=direct_global)]
        return ret

    def get_multi_deps(self, epaths: list, direct_global=False, dep_graph=False, debug=False, debug_fd=False, use_pipes=False, wrap_deps=False):
        ret = []
        for epath in epaths:
            ret += [self.get_deps(epath, direct_global=direct_global, dep_graph=dep_graph, debug=debug, debug_fd=debug_fd, use_pipes=use_pipes, wrap_deps=wrap_deps)[2]]
        return ret

    def get_dependency_graph(self, epaths, direct_global=False, debug=False, debug_fd=False, use_pipes=False, wrap_deps=False):
        ret = {}

        def add_cmds_to_pids(val):
            for i, eid in enumerate(val[0]):
                val[0][i] = val[0][i] + ([" ".join(exe.argv) for exe in self.get_pid(eid[0]) if len(exe.argv) > 0],)
            return val

        for epath in epaths:
            deps = self.get_deps(epath, direct_global=direct_global, dep_graph=True, debug=debug, debug_fd=debug_fd, use_pipes=use_pipes, wrap_deps=wrap_deps)[3]
            ret.update({k: add_cmds_to_pids(v) for k, v in deps.items()})
        return [x for x in ret.items()]

    def get_rdm(self, file_paths, recursive=False, sort=False, reverse=False):
        ret = []
        for dep_path in file_paths:
            rdep = self.get_reverse_dependencies(dep_path, recursive)
            if sort:
                ret.append([dep_path, sorted(list(rdep), reverse=reverse)])
            else:
                ret.append([dep_path, list(rdep)])
        return ret

    def get_relative_path(self, file_path:str)-> str:
        """
        Function removes source path from given file path.

        :param file_path: file path
        :type file_path: str
        :return: file path without source root
        :rtype: str
        """
        return file_path.replace(self.source_root, "")

    def get_open_path(self, opn: libetrace.nfsdbEntryOpenfile, relative_path=False, original_path=False) -> str:
        """
        Function returns desired path from nfsdbEntryOpenfile object depending on original_path and relative_path parameters.

        :param opn: nfsdbEntryOpenfile object
        :type opn: libetrace.nfsdbEntryOpenfile
        :param relative_path: get only relative path
        :type relative_path: bool
        :param original_path: get original path
        :type original_path: bool
        :return: Desired file path
        :rtype: str
        """
        if relative_path:
            return self.get_relative_path(opn.original_path if original_path else opn.path)
        else:
            return opn.original_path if original_path else opn.path

    def get_cdm(self, file_paths, cdm_exclude_patterns=None, cdm_exclude_files=None, recursive=False, sort=False, reverse=False):

        ret = []
        for dep_path in file_paths:
            excl_patterns, excl_commands, excl_commands_index = self.gen_excludes_for_path(dep_path)

            if cdm_exclude_patterns is not None and isinstance(cdm_exclude_patterns, list):
                for ex in cdm_exclude_patterns:
                    excl_commands.append(ex)

            if cdm_exclude_files is not None and isinstance(cdm_exclude_files, list):
                for ex in cdm_exclude_files:
                    excl_patterns.append(ex)

            deps = self.db.fdeps(dep_path, exclude_patterns=excl_patterns,
                exclude_commands=excl_commands, exclude_commands_index=excl_commands_index,
                recursive=recursive)[2]
            comps = [
                d.path
                for d in deps
                if d.opaque is not None and d.opaque.compilation_info is not None
                ]
            if len(comps) > 0:
                if sort:
                    ret.append([dep_path, sorted(list(comps), reverse=reverse)])
                else:
                    ret.append([dep_path, list(comps)])
        return ret

    @staticmethod
    def parse_trace_to_json(tracer_db_filename, json_db_filename):
        """
        Wrapper function for `libetrace.parse_nfsdb` function that parses trace file into json file.

        :param tracer_db_filename: tracer file path
        :type tracer_db_filename: str
        :param json_db_filename: output json database file path
        :type json_db_filename: str
        """
        libetrace.parse_nfsdb(tracer_db_filename, json_db_filename)

    @staticmethod
    def get_src_root_from_tracefile(filename):
        """
        Function looks up for initial working directroy stored in first line of trace file.

        :param filename: tracer file path
        :type filename: str
        :return: tracer initial directory
        :rtype: str
        """
        if os.path.exists(filename):
            with open(filename, "r", encoding=sys.getfilesystemencoding()) as fil:
                line = fil.readline().strip()
            if line.startswith("INITCWD="):
                return line.strip().split("INITCWD=")[1]
            else:
                print("ERROR: source_root not found in {} tracer file!".format(filename))
                return None
        else:
            print("ERROR: tracer file {} not found!".format(filename))
            return None

    @staticmethod
    def create_db_image(json_db_filename:str, src_root:str, set_version:str, exclude_command_patterns:"list[str]", cache_db_filename:str, debug=False) -> bool:
        """
        Function creates cached database image from json database.

        :param json_db_filename: json database file path
        :type json_db_filename: str
        :param src_root: _description_
        :type src_root: _type_
        :param set_version: _description_
        :type set_version: _type_
        :param exclude_command_patterns: _description_
        :type exclude_command_patterns: _type_
        :param cache_db_filename: _description_
        :type cache_db_filename: _type_
        """
        with open(json_db_filename, "rb") as fil:
            if debug:
                print("Before load")
                print_mem_usage()
            json_db = json.load(fil)
            if len(json_db)>0:
                json_db[0]["r"]["p"] = 0 # fix parent process
            if debug:
                print("After load/ before create_nfsdb")
                print_mem_usage()
            r = libetrace.create_nfsdb(json_db, src_root, set_version, exclude_command_patterns, cache_db_filename)
            if debug:
                print("After create_nfsdb")
                print_mem_usage()
            return r

    def create_deps_db_image(self, deps_cache_db_filename, depmap_filename, ddepmap_filename, jobs=multiprocessing.cpu_count(),deps_threshold= 90000, debug=False):

        all_modules = sorted([e.linked_path for e in self.get_execs_filtered(has_linked_file=True) if not e.linked_path.startswith("/dev/")])

        print("Number of all linked modules: %d" % (len(all_modules)))

        if len(all_modules) == 0:
            print ("No linked modules found! Check linker patterns in config.")
            exit(0)

        depmap = {}

        def get_module_dependencies(module_path, depmap, done_modules, all_modules):
            linked_modules = [x[2] for x in depmap[module_path] if x[2] in all_modules]
            allm = set(linked_modules)
            for module_path in linked_modules:
                if module_path not in done_modules:
                    done_modules.add(module_path)
                    allm|=get_module_dependencies(module_path, depmap, done_modules, all_modules)
            return allm

        global calc_deps
        def calc_deps(module_path, direct=True, allm=all_modules):
            ret = self.get_deps(module_path, direct_global=direct, all_modules=allm)
            # print("[%d / %d] %s %s_deps(%d)" % (processed.value, len(allm), module_path, "direct" if direct is True else "full", len(ret[2])), flush=True)
            with processed.get_lock():
                processed.value += 1
                pbar.n = processed.value
                pbar.refresh()
            return module_path, [(x.ptr[0], x.ptr[1], x.path) for x in ret[2]]

        pbar = progressbar(total = len(all_modules), disable=None)
        processed = multiprocessing.Value('i', 0)
        processed.value = 0

        pool = multiprocessing.Pool(jobs)
        results = pool.map(calc_deps, all_modules)
        pool.close()
        pool.join()

        pbar.n = processed.value
        pbar.refresh()
        pbar.close()

        depmap = { d[0]:d[1] for d in results}
        depmap_keys = list(depmap.keys())
        del results
        deplst = { k:len(v) for k, v in depmap.items()}
        print("")
        print("# Sorted list of dependencies count")
        errcount = 0

        for m, q in sorted(deplst.items(), key=lambda item: item[1], reverse=True):
            print("  %s: %d" % (m, q))
            if deps_threshold and q > int(deps_threshold):
                print("ERROR: Number of direct dependencies [%d] for linked module [%s] exceeds the threshold [%d]" % (q, m, int(deps_threshold)))
                errcount+=1

        global get_full_deps
        def get_full_deps(lm):
            return calc_deps(lm, direct=False, allm=depmap_keys)

        if errcount > 0:
            print("ERROR: Exiting due dependency mismatch errors (%d errors)" % errcount)
            sys.exit(errcount)

        print("Saving {} ...".format(ddepmap_filename))
        with open(ddepmap_filename, "w", encoding=sys.getfilesystemencoding()) as f:
            json.dump(obj=depmap, fp=f, indent=4)
        if debug: print("DEBUG: written {}".format(ddepmap_filename))

        # Compute full dependency list for all modules

        full_depmap = {}
        print("")
        print("Computing full dependency list for all modules...")
        
        pbar = progressbar(total = len(all_modules), disable=None)
        processed = multiprocessing.Value('i', 1)
        processed.value = 0

        pool = multiprocessing.Pool(jobs)
        results = pool.map(get_full_deps, depmap)
        pool.close()
        pool.join()

        pbar.n = processed.value
        pbar.refresh()
        pbar.close()        

        full_depmap = {r[0]: r[1] for r in results}
        del results

        print("Saving {} ...".format(depmap_filename))
        with open(depmap_filename, "w", encoding=sys.getfilesystemencoding()) as f:
            json.dump(obj=full_depmap, fp=f, indent=4)
        if debug: print("DEBUG: written {}".format(depmap_filename))

        mismatch_list = list()
        for k, v in full_depmap.items():
            if deps_threshold and len(v) > int(deps_threshold):
                mismatch_list.append((k, len(v)))
        if len(mismatch_list) > 0:
            print("Number of modules with full dependency list that exceeds the predefined threshold [%d]: %d" % (int(deps_threshold), len(mismatch_list)))
            for m, n in mismatch_list:
                print("  %s: %d" % (m, n))
            print("Exiting due dependency mismatch errors (%d errors)" % (len(mismatch_list)))
            sys.exit(len(mismatch_list))

        print("Writing {} ".format(deps_cache_db_filename))
        assert self.db.create_deps_cache(full_depmap, depmap, deps_cache_db_filename, True)
        if debug: print("DEBUG: Written {} ".format(deps_cache_db_filename))

    @staticmethod
    @lru_cache(maxsize=1024)
    def is_integrated_clang_compiler(compiler_path, test_file):
        pn = subprocess.Popen(["%s" % compiler_path, "-c", "-Wall", "-o", "/dev/null", test_file, "-###"], shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        out, err = pn.communicate()
        return " (in-process)" in out.decode("utf-8")

    @staticmethod
    @lru_cache(maxsize=1024)
    def have_integrated_cc1(compiler_path, is_integrated, test_file) -> bool:
        return CASDatabase.is_integrated_clang_compiler(compiler_path, test_file) and is_integrated

    @staticmethod
    def clang_ir_generation(cmds):
        if "-x" in cmds:
            if cmds[cmds.index("-x")+1] == "ir":
                return True
        return False

    @staticmethod
    def clang_pp_input(cmds):
        if "-x" in cmds:
            if cmds[cmds.index("-x")+1] == "cpp-output":
                return True
        return False

    @staticmethod
    @lru_cache(maxsize=1024)
    def maybe_compiler_binary(binary_path_or_link):
        if binary_path_or_link.endswith("/cc") or binary_path_or_link.endswith("/c++"):
            return os.path.realpath(binary_path_or_link)
        else:
            return binary_path_or_link

    @staticmethod
    @lru_cache(maxsize=1024)
    def is_elf_executable(path):
        if os.path.exists(path):
            path = os.path.realpath(path)
            pn = subprocess.Popen(["file", path], shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            out, _ = pn.communicate()
            out_file = set([x.strip(",") for x in out.decode("utf-8").split()])
            return "ELF" in out_file and "executable" in out_file
        else:
            return False

    @staticmethod
    @lru_cache(maxsize=1024)
    def is_elf_interpreter(path):
        if os.path.exists(path):
            path = os.path.realpath(path)
            pn = subprocess.Popen(["file", path], shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            out, _ = pn.communicate()
            out_file = set([x.strip(",") for x in out.decode("utf-8").split()])
            return "ELF" in out_file and "interpreter" in out_file
        else:
            return False

    def post_process(self, json_db_filename, wrapping_bin_list:List[str], workdir,
        process_linking=True, process_comp=True, process_rbm=True, process_pcp=True,
        new_database=True, no_update=False, no_auto_detect_icc=False, max_chunk_size=sys.maxsize,
        allow_pp_in_compilations=False, jobs=multiprocessing.cpu_count(),
        debug_compilations=False, debug=False, verbose=False):



        start_time = time.time()
        total_start_time = time.time()

        interediate_db = json_db_filename.replace(".nfsdb.json", ".nfsdb.img.tmp")
        if not os.path.exists(interediate_db):
            print("NO intermediate database!")
            exit(2)
        self.load_db(interediate_db, debug=False, quiet=True, mp_safe=True, no_map_memory=False)
        if debug: print_mem_usage()
        no_module_specified = (not process_pcp and not process_linking and not process_comp and not process_rbm)
        do_rbm = process_rbm or no_module_specified
        do_pcp = process_pcp or no_module_specified
        do_linked = process_linking or no_module_specified
        do_compilations = process_comp or no_module_specified

        test_file = "/tmp/.nfsdb__test.c"
        if do_compilations:
            if not os.path.exists(test_file):
                with open(test_file, "w", encoding=sys.getfilesystemencoding()) as out_f:
                    out_f.write(";\n")

        start_time = time.time()
        wr_map:Dict[int,List[str]] = {}
        allow_llvm_bc = True
        gcc_comp_pids:List[int] = []
        gpp_comp_pids:List[int] = []
        integrated_clang_compilers:Set[str] = set()
        clang_execs:List[int] = []
        clangpp_execs:List[int] = []
        clang_pattern_match_execs:List[int] = []
        clangpp_pattern_match_execs:List[int] = []
        clangxx_input_execs = []
        gxx_input_execs = []
        comp_pids:Set[int] = set()
        gcc_spec_patterns = re.compile("|".join([fnmatch.translate(pattern) for pattern in self.config.gcc_spec]))
        gpp_spec_patterns = re.compile("|".join([fnmatch.translate(pattern) for pattern in self.config.gpp_spec]))
        clang_spec_patterns = re.compile("|".join([fnmatch.translate(pattern) for pattern in self.config.clang_spec]))
        clangpp_spec_patterns = re.compile("|".join([fnmatch.translate(pattern) for pattern in self.config.clangpp_spec]))
        cc1_patterns = re.compile("|".join([fnmatch.translate(pattern) for pattern in ["*cc1","*cc1plus"]]))
        comps_filename = os.path.join(workdir, ".nfsdb.comps.json")
        rbm_filename = os.path.join(workdir, ".nfsdb.rbm.json")
        pcp_filename = os.path.join(workdir, ".nfsdb.pcp.json")
        link_filename = os.path.join(workdir, ".nfsdb.link.json")
        manager = multiprocessing.Manager() 
        out_compilation_info = manager.dict() # we will need this for multiprocessing
        out_linked = dict()
        out_pcp_map = dict()
        out_rbm = dict()

        if do_rbm:
            if new_database or not os.path.exists(rbm_filename):
                start_time = time.time()
                def set_children_list(exe:libetrace.nfsdbEntry, ptree:Dict, ignore_binaries:Set[str]) -> None:
                    for chld in exe.childs:
                        if not ignore_binaries or not set([u.binary for u in self.get_pid(exe.eid.pid)]) & ignore_binaries:
                            set_children_list(chld, ptree, ignore_binaries)
                    assert exe.eid.pid not in ptree, "process tree have entry for pid {}".format(exe.eid.pid)
                    if len(exe.childs)>0:
                        ptree[exe.eid.pid] = [c.eid.pid for c in exe.childs]
                    else:
                        ptree[exe.eid.pid] = []

                def process_tree_for_pid(ex:libetrace.nfsdbEntry, ignore_binaries:Set[str]):
                    ptree = {}
                    set_children_list(ex, ptree, ignore_binaries)
                    return ptree

                def update_reverse_binary_mapping(reverse_binary_mapping:Dict[int,int], pid:int, ptree, root_pid, root_binary):
                    if pid in ptree:
                        for chld_pid in ptree[pid]:
                            if chld_pid in reverse_binary_mapping:
                                assert reverse_binary_mapping[chld_pid] == pid, "reverse binary mapping contains: {} (root: {})".format(chld_pid, root_pid)
                            else:
                                exelst = self.get_pid(chld_pid)
                                if root_binary not in set([u.binary for u in exelst]):
                                    reverse_binary_mapping[chld_pid] = root_pid
                                update_reverse_binary_mapping(reverse_binary_mapping, chld_pid, ptree, root_pid, root_binary)

                def build_reverse_binary_mapping(bin_lst:List[str]) -> "dict[int,int]":
                    reverse_binary_mapping:Dict[int,int] = dict()
                    for binary in bin_lst:
                        for bin_ex in self.get_execs_using_binary(binary):
                            ptree = process_tree_for_pid(bin_ex, set(bin_lst))
                            if ptree is not None:
                                update_reverse_binary_mapping(reverse_binary_mapping, bin_ex.eid.pid, ptree, bin_ex.eid.pid, binary)
                    return reverse_binary_mapping

                # { PID: wrapping_pid }
                out_rbm = build_reverse_binary_mapping(wrapping_bin_list)
                if len(out_rbm) > 0:
                    with open(rbm_filename, "w", encoding=sys.getfilesystemencoding()) as f:
                        f.write(json.dumps(out_rbm, indent=4))
                    if debug: print("DEBUG: written generated {}".format(rbm_filename))
                else:
                    print("WARNING: rbm map is empty! len==0")
                print("reverse binary mappings processed (%d binaries, %d entries)   [%.2fs]" % (len(wrapping_bin_list),len(out_rbm.keys()), time.time() - start_time))
                out_rbm = dict()

        if do_pcp:
            start_time = time.time()
            import base64
            if new_database or not os.path.exists(pcp_filename):
                os.system('setterm -cursor off')
                pcp_map = self.db.precompute_command_patterns(self.config.string_table)
                os.system('setterm -cursor on')
                if pcp_map:
                    for k, v in pcp_map.items():
                        l = self.get_eid((k[0],k[1]))
                        assert len(l) == 1
                    out_pcp_map = {str(self.get_exec(k[0], k[1]).ptr): base64.b64encode(v).decode("utf-8") for k, v in pcp_map.items()} # Can we use only other than "AA=="
                    del pcp_map
                else:
                    out_pcp_map = {}
                with open(pcp_filename, "w", encoding=sys.getfilesystemencoding()) as f:
                    f.write(json.dumps(out_pcp_map, indent=4))
                if debug: print("DEBUG: written generated {}".format(pcp_filename))
                print("command patterns precomputed processed (%d patterns)  [%.2fs]" % (len(self.config.string_table), time.time()-start_time))
                out_pcp_map = dict()

        if do_linked:
            def get_effective_args(args:List[str], cwd:str) -> List[str]:
                ret = []
                for i, arg in enumerate(args):
                    if arg.startswith("@"):
                        f = os.path.normpath(os.path.join(cwd, arg[1:]))
                        if os.path.exists(f):
                            with open(f, "r") as arg_file:
                                r = arg_file.read()
                                ret.extend(r.split())
                        else:
                            print("Parsing args failed! {}".format(args))
                            print("Argfile does not exists {}".format(f))
                    else:
                        ret.append(arg)

                return ret

            start_time = time.time()

            if new_database or not os.path.exists(link_filename):
                linked_pattern = re.compile("|".join([fnmatch.translate(pattern) for pattern in self.config.ld_spec]))
                ared_pattern = re.compile("|".join([fnmatch.translate(pattern) for pattern in self.config.ar_spec]))
                l_size = 0
                a_size = 0

                for e in self.db.iter():
                    if linked_pattern.match(e.binary):
                        effective_args:List[str] = get_effective_args(e.argv, e.cwd)
                        if "-o" in effective_args:
                            outidx = effective_args.index("-o")
                            if outidx < len(effective_args)-1:
                                lnkm = os.path.normpath(os.path.join(e.cwd, effective_args[outidx+1]))
                                if os.path.splitext(lnkm)[1] == ".o" and outidx < len(effective_args)-2:
                                    if os.path.basename(effective_args[outidx+2]) == ".tmp_%s" % (os.path.basename(lnkm)):
                                        continue
                                if lnkm.startswith("/dev/"):
                                    continue
                                assert e.ptr not in out_linked
                                out_linked[e.ptr] = {"l":lnkm, "t":1}
                                l_size += 1
                        elif "--output" in effective_args:
                            outidx = effective_args.index("--output")
                            if outidx < len(effective_args)-1:
                                assert e.ptr not in out_linked
                                out_linked[e.ptr] = {"l": os.path.normpath(os.path.join(e.cwd, effective_args[outidx+1])), "t": 1}
                                l_size += 1
                    if ared_pattern.match(e.binary):
                        effective_args:List[str] = get_effective_args(e.argv, e.cwd)
                        armod = next((x for x in effective_args if x.endswith(".a") or x.endswith(".a.tmp") or x.endswith(".lib") or x.endswith("built-in.o")), None)
                        if armod:
                            if not os.path.isabs(armod):
                                armod = os.path.normpath(os.path.join(e.cwd, armod))
                            if armod.endswith(".a.tmp"):
                                # There are some places in Android build system where ar creates '.a.tmp' archive file only to mv it to '.a' archive right away
                                # Work around that by setting the linking process as the parent bash invocation (which does the ar and mv altogether)
                                if len(e.pipe_eids) > 0:
                                    pipes = [(pp.pid, pp.index) for pp in e.pipe_eids]
                                    we_l = [x for x in self.get_eids(pipes) if len(x.argv) > 0]
                                    if len(we_l) > 0:
                                        if e.ptr not in out_linked:
                                            out_linked[we_l[0].ptr]= {"l": os.path.normpath(os.path.join(e.cwd, armod[:-4])), "t": 0}
                                            a_size += 1
                            else:
                                # Ignore potential linked files not opened for write (e.g. ar t <archive_file>)
                                for x in e.opens:
                                    if x.path == armod and (x.mode & 0x03) > 0:
                                        if e.ptr not in out_linked:
                                            out_linked[e.ptr] = {"l": armod, "t": 0}
                                            a_size += 1

                with open(link_filename, "w", encoding=sys.getfilesystemencoding()) as f:
                    f.write(json.dumps(out_linked, indent=4))
                if debug: print("DEBUG: written generated {}".format(link_filename))
                print("computed linked modules (linked: %d, ared: %d)  [%.2fs]" % (l_size, a_size, time.time() - start_time))
                out_linked = dict()

        if do_compilations:
            start_time = time.time()
            libetrace_dir = os.path.dirname(os.path.realpath(__file__))

            compilation_start_time = time.time()

            if new_database or not os.path.exists(comps_filename):
                if debug: print_mem_usage()
                print("creating compilations input map ...")

                fork_map:Dict[int,List[int]] = {}
                clang_input_execs= []
                clangpp_input_execs= []
                gcc_input_execs = []
                gpp_input_execs = []

                clangxx_compilers:Set[str] = set()

                for ex in self.db.filtered(has_command=True):
                    fns = [op.path for op in ex.opens if op.mode & 3 >= 1] + [os.path.normpath(op.path) for op in ex.opens if op.mode & 3 >= 1]
                    if ex.eid.pid in wr_map:
                        wr_map[ex.eid.pid] += list(set(wr_map[ex.eid.pid]+fns))
                    else:
                        wr_map[ex.eid.pid] = list(set(fns))
                    if ex.eid.pid in fork_map:
                        fork_map[ex.eid.pid] += [ch.eid.pid for ch in ex.childs]
                    else:
                        fork_map[ex.eid.pid] = [ch.eid.pid for ch in ex.childs]
                    if do_compilations and ex.binary != '':
                        b = os.path.realpath(ex.binary) if ex.binary.endswith("/cc") or ex.binary.endswith("/c++") else ex.binary
                        if clangpp_spec_patterns.match(b):
                            clangpp_pattern_match_execs.append(ex.ptr)
                            if ("-cc1" in ex.argv or ("-c" in ex.argv and self.have_integrated_cc1(ex.binary, "-fno-integrated-cc1" not in ex.argv, test_file))) \
                                and "-o" in ex.argv and ("-emit-llvm-bc" not in ex.argv or allow_llvm_bc) \
                                and not self.clang_ir_generation(ex.argv) and not self.clang_pp_input(ex.argv) \
                                and "-analyze" not in ex.argv and ex.eid.pid not in comp_pids and comp_pids.add(ex.eid.pid) is None:
                                clangpp_execs.append(ex.ptr)
                                if not ex.argv[-1] == "-":
                                    clangpp_input_execs.append(ex)
                                if self.have_integrated_cc1(ex.binary, "-fno-integrated-cc1" not in ex.argv, test_file):
                                    integrated_clang_compilers.add(ex.binary)
                        elif clang_spec_patterns.match(b):
                            clang_pattern_match_execs.append(ex.ptr)
                            if ("-cc1" in ex.argv or ("-c" in ex.argv and self.have_integrated_cc1(ex.binary, "-fno-integrated-cc1" not in ex.argv, test_file))) \
                                and "-o" in ex.argv and ("-emit-llvm-bc" not in ex.argv or allow_llvm_bc) \
                                and not self.clang_ir_generation(ex.argv) and not self.clang_pp_input(ex.argv) \
                                and "-analyze" not in ex.argv and ex.eid.pid not in comp_pids and comp_pids.add(ex.eid.pid) is None:
                                clang_execs.append(ex.ptr)
                                if not ex.argv[-1] == "-":
                                    clang_input_execs.append(ex)
                                if self.have_integrated_cc1(ex.binary, "-fno-integrated-cc1" not in ex.argv, test_file):
                                    integrated_clang_compilers.add(ex.binary)
                        elif gcc_spec_patterns.match(b):
                            gcc_comp_pids.append(ex.ptr)
                            if "-" not in ex.argv:
                                gcc_input_execs.append(ex)
                        elif gpp_spec_patterns.match(b):
                            gpp_comp_pids.append(ex.ptr)
                            if "-" not in ex.argv:
                                gpp_input_execs.append(ex)

                print("input map created in [%.2fs]" % (time.time()-start_time))
                start_time = time.time()

                rev_fork_map:Dict[int,int] = {}

                for fork_pid in fork_map:
                    for child in fork_map[fork_pid]:
                        if child in rev_fork_map:
                            assert True, "Multiple parents for process %d" % child
                        else:
                            rev_fork_map[child] = fork_pid

                print("reverse fork map created in [%.2fs]" % (time.time()-start_time))
                start_time = time.time()
                if debug: print_mem_usage()

                computation_finished = multiprocessing.Value("b")
                computation_finished.value = False
                execs_to_process = len(clangpp_execs) + len(clang_execs) + len(gcc_input_execs) + len(gpp_input_execs)
                write_queue = multiprocessing.Queue(maxsize=execs_to_process)

                def writer():
                    if debug: print( "Starting writer process...")
                    is_first = True
                    with open (comps_filename, "w") as of:
                        of.write("{\n")
                        while not computation_finished.value:
                            try:
                                rec:Dict = write_queue.get(timeout=3)
                                if not rec:
                                    continue
                                ptr = next(iter(rec.keys()))
                                if is_first:
                                    is_first = False
                                    of.write(f'"{ptr}": {json.dumps(rec[ptr])}')
                                else:
                                    of.write(",\n" + f'"{ptr}": {json.dumps(rec[ptr])}')
                            except:
                                if computation_finished.value:
                                    break
                        of.write("}")
                    os.sync()
                    if debug: print( "Writer process ends")

                writer_process = multiprocessing.Process(target=writer)
                writer_process.start()

                if True: # clang compilations section
                    start_time = time.time()
                    clang_compilers = [u for u in list(set([os.path.join(x.cwd, self.maybe_compiler_binary(x.binary)) for x in clang_input_execs])) if self.is_elf_executable(u) or self.is_elf_interpreter(u)]
                    clangpp_compilers = [u for u in list(set([os.path.join(x.cwd, self.maybe_compiler_binary(x.binary)) for x in clangpp_input_execs])) if self.is_elf_executable(u) or self.is_elf_interpreter(u)]
                    clangxx_compilers = set(clang_compilers + clangpp_compilers)

                    print("clang matches: %d" % (len(clang_execs)))
                    print("clang++ matches: %d" % (len(clangpp_execs)))
                    print("clang/++ inputs: %d" % (len(clangxx_input_execs)))
                    print("clang compilers: ")
                    for x in clang_compilers:
                        print("  %s" % x)
                    print("clang++ compilers: ")
                    for x in clangpp_compilers:
                        print("  %s" % x)
                    print("integrated clang compilers: ")
                    for x in integrated_clang_compilers:
                        print("  %s" % x)

                    clangxx_input_execs:List[Tuple] = [ (xT[0].ptr, xT[1])
                            for xT in [(x, 1) for x in clang_input_execs] + [(x, 2) for x in clangpp_input_execs]
                            if os.path.join(xT[0].cwd, self.maybe_compiler_binary(xT[0].binary)) in clangxx_compilers
                    ]

                    del clang_input_execs
                    del clangpp_input_execs
                    del clangxx_compilers

                    if len(clangxx_input_execs) > 0:
                        clang_work_queue = multiprocessing.Queue(maxsize=len(clangxx_input_execs))
                        for input_index in range(len(clangxx_input_execs)):
                            clang_work_queue.put(input_index)

                        clang_c = clang.clang(verbose, debug, clang_compilers, clangpp_compilers, integrated_clang_compilers, debug_compilations, allow_pp_in_compilations)

                        def clang_executor(worker_idx):
                            worker = exec_worker.ExecWorker(os.path.join(libetrace_dir, "worker"))

                            if debug: print ("Worker {} starting...".format(worker_idx))
                            no_o_args = 0
                            not_exist = 0
                            empty_output = 0
                            not_allow_pp = 0
                            parsing_fail = 0
                            cc1as_skipped = 0
                            while not clang_work_queue.empty():
                                try:
                                    pos = clang_work_queue.get(timeout=3)
                                except:
                                    continue
                                with processed.get_lock():
                                    processed.value += 1
                                    pbar.n = processed.value
                                    pbar.refresh()
                                ptr, compiler_type = clangxx_input_execs[pos]
                                exe = self.get_exec_at_pos(ptr)

                                argv:List[str] = exe.argv.copy()
                                bin = exe.binary
                                cwd = exe.cwd
                                have_int_cc1 = self.have_integrated_cc1(os.path.join(cwd, bin), "-fno-integrated-cc1" not in argv, test_file)

                                try:
                                    out, ret_code = worker.runCmd(cwd, bin, argv[1:] + ["-###"])
                                except exec_worker.ExecWorkerException as e:
                                    worker.initialize()
                                    print("Exception while running -###")
                                    print ("[%s] %s"%(cwd," ".join(argv[1:] + ["-###"])))
                                    continue
                                lns = out.split("\n")
                                idx = [k for k,u in enumerate(lns) if "(in-process)" in u]
                                if idx and len(lns) >= idx[0] + 2:
                                    ncmd = lns[idx[0] + 1]
                                    try:
                                        argv = [
                                                    u[1:-1].encode('latin1').decode('unicode-escape').encode('latin1').decode('utf-8')
                                                    for u in libetrace.parse_compiler_triple_hash(ncmd)
                                                ]
                                        have_int_cc1 = 1
                                    except Exception as e:
                                        print (exe.json(), flush=True)
                                        print (lns, flush=True)
                                        print (ncmd, flush=True)

                                if "-cc1as" in argv:
                                    cc1as_skipped += 1
                                    # Remove clang invocations with -cc1as (it's not actual C/C++ compilation which generates errors later)
                                    continue

                                extra_arg_num = 0
                                for i,u in enumerate(reversed(argv)):
                                    if u not in self.config.clang_tailopts:
                                        break
                                    extra_arg_num+=1
                                fn = os.path.normpath(os.path.join(cwd, exe.argv[-1-i]))
                                if not os.path.exists(fn):
                                    if "tmp" not in fn:
                                        print ("Error: {} - does not exist".format(fn), flush=True)
                                        print ("Original cmd: {}".format(exe.argv), flush=True)
                                    not_exist += 1
                                    continue
                                # fn - the path to the compiled file that exists
                                output_arg_index = next((i for i,x in enumerate(argv) if x=="-o"), None)
                                if output_arg_index is None:
                                    print ("****\nno -o arg \n{}\n{}\n****".format(argv, out), flush=True)
                                    no_o_args += 1
                                    continue
                                argv.insert(output_arg_index,"-dD")
                                argv.insert(output_arg_index,"-E")
                                argv.insert(output_arg_index,"-P")
                                argv.insert(output_arg_index,"-v")
                                if compiler_type==1:
                                    argv.insert(output_arg_index,"c")
                                else:
                                    argv.insert(output_arg_index,"c++")
                                argv.insert(output_arg_index,"-x")
                                argv[output_arg_index+7] = "-"
                                if extra_arg_num>0:
                                    argv.pop(-1-extra_arg_num)
                                    argv.append("-")
                                else:
                                    argv[-1] = "-"
                                try:
                                    output, ret_code = worker.runCmd(cwd, bin, argv[1:], "")  # last parameter is empty stdin for clang
                                except exec_worker.ExecWorkerException as e:
                                    worker.initialize()
                                    print (e)
                                    print ("cwd  =  {}".format(cwd))
                                    print ("bin  =  {}".format(bin))
                                    print ("argv =  {}".format(" ".join(argv[1:])))
                                    continue
                                if output is None:
                                    empty_output += 1
                                    print ("empty output", flush=True)
                                    continue
                                if not clang_c.allow_pp_in_compilations and '-E' in exe.argv:
                                    # print ("not allow_pp_in_compilations and -E in args")
                                    not_allow_pp += 1
                                    continue

                                # Remove bogus compiled files (e.g. /dev/null)
                                # cks = list(comp_dict_clang_chunk.keys())
                                # for k in cks:
                                #     if comp_dict_clang_chunk[k][0][0].startswith("/dev/"):
                                #         del comp_dict_clang_chunk[k]

                                compiler_path = os.path.join(cwd, self.maybe_compiler_binary(bin))
                                json_repr = exe.json()
                                comp_objs = clang_c.get_object_files(json_repr, have_int_cc1, fork_map, rev_fork_map, wr_map)

                                try:
                                    includes, defs, undefs = clang_c.parse_defs(output)
                                    includes = [os.path.realpath(os.path.normpath(os.path.join(cwd, x))) for x in includes]
                                    ipaths = clang_c.compiler_include_paths(compiler_path)
                                    for u in ipaths:
                                        if u not in includes:
                                            includes.append(u)
                                    ifiles = clang_c.parse_include_files(json_repr, includes)
                                except Exception as e:
                                    print (cwd, flush=True)
                                    print (output, flush=True)
                                    parsing_fail += 1
                                    with open(".nfsdb.log.err", "a") as ef:
                                        ef.write("ERROR: Exception while processing compilation:\n%s\n" % (json_repr))
                                        ef.write("%s\n\n" % (str(e)))
                                    continue

                                src_type = clang_c.get_source_type(argv, compiler_type, os.path.splitext(fn)[1])
                                absfn = os.path.realpath(os.path.normpath( os.path.join(cwd, fn)))

                                if absfn.startswith("/dev/"):
                                    if debug: print("\nSkipping bogus file {} ".format(absfn))
                                    continue

                                write_queue.put({ ptr:{
                                        "f": [absfn],
                                        "i": includes,
                                        "d": [{"n":di[0], "v":di[1]} for di in defs],
                                        "u": [{"n":ui[0], "v":ui[1]} for ui in undefs],
                                        "h": ifiles,
                                        "s": src_type,
                                        "o": comp_objs,
                                        "p": have_int_cc1
                                    }})
                                found_comps.value += 1

                            if debug: print ("Worker {} finished! ok={} no_o_args={} not_exist={} empty_output={} not_allow_pp ={} parsing_fail={} cc1as_skipped={}".format(worker_idx,processed.value,no_o_args,not_exist,empty_output,not_allow_pp,parsing_fail,cc1as_skipped), flush=True)

                        print("Searching for clang compilations ... (%d candidates; %d jobs)" % (len(clangxx_input_execs), jobs))
                        if debug: print_mem_usage()
                        print(flush=True)

                        pbar = progressbar(total = len(clangxx_input_execs), disable=None)
                        processed = multiprocessing.Value("i")
                        processed.value = 0
                        found_comps = multiprocessing.Value("i")
                        found_comps.value = 0    
                        sstart = time.time()
                        procs = []
                        for i in range(jobs-1):
                            p = multiprocessing.Process(target=clang_executor, args=(i,))
                            procs.append(p)
                            p.start()

                        for proc in procs:
                            proc.join()

                        pbar.n = processed.value
                        pbar.refresh()
                        pbar.close()

                        print(flush=True)
                        print(f"Workers finished in [{time.time() - sstart:.2f}s] - found {found_comps.value} compilations.")
                        if debug: print_mem_usage()

                    print("finished searching for clang compilations [%.2fs]" % (time.time()-start_time))

                if True: # gcc compilations section
                    start_time = time.time()
                    gcc_compilers = [u for u in list(set([os.path.join(x.cwd, self.maybe_compiler_binary(x.binary)) for x in gcc_input_execs])) if self.is_elf_executable(u)]
                    gpp_compilers = [u for u in list(set([os.path.join(x.cwd, self.maybe_compiler_binary(x.binary)) for x in gpp_input_execs])) if self.is_elf_executable(u)]
                    gxx_compilers = set(gcc_compilers + gpp_compilers)

                    gxx_input_execs:List[Tuple] = [ (xT[0].ptr, xT[1])
                            for xT in [(x, 1) for x in gcc_input_execs] + [(x, 2) for x in gpp_input_execs]
                            if os.path.join(xT[0].cwd, self.maybe_compiler_binary(xT[0].binary)) in gxx_compilers
                        ]

                    print("gcc matches: %d" % (len(gcc_comp_pids)))
                    print("g++ matches: %d" % (len(gpp_comp_pids)))
                    print("gcc/++ inputs: %d" % (len(gxx_input_execs)))
                    print("gcc compilers: ")
                    for x in gcc_compilers:
                        print("  %s" % x)
                    print("g++ compilers: ")
                    for x in gpp_compilers:
                        print("  %s" % x)
                    del gxx_compilers
                    del gcc_input_execs
                    del gpp_input_execs

                    if len(gxx_input_execs) > 0:
                        gcc_work_queue = multiprocessing.Queue(maxsize=len(gxx_input_execs))
                        for input_index in range(len(gxx_input_execs)):
                            gcc_work_queue.put(input_index)
                        gcc_c = gcc.gcc(verbose, debug, gcc_compilers, gpp_compilers, debug_compilations)

                        plugin_insert = "-fplugin=" + os.path.realpath(os.path.join(libetrace_dir, "libgcc_input_name.so"))

                        input_compiler_path = None # TODO -make parameter of this!

                        def gcc_executor(worker_idx):
                            worker = exec_worker.ExecWorker(os.path.join(libetrace_dir, "worker"))
                            if debug: print ("Worker {} starting...".format(worker_idx))

                            while not gcc_work_queue.empty():
                                try:
                                    pos = gcc_work_queue.get(timeout=3)
                                except:
                                    continue
                                with processed.get_lock():
                                    processed.value += 1
                                    pbar.n = processed.value
                                    pbar.refresh()
                                ptr, compiler_type = gxx_input_execs[pos]
                                exe = self.get_exec_at_pos(ptr)
                                argv:List[str] = exe.argv.copy()
                                bin = exe.binary
                                cwd = exe.cwd

                                if len(argv) == 2 and (argv[1] == "--version" or argv[1] == "-dumpmachine"):
                                    continue
                                try:
                                    # print (bin)
                                    # bin = str(Path(shutil.which(bin)).resolve())
                                    # print (bin)
                                    if os.path.exists(cwd) and os.path.exists(bin):
                                        out, ret = worker.runCmd(cwd, bin, argv[1:] + ["-###"])
                                    else:
                                        print("Command or cwd does not exist cwd={} bin={}".format(cwd,bin))
                                        continue
                                except exec_worker.ExecWorkerException as e:
                                    worker.initialize()
                                    print("**********************************\n"\
                                        "ERROR = {}\n"\
                                        "cwd   = {}\n"\
                                        "bin   = {}\n"\
                                        "argv  = {}\n"\
                                        "**********************************".format(e,cwd,bin," ".join(argv[1:] + ["-###"])))
                                    continue
                                # if ret != 0:
                                #     print ("ret  =  {}:".format(ret))
                                #     print ("cwd  =  {}".format(cwd))
                                #     print ("bin  =  {}".format(bin))
                                #     print ("argv =  {}".format(" ".join(argv[1:] + ["-###"])))
                                #     print ("----------------------------------------")
                                #     print (out)
                                #     print ("----------------------------------------")
                                #     continue

                                lns = [ shlex.split(x)
                                    for x in out.split("\n")
                                    if x.startswith(" ") and re.match(cc1_patterns,x.split()[0])]

                                if len(lns) == 0:
                                    # print("No cc1 \n{}".format(lns))
                                    continue

                                nargv = [bin if input_compiler_path is None else input_compiler_path] + argv[1:]
                                if '-o' in nargv:
                                    i = nargv.index('-o')
                                    nargv.pop(i)
                                    nargv.pop(i)
                                nargv.append(plugin_insert)
                                if '-pipe' in nargv:
                                    nargv.remove('-pipe')
                                if '-E' in nargv:
                                    nargv.remove('-E')
                                if '-c' not in nargv:
                                    nargv.append('-c')

                                try:
                                    out, ret = worker.runCmd(cwd, nargv[0], nargv[1:], "")
                                except exec_worker.ExecWorkerException as e:
                                    worker.initialize()
                                    print("Exception while running gcc -fplugin command:")
                                    print ("[%s] %s"%(cwd," ".join(nargv)))  
                                    continue
                                if ret != 0:
                                    print ("Error getting input files from gcc compilation command (%d):"%(ret))
                                    print ("----------------------------------------")
                                    print (out)
                                    print ("----------------------------------------")
                                    continue

                                fns = out.strip().split("\n")

                                if not all((os.path.exists(os.path.join(cwd, x)) for x in fns)):
                                    print ("args: {}".format(nargv))
                                    print ("Missing gcc compilation input files:")
                                    for x in fns:
                                        if not os.path.exists(os.path.join(cwd,x)):
                                            print ("  [%s] %s"%(cwd,x))
                                    continue
                                # Spawn the child to read all include paths and preprocessor definitions
                                nargv = [bin] + argv[1:]
                                while '-include' in nargv:
                                    i = nargv.index('-include')
                                    nargv.pop(i)
                                    nargv.pop(i)
                                if '-o' in nargv:
                                    i = nargv.index('-o')
                                    nargv[i] = '-E'
                                    nargv[i+1] = '-P'
                                    nargv.insert(i+2,'-v')
                                    nargv.insert(i+3,'-dD')
                                else:
                                    nargv+=['-E','-P','-v','-dD']
                                for x in fns:
                                    li = [i for i,u in enumerate(nargv) if u==x]
                                    if len(li):
                                        nargv.pop(li[-1])
                                    else:
                                        print ("CAN'T POP FILENAME \n{}\n{}".format(fns,nargv))
                                nargv.append('-')
                                try:
                                    out, ret = worker.runCmd(cwd, nargv[0], nargv[1:], "")
                                except exec_worker.ExecWorkerException as e:
                                    worker.initialize()
                                    print("Exception")
                                    print ("[%s] %s"%(cwd," ".join(nargv)))
                                    continue

                                if ret != 0:
                                    print ("Error getting compilation data (%d) with command\n  %s"%(ret," ".join(nargv)))
                                    print ("----------------------------------------")
                                    print (out)
                                    print ("----------------------------------------")
                                    continue

                                compiler_path = os.path.join(cwd, self.maybe_compiler_binary(bin))
                                exe_dct = exe.json()
                                comp_objs = gcc_c.get_object_files(exe_dct, fork_map, rev_fork_map, wr_map)

                                #try:
                                includes, defs, undefs = gcc_c.parse_defs(out, exe_dct)
                                includes = [os.path.realpath(os.path.normpath(os.path.join(cwd, x))) for x in includes]
                                ipaths = gcc_c.compiler_include_paths(compiler_path)
                                for u in ipaths:
                                    if u not in includes:
                                        includes.append(u)
                                ifiles = gcc_c.parse_include_files(exe_dct, includes)
                                # except Exception as e:
                                #     print("ERROR: Exception while processing compilation:\n%s\n" % (exe.json()), flush=True)
                                #     # print (cwd, flush=True)
                                #     # print (out, flush=True)
                                #     with open(".nfsdb.log.err", "a") as ef:
                                #         ef.write("ERROR: Exception while processing compilation:\n%s\n" % (exe.json()))
                                #         ef.write("%s\n\n" % (str(e)))
                                #         traceback.print_exc(limit=0,chain =True)
                                #     continue

                                out_fns = set()
                                src_type = None
                                for fn in fns:
                                    src_type = gcc_c.get_source_type(nargv, compiler_type, os.path.splitext(fn)[1])
                                    absfn = os.path.realpath(os.path.normpath( os.path.join(cwd, fn)))
                                    if not absfn.startswith("/dev/"):
                                        out_fns.add(absfn)

                                if len(out_fns) == 0:
                                    if debug: print("Skipping empty src compilation (probably /dev/null file) ")
                                    continue

                                write_queue.put({ptr: {
                                        "f": list(out_fns),
                                        "i": includes,
                                        "d": [{"n":di[0], "v":di[1]} for di in defs],
                                        "u": [{"n":ui[0], "v":ui[1]} for ui in undefs],
                                        "h": ifiles,
                                        "s": src_type,
                                        "o": comp_objs,
                                        "p": 0
                                    }})
                                found_comps.value += 1

                        print("Searching for gcc compilations ... (%d candidates; %d jobs)" % (len(gxx_input_execs), jobs))
                        if debug: print_mem_usage()
                        print(flush=True)
                        pbar = progressbar(total = len(gxx_input_execs), disable=None )
                        processed = multiprocessing.Value("i")
                        processed.value = 0
                        found_comps = multiprocessing.Value("i")
                        found_comps.value = 0                        
                        sstart = time.time()
                        procs = []
                        for i in range(jobs-1):
                            p = multiprocessing.Process(target=gcc_executor, args=(i,))
                            procs.append(p)
                            p.start()

                        for proc in procs:
                            proc.join()

                        pbar.n = processed.value
                        pbar.refresh()
                        pbar.close()
                        print(flush=True)
                        print(f"Workers finished in {time.time() - sstart:.2f}s - found {found_comps.value} compilations.")
                        if debug: print_mem_usage()

                    print("finished searching for gcc compilations [%.2fs]" % (time.time() - start_time))

                computation_finished.value = True
                writer_process.join()

                print("computed compilations  [%.2fs]" % (time.time()-compilation_start_time))

        if not no_update:
            if len(out_linked.keys()) == 0 and os.path.exists(link_filename):
                if debug:print ("Linked info empty - loading from file...")
                with open(link_filename, "r") as input_json:
                    out_linked = json.load(input_json)
                if debug: print("DEBUG: loaded pre-generated {}".format(link_filename))

            if len(out_pcp_map.keys()) == 0 and os.path.exists(pcp_filename):
                if debug:print ("PCP map empty - loading from file...")
                with open(pcp_filename, "r") as input_json:
                    out_pcp_map = json.load(input_json)
                if debug: print("DEBUG: loaded pre-generated {}".format(pcp_filename))

            if len(out_rbm.keys()) == 0 and os.path.exists(rbm_filename):
                if debug:print ("RBM info empty - loading from file...")
                with open(rbm_filename, "r") as input_json:
                    out_rbm = json.load(input_json)
                if debug: print("DEBUG: loaded pre-generated {}".format(rbm_filename))

            if len(out_compilation_info.keys()) == 0 and os.path.exists(comps_filename):
                if debug:print ("Compilation info empty - loading from file...")
                with open(comps_filename, "r") as input_json:
                    out_compilation_info = json.load(input_json)
                if debug: print("DEBUG: loaded pre-generated {}".format(comps_filename))
            print("Rewriting {} ...".format(json_db_filename))

            with open (json_db_filename + ".tmp", "wb") as ofile:
                ofile.write(b"[")
                pbar = progressbar(total=len(self.db), disable=None)
                for i, ex in enumerate(self.db.iter()):
                    pbar.n += 1
                    pbar.refresh()
                    ptr = str(ex.ptr)
                    r = ex.json()
                    u = ""
                    lnk = out_linked.get(ptr, None)
                    if lnk is not None:
                        u += " l"
                        r.update(lnk)
                    ci = out_compilation_info.get(ptr, None)
                    if ci is not None:
                        u += " c"
                        r['d'] = ci
                    pcp = out_pcp_map.get(ptr, None)
                    if pcp is not None:
                        u += " p"
                        r['n'] = pcp
                    wpid = out_rbm.get(str(r['p']), None)
                    if wpid is not None:
                        u += " r"
                        r['m'] = wpid
                    if verbose and len(u) > 0:
                        print("\n Updating {} {}".format(ptr,u))

                    if i > 0:
                        ofile.write(b",\n")
                    else:
                        ofile.write(b"\n")
                    try:
                        ofile.write(json.dumps(r).encode(sys.getdefaultencoding()))
                    except Exception as ee:
                        print(ee)
                ofile.write(b"\n]")
            print(flush=True)
            os.sync()

            shutil.move(json_db_filename + ".tmp", json_db_filename)
            print("updated parsed entries  [%.2fs]" % (time.time()-start_time))

        print("Done [%.2fs]" % (time.time()-total_start_time))
