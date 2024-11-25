"""
Module contains set of useful libetrace binding functions.
"""
from typing import List, Dict, Set, Tuple, Optional, Iterable
import fnmatch
import multiprocessing
import json
import sys
import os
import time
import shlex
import re
import subprocess
import base64
from functools import lru_cache
import shutil
import libetrace
from bas import gcc
from bas import clang
from bas import exec_worker

try:
    # If tqdm is available use it for nice progressbar
    from tqdm import tqdm as progressbar
except ModuleNotFoundError:
    # Fallback if tqdm is not installed
    class progressbar:
        n = 0
        x = None

        def __init__(self, data:Optional[Iterable]=None, x=None, total=0, disable=None):
            self.data = data
            self.x = x
            self.total = total
            if disable is None:
                self.disable = not sys.stdout.isatty()
            else:
                self.disable = disable

        def __iter__(self):
            return iter(self.data) if self.data else iter([])

        def refresh(self):
            if not self.disable:
                print(f'\r\033[KProcessing {self.n} / {self.total}', end='')

        def close(self):
            pass


class DepsParam:
    """
    Extended parameter used in deps generation. Enables use per-path excludes and direct switch.

    :param file: path to process
    :type file: str
    :param direct: return only direct dependencies of this file
    :type direct: bool
    :param exclude_cmd: list of commands to exclude while generating dependencies of this file
    :type exclude_cmd: List[str]
    :param exclude_pattern: list of file patterns to exclude while generating dependencies of this file
    :type exclude_pattern: List[str]
    :param negate_pattern: reversed pattern - exclude is included
    :type negate_pattern: bool
    """

    def __init__(self, file: str, direct: Optional[bool] = None, exclude_cmd: Optional[List[str]] = None,
                exclude_pattern: Optional[List[str]] = None, negate_pattern: bool = False):
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
    """
    CAS configuration handler. Loads config from filesystem, and generates excludes.
    Specifies compilers and linkers matching patterns, dependency excludes patterns and more.

    :param config_file: config file path
    :type config_file: str

    """
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
        self.module_dependencies_with_pipes:List[str] = []
        self.module_dependencies_exclude_with_pipes:List[str] = []
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

    def gen_excludes_for_path(self, path) -> Tuple[List, List, List]:
        excl_patterns = [x for x in self.dependency_exclude_patterns]
        excl_commands = []
        excl_commands_index = []

        for pattern_variant, pattern_val in self.additional_module_exclude_pattern_variants.items():
            if fnmatch.fnmatch(path, pattern_variant):
                excl_patterns += pattern_val

        for command_variant, command_val in self.exclude_command_variants_index.items():
            if fnmatch.fnmatch(path, command_variant):
                excl_commands_index += command_val

        return excl_patterns, excl_commands, excl_commands_index

    def get_use_pipe_for_path(self, path:str, global_pipe_opt:bool) -> bool:
        """Function returns proper use-pipe argument if overwrite was defined in config

        :param path: File path
        :type path: str
        :param global_pipe_opt: global use-pipe argument
        :type global_pipe_opt: bool
        :return: True if pipes should be used otherwise False
        :rtype: bool
        """
        if global_pipe_opt:
            for path_wildcard in self.module_dependencies_exclude_with_pipes:
                if fnmatch.fnmatch(path, path_wildcard):
                    return False
            return True
        else:
            for path_wildcard in self.module_dependencies_with_pipes:
                if fnmatch.fnmatch(path, path_wildcard):
                    return True
            return False

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
        self.config = None
        self.db_path = None
        self.cache_db_path= None

    def set_config(self, config: CASConfig):
        """
        Function used to assign CASConfig to database

        :param config: config object
        :type config: CASConfig
        """
        self.config = config

    def load_db(self, db_file:str, debug: bool=False, quiet: bool=True) -> bool:
        """
        Function uses libetrace.load to load database and applies config.

        :param db_file: Database file
        :type db_file: str
        :param debug: print debug information, defaults to False
        :type debug: bool, optional
        :param quiet: suppress verbose prints, defaults to True
        :type quiet: bool, optional
        :return: True if load succeed otherwise False
        :rtype: bool
        """

        self.db_loaded = self.db.load(db_file, debug=debug, quiet=quiet)
        self.source_root = self.db.source_root
        assert self.config is not None, "Config is empty!"
        self.config.apply_source_root(self.source_root)
        self.db_path = db_file
        return self.db_loaded

    @staticmethod
    def get_db_version(db_file: str) -> str:
        """
        Function returns version of given nfsdb file

        :param db_file: Database file
        :type db_file: str
        """
        return libetrace.database_version(db_file)

    @staticmethod
    def get_img_version(db_file: str) -> str:
        """
        Function returns version of given image file
        
        :param db_file: Database file
        :type db_file: str
        """
        try:
            return libetrace.image_version(db_file)
        except:
            return "UNKNOWN"

    def load_deps_db(self, db_file:str, debug: bool=False, quiet: bool=True, mp_safe: bool=True, no_map_memory: bool = False) -> bool:
        """
        Function uses libetrace.load_deps to load database from given filename

        :param db_file: database file path
        :type db_file: str
        :param debug: print debug information, defaults to False
        :type debug: bool, optional
        :param quiet: suppress verbose prints, defaults to True
        :type quiet: bool, optional
        :param mp_safe: load database in read-only mode slower but safer when using multiprocessing, defaults to True
        :type mp_safe: bool, optional
        :param no_map_memory: prevents memory mapping, defaults to False
        :type no_map_memory: bool, optional
        :return: True if load succeed otherwise False
        :rtype: bool
        """

        self.cache_db_loaded = self.db.load_deps(db_file, debug=debug, quiet=quiet, mp_safe=mp_safe, no_map_memory=no_map_memory)
        self.cache_db_path = db_file
        return self.cache_db_loaded

    @lru_cache(maxsize=1)
    def linked_module_paths(self) -> List[str]:
        """
        Function returns linked modules paths.
        This function uses cache.

        :return: List of module paths
        :rtype: List[str]
        """

        return list({x[0] for x in self.db.linked_module_paths()})

    @lru_cache(maxsize=1)
    def object_files_paths(self) -> List[str]:
        """
        Function returns objects file paths.
        This function uses cache.

        :return: List of object files paths
        :rtype: List[str]
        """

        return list({ obj_p
                     for x in self.db.filtered_execs(has_comp_info=True)
                     for obj_p in x.compilation_info.object_paths })

    @lru_cache(maxsize=1)
    def linked_modules(self) -> List[libetrace.nfsdbEntryOpenfile]:
        """
        Function returns linked modules as libetrace.nfsdbEntryOpenfile objects.
        This function uses cache.

        :return: List of opens objects that are modules
        :rtype: List[libetrace.nfsdbEntryOpenfile]
        """

        return list({x[0] for x in self.db.linked_modules()})

    def get_entries_with_pid(self, pid: int) -> List[libetrace.nfsdbEntry]:
        """
        Function return list of execs with given pid.

        :param pid: process pid
        :type pid: int
        :return: list of execs object
        :rtype: List[libetrace.nfsdbEntry]
        """

        return self.db[(pid,)]

    def get_entries_with_pids(self, pidlist: "List[Tuple[int, int]]| List[Tuple[int,]]") -> List[libetrace.nfsdbEntry]:
        """
        Function return list of execs with given list of pid,idx tuples.

        :param pidlist: process pid and optional index tuple
        :type pidlist: List[Tuple[int, Optional[int]]]
        :return: list of execs object
        :rtype: List[libetrace.nfsdbEntry]
        """

        return self.db[pidlist]

    def get_exec(self, pid: int, index: int) -> libetrace.nfsdbEntry:
        """
        Function returns exec with given pid and index.

        :param pid: process pid
        :type pid: int
        :param index: process index
        :type index: int
        :return: exec object
        :rtype: libetrace.nfsdbEntry
        """
        return self.db[(pid, index)][0]

    def get_exec_at_pos(self, ptr: int) -> libetrace.nfsdbEntry:
        """
        Function returns exec with given pointer value.

        :param pos: pointer to exec `libetrace.nfsdbEntry.ptr`
        :type pos: int
        :return: exec object
        :rtype: libetrace.nfsdbEntry
        """
        return self.db[ptr]

    def get_eid(self, eid: "Tuple[int, int] | Tuple[int,]") -> List[libetrace.nfsdbEntry]:
        """
        Function returns execs with given eid value.

        :param pos: list of execs matching given eid value
        :type pos: `Tuple[int, int] | Tuple[int,]`
        :return: list of exec objects
        :rtype: `List[libetrace.nfsdbEntry]`
        """
        return self.db[eid]

    def get_eids(self, eids: "List[Tuple[int, int]] | List[Tuple[int,]]"):
        """
        Function returns execs with given eid values.

        :param pos: list of execs matching given eid values
        :type pos: `"List[Tuple[int, int]] | List[Tuple[int,]]"`
        :return: list of exec objects
        :rtype: `List[libetrace.nfsdbEntry]`
        """
        return self.db[eids]

    def opens_list(self) -> List[libetrace.nfsdbEntryOpenfile]:
        """
        Function returns list of all opens objects.

        :return: list of opens
        :rtype: List[libetrace.nfsdbEntryOpenfile]
        """
        return self.db.opens()

    def opens_iter(self) -> libetrace.nfsdbOpensIter:
        """
        Function returns iterator to all opens objects.

        :return: opens iterator
        :rtype: libetrace.nfsdbOpensIter
        """
        return self.db.opens_iter()

    def opens_paths(self) -> List[str]:
        """
        Function returns list of unique opens paths.

        :return: list of open paths
        :rtype: List[str]
        """
        return self.db.opens_paths()

    def filtered_paths(self, file_filter:Optional[List]=None, path: Optional[List[str]] = None,
                        has_path: Optional[str] = None, wc: Optional[str] = None, re: Optional[str] = None,
                        compiled: Optional[bool] = None, linked: Optional[bool] = None, linked_static: Optional[bool] = None,
                        linked_shared: Optional[bool] = None, linked_exe: Optional[bool] = None, plain: Optional[bool] = None,
                        compiler: Optional[bool] = None, linker: Optional[bool] = None, binary: Optional[bool] = None,
                        symlink: Optional[bool] = None, no_symlink: Optional[bool] = None,
                        file_exists: Optional[bool] = None, file_not_exists: Optional[bool] = None, dir_exists: Optional[bool] = None,
                        has_access: Optional[int] = None, negate: Optional[bool] = None,
                        at_source_root: Optional[bool] = None, not_at_source_root: Optional[bool] = None,
                        source_type: Optional[int] = None) -> List[str]:
        return self.db.filtered_paths(file_filter=file_filter, path=path, has_path=has_path, wc=wc, re=re, compiled=compiled,
                                        linked=linked, linked_static=linked_static, linked_shared=linked_shared,
                                        linked_exe=linked_exe, plain=plain, compiler=compiler,linker=linker, binary=binary, symlink=symlink,
                                        no_symlink=no_symlink, file_exists=file_exists, file_not_exists=file_not_exists, dir_exists=dir_exists,
                                        has_access=has_access, negate=negate, at_source_root=at_source_root, not_at_source_root=not_at_source_root, source_type=source_type)

    def filtered_paths_iter(self, file_filter:Optional[List]=None, path: Optional[List[str]] = None,
                        has_path: Optional[str] = None, wc: Optional[str] = None, re: Optional[str] = None,
                        compiled: Optional[bool] = None, linked: Optional[bool] = None, linked_static: Optional[bool] = None,
                        linked_shared: Optional[bool] = None, linked_exe: Optional[bool] = None, plain: Optional[bool] = None,
                        compiler: Optional[bool] = None, linker: Optional[bool] = None, binary: Optional[bool] = None,
                        symlink: Optional[bool] = None, no_symlink: Optional[bool] = None,
                        file_exists: Optional[bool] = None, file_not_exists: Optional[bool] = None, dir_exists: Optional[bool] = None,
                        has_access: Optional[int] = None, negate: Optional[bool] = None,
                        at_source_root: Optional[bool] = None, not_at_source_root: Optional[bool] = None,
                        source_type: Optional[int] = None) -> libetrace.nfsdbFilteredOpensPathsIter:
        return self.db.filtered_paths_iter(file_filter=file_filter, path=path, has_path=has_path, wc=wc, re=re, compiled=compiled,
                                        linked=linked, linked_static=linked_static, linked_shared=linked_shared,
                                        linked_exe=linked_exe, plain=plain, compiler=compiler,linker=linker, binary=binary, symlink=symlink,
                                        no_symlink=no_symlink, file_exists=file_exists, file_not_exists=file_not_exists, dir_exists=dir_exists,
                                        has_access=has_access, negate=negate, at_source_root=at_source_root, not_at_source_root=not_at_source_root, source_type=source_type)


    def filtered_opens(self, file_filter:Optional[List]=None, path: Optional[List[str]] = None,
                        has_path: Optional[str] = None, wc: Optional[str] = None, re: Optional[str] = None,
                        compiled: Optional[bool] = None, linked: Optional[bool] = None, linked_static: Optional[bool] = None,
                        linked_shared: Optional[bool] = None, linked_exe: Optional[bool] = None, plain: Optional[bool] = None,
                        compiler: Optional[bool] = None, linker: Optional[bool] = None, binary: Optional[bool] = None,
                        symlink: Optional[bool] = None, no_symlink: Optional[bool] = None,
                        file_exists: Optional[bool] = None, file_not_exists: Optional[bool] = None, dir_exists: Optional[bool] = None,
                        has_access: Optional[int] = None, negate: Optional[bool] = None,
                        at_source_root: Optional[bool] = None, not_at_source_root: Optional[bool] = None,
                        source_type: Optional[int] = None) -> List[libetrace.nfsdbEntryOpenfile]:
        """
        Function filters opens with given `file_filter` and set of global filter parameters and returns list of opens objects.

        Global filter switches are used before `file_filter`.

        :param file_filter: file filter object, defaults to None
        :type file_filter: Optional[List], optional
        :param path: match opens with given paths, defaults to None
        :type path: Optional[List[str]], optional
        :param has_path: _description_, defaults to None
        :type has_path: Optional[str], optional
        :param wc: match opens with wildcard, defaults to None
        :type wc: Optional[str], optional
        :param re: match opens with regex, defaults to None
        :type re: Optional[str], optional
        :param compiled: returns opens that has been use in compilation, defaults to None
        :type compiled: Optional[bool], optional
        :param linked: returns opens that has been use in linking, defaults to None
        :type linked: Optional[bool], optional
        :param linked_static: returns opens that has been use in linking static module, defaults to None
        :type linked_static: Optional[bool], optional
        :param linked_shared: returns opens that has been use in linking shared module, defaults to None
        :type linked_shared: Optional[bool], optional
        :param linked_exe: returns opens that has been use in linking executable module, defaults to None
        :type linked_exe: Optional[bool], optional
        :param plain: returns opens that wasn't used in any linking or compilation, defaults to None
        :type plain: Optional[bool], optional
        :param compiler: returns opens which path is compilers, defaults to None
        :type compiler: Optional[bool], optional
        :param linker: returns opens which path is linkers, defaults to None
        :type linker: Optional[bool], optional
        :param binary: returns opens which path is binary file, defaults to None
        :type binary: Optional[bool], optional
        :param symlink: returns opens which path is symlink, defaults to None
        :type symlink: Optional[bool], optional
        :param no_symlink: returns opens which path is not symlink, defaults to None
        :type no_symlink: Optional[bool], optional
        :param file_exists: returns opens which path exists while building database, defaults to None
        :type file_exists: Optional[bool], optional
        :param file_not_exists: returns opens which path did not exists while building database, defaults to None
        :type file_not_exists: Optional[bool], optional
        :param dir_exists: returns opens which path is dir and exists while building database, defaults to None
        :type dir_exists: Optional[bool], optional
        :param has_access: returns opens which where opened with given access type, defaults to None
        :type has_access: Optional[int], optional
        :param negate: negate global switches, defaults to None
        :type negate: Optional[bool], optional
        :param at_source_root: returns opens with source root in begining of path, defaults to None
        :type at_source_root: Optional[bool], optional
        :param not_at_source_root: returns opens without source root in begining of path, defaults to None
        :type not_at_source_root: Optional[bool], optional
        :param source_type: returns opens with path that was compiled and matching given source type, defaults to None
        :type source_type: Optional[int], optional
        :return: list of opens objects
        :rtype: List[libetrace.nfsdbEntryOpenfile]
        """
        return self.db.filtered_opens(file_filter=file_filter, path=path, has_path=has_path, wc=wc, re=re, compiled=compiled,
                                        linked=linked, linked_static=linked_static, linked_shared=linked_shared,
                                        linked_exe=linked_exe, plain=plain, compiler=compiler,linker=linker, binary=binary, symlink=symlink,
                                        no_symlink=no_symlink, file_exists=file_exists, file_not_exists=file_not_exists, dir_exists=dir_exists,
                                        has_access=has_access, negate=negate, at_source_root=at_source_root, not_at_source_root=not_at_source_root, source_type=source_type)

    def filtered_opens_iter(self, file_filter:Optional[List]=None, path: Optional[List[str]] = None,
                        has_path: Optional[str] = None, wc: Optional[str] = None, re: Optional[str] = None,
                        compiled: Optional[bool] = None, linked: Optional[bool] = None, linked_static: Optional[bool] = None,
                        linked_shared: Optional[bool] = None, linked_exe: Optional[bool] = None, plain: Optional[bool] = None,
                        compiler: Optional[bool] = None, linker: Optional[bool] = None, binary: Optional[bool] = None,
                        symlink: Optional[bool] = None, no_symlink: Optional[bool] = None,
                        file_exists: Optional[bool] = None, file_not_exists: Optional[bool] = None, dir_exists: Optional[bool] = None,
                        has_access: Optional[int] = None, negate: Optional[bool] = None,
                        at_source_root: Optional[bool] = None, not_at_source_root: Optional[bool] = None,
                        source_type: Optional[int] = None) -> libetrace.nfsdbFilteredOpensIter:
        """
        Function filters opens with given `file_filter` and set of global filter parameters  and returns opens objects iterator.

        Global filter switches are used before `file_filter`.

        :param file_filter: file filter object, defaults to None
        :type file_filter: Optional[List], optional
        :param path: match opens with given paths, defaults to None
        :type path: Optional[List[str]], optional
        :param has_path: _description_, defaults to None
        :type has_path: Optional[str], optional
        :param wc: match opens with wildcard, defaults to None
        :type wc: Optional[str], optional
        :param re: match opens with regex, defaults to None
        :type re: Optional[str], optional
        :param compiled: returns opens that has been use in compilation, defaults to None
        :type compiled: Optional[bool], optional
        :param linked: returns opens that has been use in linking, defaults to None
        :type linked: Optional[bool], optional
        :param linked_static: returns opens that has been use in linking static module, defaults to None
        :type linked_static: Optional[bool], optional
        :param linked_shared: returns opens that has been use in linking shared module, defaults to None
        :type linked_shared: Optional[bool], optional
        :param linked_exe: returns opens that has been use in linking executable module, defaults to None
        :type linked_exe: Optional[bool], optional
        :param plain: returns opens that wasn't used in any linking or compilation, defaults to None
        :type plain: Optional[bool], optional
        :param compiler: returns opens which path is compilers, defaults to None
        :type compiler: Optional[bool], optional
        :param linker: returns opens which path is linkers, defaults to None
        :type linker: Optional[bool], optional
        :param binary: returns opens which path is binary file, defaults to None
        :type binary: Optional[bool], optional
        :param symlink: returns opens which path is symlink, defaults to None
        :type symlink: Optional[bool], optional
        :param no_symlink: returns opens which path is not symlink, defaults to None
        :type no_symlink: Optional[bool], optional
        :param file_exists: returns opens which path exists while building database, defaults to None
        :type file_exists: Optional[bool], optional
        :param file_not_exists: returns opens which path did not exists while building database, defaults to None
        :type file_not_exists: Optional[bool], optional
        :param dir_exists: returns opens which path is dir and exists while building database, defaults to None
        :type dir_exists: Optional[bool], optional
        :param has_access: returns opens which where opened with given access type, defaults to None
        :type has_access: Optional[int], optional
        :param negate: negate global switches, defaults to None
        :type negate: Optional[bool], optional
        :param at_source_root: returns opens with source root in begining of path, defaults to None
        :type at_source_root: Optional[bool], optional
        :param not_at_source_root: returns opens without source root in begining of path, defaults to None
        :type not_at_source_root: Optional[bool], optional
        :param source_type: returns opens with path that was compiled and matching given source type, defaults to None
        :type source_type: Optional[int], optional
        :return: open objects iterator
        :rtype: libetrace.nfsdbFilteredOpensIter
        """
        return self.db.filtered_opens_iter(file_filter=file_filter, path=path, has_path=has_path, wc=wc, re=re, compiled=compiled,
                                        linked=linked, linked_static=linked_static, linked_shared=linked_shared,
                                        linked_exe=linked_exe, plain=plain, compiler=compiler,linker=linker, binary=binary, symlink=symlink,
                                        no_symlink=no_symlink, file_exists=file_exists, file_not_exists=file_not_exists, dir_exists=dir_exists,
                                        has_access=has_access, negate=negate, at_source_root=at_source_root, not_at_source_root=not_at_source_root, source_type=source_type)

    def filtered_execs(self, exec_filter: Optional[List]=None, bins: Optional[List[str]] = None, pids: Optional[List[int]] = None,
                        cwd_has_str: Optional[str] = None, cwd_wc: Optional[str] = None, cwd_re: Optional[str] = None,
                        cmd_has_str: Optional[str] = None, cmd_wc: Optional[str] = None, cmd_re: Optional[str] = None,
                        bin_has_str: Optional[str] = None, bin_wc: Optional[str] = None, bin_re: Optional[str] = None,
                        has_ppid: Optional[int] = None, has_command: Optional[bool] = None, has_comp_info: Optional[bool] = None,
                        has_linked_file:Optional[bool] = None, negate: Optional[bool] = None,
                        bin_at_source_root: Optional[bool] = None, bin_not_at_source_root: Optional[bool] = None,
                        cwd_at_source_root: Optional[bool] = None, cwd_not_at_source_root: Optional[bool] = None) -> List[libetrace.nfsdbEntry]:
        """
        Function filters execs with given `exec_filter` and set of global filter parameters and returns list of execs objects.

        Global filter switches are used before `exec_filter`.

        :param exec_filter: exec filter object, defaults to None
        :type exec_filter: Optional[List], optional
        :param bins: return execs with given bins, defaults to None
        :type bins: Optional[List[str]], optional
        :param pids: return execs with given pids, defaults to None
        :type pids: Optional[List[int]], optional
        :param cwd_has_str: return execs which cwd contains given path, defaults to None
        :type cwd_has_str: Optional[str], optional
        :param cwd_wc: return execs which cwd matches given wildcard, defaults to None
        :type cwd_wc: Optional[str], optional
        :param cwd_re: return execs which cwd matches given regex, defaults to None
        :type cwd_re: Optional[str], optional
        :param cmd_has_str: return execs which cmd contains given path, defaults to None
        :type cmd_has_str: Optional[str], optional
        :param cmd_wc: return execs which cmd matches given wildcard, defaults to None
        :type cmd_wc: Optional[str], optional
        :param cmd_re: return execs which cmd matches given regex, defaults to None
        :type cmd_re: Optional[str], optional
        :param bin_has_str: return execs which bin contains given path, defaults to None
        :type bin_has_str: Optional[str], optional
        :param bin_wc: return execs which bin matches given wildcard, defaults to None
        :type bin_wc: Optional[str], optional
        :param bin_re: return execs which bin matches given regex, defaults to None
        :type bin_re: Optional[str], optional
        :param has_ppid: return execs with given parent pids, defaults to None
        :type has_ppid: Optional[int], optional
        :param has_command: return execs that are generic commands, defaults to None
        :type has_command: Optional[bool], optional
        :param has_comp_info: return execs that are compilations, defaults to None
        :type has_comp_info: Optional[bool], optional
        :param has_linked_file: return execs that are linkers, defaults to None
        :type has_linked_file: Optional[bool], optional
        :param negate: negate global switches, defaults to None
        :type negate: Optional[bool], optional
        :param bin_at_source_root: return execs which bin starts with source root, defaults to None
        :type bin_at_source_root: Optional[bool], optional
        :param bin_not_at_source_root: return execs which bin does not starts with source root, defaults to None
        :type bin_not_at_source_root: Optional[bool], optional
        :param cwd_at_source_root: return execs which cwd starts with source root, defaults to None
        :type cwd_at_source_root: Optional[bool], optional
        :param cwd_not_at_source_root: return execs which cwd does not starts with source root, defaults to None
        :type cwd_not_at_source_root: Optional[bool], optional
        :return: list of execs objects
        :rtype: List[libetrace.nfsdbEntry]
        """
        return self.db.filtered_execs(exec_filter=exec_filter, bins=bins, pids=pids, cwd_has_str=cwd_has_str, cwd_wc=cwd_wc, cwd_re=cwd_re,
                                    cmd_has_str=cmd_has_str, cmd_wc=cmd_wc, cmd_re=cmd_re, bin_has_str=bin_has_str, bin_wc=bin_wc, bin_re=bin_re,
                                    has_ppid=has_ppid, has_command=has_command, has_comp_info=has_comp_info, has_linked_file=has_linked_file,negate=negate,
                                    bin_at_source_root=bin_at_source_root,bin_not_at_source_root=bin_not_at_source_root,
                                    cwd_at_source_root=cwd_at_source_root,cwd_not_at_source_root=cwd_not_at_source_root)

    def filtered_execs_iter(self, exec_filter:Optional[List]=None, bins: Optional[List[str]] = None, pids: Optional[List[int]] = None,
                        cwd_has_str: Optional[str] = None, cwd_wc: Optional[str] = None, cwd_re: Optional[str] = None,
                        cmd_has_str: Optional[str] = None, cmd_wc: Optional[str] = None, cmd_re: Optional[str] = None,
                        bin_has_str: Optional[str] = None, bin_wc: Optional[str] = None, bin_re: Optional[str] = None,
                        has_ppid: Optional[int] = None, has_command: Optional[bool] = None, has_comp_info: Optional[bool] = None,
                        has_linked_file:Optional[bool] = None, negate: Optional[bool] = None,
                        bin_at_source_root: Optional[bool] = None, bin_not_at_source_root: Optional[bool] = None,
                        cwd_at_source_root: Optional[bool] = None, cwd_not_at_source_root: Optional[bool] = None) -> libetrace.nfsdbFilteredCommandsIter:
        """
        Function filters execs with given `exec_filter` and set of global filter parameters and returns execs objects iterator.

        Global filter switches are used before `exec_filter`.

        :param exec_filter: exec filter object, defaults to None
        :type exec_filter: Optional[List], optional
        :param bins: return execs with given bins, defaults to None
        :type bins: Optional[List[str]], optional
        :param pids: return execs with given pids, defaults to None
        :type pids: Optional[List[int]], optional
        :param cwd_has_str: return execs which cwd contains given path, defaults to None
        :type cwd_has_str: Optional[str], optional
        :param cwd_wc: return execs which cwd matches given wildcard, defaults to None
        :type cwd_wc: Optional[str], optional
        :param cwd_re: return execs which cwd matches given regex, defaults to None
        :type cwd_re: Optional[str], optional
        :param cmd_has_str: return execs which cmd contains given path, defaults to None
        :type cmd_has_str: Optional[str], optional
        :param cmd_wc: return execs which cmd matches given wildcard, defaults to None
        :type cmd_wc: Optional[str], optional
        :param cmd_re: return execs which cmd matches given regex, defaults to None
        :type cmd_re: Optional[str], optional
        :param bin_has_str: return execs which bin contains given path, defaults to None
        :type bin_has_str: Optional[str], optional
        :param bin_wc: return execs which bin matches given wildcard, defaults to None
        :type bin_wc: Optional[str], optional
        :param bin_re: return execs which bin matches given regex, defaults to None
        :type bin_re: Optional[str], optional
        :param has_ppid: return execs with given parent pids, defaults to None
        :type has_ppid: Optional[int], optional
        :param has_command: return execs that are generic commands, defaults to None
        :type has_command: Optional[bool], optional
        :param has_comp_info: return execs that are compilations, defaults to None
        :type has_comp_info: Optional[bool], optional
        :param has_linked_file: return execs that are linkers, defaults to None
        :type has_linked_file: Optional[bool], optional
        :param negate: negate global switches, defaults to None
        :type negate: Optional[bool], optional
        :param bin_at_source_root: return execs which bin starts with source root, defaults to None
        :type bin_at_source_root: Optional[bool], optional
        :param bin_not_at_source_root: return execs which bin does not starts with source root, defaults to None
        :type bin_not_at_source_root: Optional[bool], optional
        :param cwd_at_source_root: return execs which cwd starts with source root, defaults to None
        :type cwd_at_source_root: Optional[bool], optional
        :param cwd_not_at_source_root: return execs which cwd does not starts with source root, defaults to None
        :type cwd_not_at_source_root: Optional[bool], optional
        :return: execs objects iterator
        :rtype: libetrace.nfsdbFilteredCommandsIter
        """
        return self.db.filtered_execs_iter(exec_filter=exec_filter, bins=bins, pids=pids, cwd_has_str=cwd_has_str, cwd_wc=cwd_wc, cwd_re=cwd_re,
                                    cmd_has_str=cmd_has_str, cmd_wc=cmd_wc, cmd_re=cmd_re, bin_has_str=bin_has_str, bin_wc=bin_wc, bin_re=bin_re,
                                    has_ppid=has_ppid, has_command=has_command, has_comp_info=has_comp_info, has_linked_file=has_linked_file,negate=negate,
                                    bin_at_source_root=bin_at_source_root,bin_not_at_source_root=bin_not_at_source_root,
                                    cwd_at_source_root=cwd_at_source_root,cwd_not_at_source_root=cwd_not_at_source_root)

    def opens_num(self) -> int:
        """
        Function returns opens count.

        :return: opens count
        :rtype: int
        """
        return len(self.db.opens_iter())

    def execs_num(self):
        """
        Function returns execs count.

        :return: execs count
        :rtype: int
        """
        return len(self.db)

    def get_version(self) -> str:
        """
        Function returns version string (set in cache creation)

        :return: version string
        :rtype: str
        """
        return self.db.dbversion if self.db.dbversion else "_unset_"

    def get_opens_of_path(self, file_path: str) -> List[libetrace.nfsdbEntryOpenfile]:
        """
        Function returns opens objects with given path.

        :param file_path: file name
        :type file_path: str
        :return: list of opens objects
        :rtype: List[libetrace.nfsdbEntryOpenfile]
        """
        return self.db.filemap[file_path]

    def get_execs_using_binary(self, binary: str) -> List[libetrace.nfsdbEntry]:
        """
        Function returns execs objects with given binary path.

        :param binary: binary path
        :type binary: str
        :return: list of execs objects
        :rtype: List[libetrace.nfsdbEntry]
        """
        ret = []
        try:
            ret = self.db[binary]
        except Exception:
            pass
        return ret

    def get_compilations(self) -> Set[libetrace.nfsdbEntry]:
        """
        Function returns compiler execs.

        :return: compiler execs objects
        :rtype: Set[libetrace.nfsdbEntry]
        """
        return set(self.db.filtered_execs_iter(has_comp_info=True))

    def get_compiled_files(self) -> Set[libetrace.nfsdbEntryOpenfile]:
        """
        Function returns compiled opens.

        :return: compiled opens objects
        :rtype: Set[libetrace.nfsdbEntryOpenfile]
        """
        return { cfile
                for ent in self.get_compilations()
                for cfile in ent.compilation_info.files }

    @lru_cache(maxsize=1)
    def get_compiled_file_paths(self) -> Set[str]:
        """
        Function returns unique compiled file paths.

        :return: compiled paths
        :rtype: Set[str]
        """
        return { cfile
                for ent in self.get_compilations()
                for cfile in ent.compilation_info.file_paths }

    def get_linkers(self) -> Set[libetrace.nfsdbEntry]:
        """
        Function returns linker execs.

        :return: linker execs objects
        :rtype: Set[libetrace.nfsdbEntry]
        """
        return set(self.db.filtered_execs_iter(has_linked_file=True))

    def get_linked_files(self) -> Set[libetrace.nfsdbEntryOpenfile]:
        """
        Function returns linked opens.

        :return: linked opens objects
        :rtype: Set[libetrace.nfsdbEntryOpenfile]
        """
        return { ent.linked_file for ent in self.get_linkers() }

    @lru_cache(maxsize=1)
    def get_linked_file_paths(self) -> Set[str]:
        """
        Function returns unique linked file paths.

        :return: linked paths
        :rtype: Set[str]
        """
        return { ent.linked_path for ent in self.get_linkers() }

    def get_relative_path(self, file_path: str) -> str:
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

    def get_reverse_dependencies(self, file_paths: "str | List[str]", recursive=False) -> List[str]:
        """
        Function returns reverse dependencies of given file paths.

        :param file_paths: file paths
        :type file_paths: str | List[str]
        :param recursive: enable recursive processing, defaults to False
        :type recursive: bool, optional
        :return: reverse dependencies
        :rtype: List[str]
        """
        return list(self.db.rdeps(file_paths, recursive=recursive))

    def get_module_dependencies(self, module_paths:"List[DepsParam| str]", direct:bool=False) -> List[libetrace.nfsdbEntryOpenfile]:
        """
        Function gets precomputed module dependencies for given module path.

        :param module_path: extended path or simple string path
        :type module_path: DepsParam | str
        :param direct: direct flag
        :type direct: bool
        """
        paths:List[str] = []
        for m in module_paths:
            if isinstance(m, DepsParam):
                paths.append(m.file)
            else:
                paths.append(m)

        return self.db.mdeps(paths, direct=direct)

    def get_deps(self, epath: "DepsParam | str", direct_global=False, dep_graph=False, debug=False, debug_fd=False, use_pipes=False,
                wrap_deps=False, all_modules: Optional[List[str]] = None) -> Tuple[List[int], List[str], List[libetrace.nfsdbEntryOpenfile], Dict]:
        """
        Function calculates dependencies of given file.

        :param epath: extended path or simple string path
        :type epath: DepsParam | str
        :param direct_global: global direct flag
        :type direct_global: bool
        :param dep_graph: generate dependency graph
        :type dep_graph: bool
        :param debug: enable debug info about dependency generation arguments
        :type debug: bool
        :param debug_fd: enable debug info about process of dependency generation
        :type debug_fd: bool
        :param use_pipes: relation between process pipe will be respected
        :type use_pipes: bool
        :param wrap_deps: process wrappers (like bash) will be respected
        :type wrap_deps: bool
        :param all_modules: list of all modules - needed in direct deps generation
        :type all_modules: List[str]
        :return: Tuple with process id, list of paths, list of opens objects and optionaly dependency graph
        :rtype: Tuple[List[int],List[str],List[nfsdbEntryOpenfile],Dict]
        """

        direct = direct_global
        if direct and all_modules is None:
            all_modules = self.linked_module_paths()

        if isinstance(epath, str):  # simple path - no excludes
            excl_patterns, excl_commands, excl_commands_index = self.config.gen_excludes_for_path(epath)
            use_pipe_for_path = self.config.get_use_pipe_for_path(epath, use_pipes)
            if all_modules is not None:
                return self.db.fdeps(epath, debug=debug, debug_fd=debug_fd, use_pipes=use_pipe_for_path,
                                    wrap_deps=wrap_deps, direct=direct, dep_graph=dep_graph, exclude_patterns=excl_patterns,
                                    exclude_commands=excl_commands, exclude_commands_index=excl_commands_index, all_modules=all_modules)
            else:
                return self.db.fdeps(epath, debug=debug, debug_fd=debug_fd, use_pipes=use_pipe_for_path,
                                    wrap_deps=wrap_deps, direct=direct, dep_graph=dep_graph, exclude_patterns=excl_patterns,
                                    exclude_commands=excl_commands, exclude_commands_index=excl_commands_index)
        else:
            if epath.direct is not None:  # If extended path has direct it will overwrite global direct args
                direct = epath.direct

            if direct and all_modules is None:
                all_modules = self.linked_module_paths()

            excl_patterns, excl_commands, excl_commands_index = self.config.gen_excludes_for_path(epath.file)
            use_pipe_for_path = self.config.get_use_pipe_for_path(epath.file, use_pipes)
            for e_c in epath.exclude_cmd:
                excl_commands.append(e_c)

            for e_p in epath.exclude_pattern:
                excl_patterns.append(e_p)

            if all_modules is not None:
                return self.db.fdeps(epath.file, debug=debug, debug_fd=debug_fd, use_pipes=use_pipe_for_path,
                                    wrap_deps=wrap_deps, direct=direct, dep_graph=dep_graph, exclude_patterns=excl_patterns,
                                    exclude_commands=excl_commands, negate_pattern=epath.negate_pattern,
                                    exclude_commands_index=excl_commands_index, all_modules=all_modules)
            else:
                return self.db.fdeps(epath.file, debug=debug, debug_fd=debug_fd, use_pipes=use_pipe_for_path,
                                    wrap_deps=wrap_deps, direct=direct, dep_graph=dep_graph, exclude_patterns=excl_patterns,
                                    exclude_commands=excl_commands, negate_pattern=epath.negate_pattern,
                                    exclude_commands_index=excl_commands_index)

    def get_multi_deps_cached(self, epaths:"List[DepsParam|str]", direct_global=False) -> List[libetrace.nfsdbEntryOpenfile]:
        """
        Function returns cached list of open files that are dependencies of given file(s).

        :param epaths: extended path or simple string path
        :type epaths: List[DepsParam] | List[str]
        :param direct_global: global direct flag, defaults to False
        :type direct_global: bool, optional
        :return: list of dependency opens
        :rtype: List[List[libetrace.nfsdbEntryOpenfile]]
        """
        return self.get_module_dependencies(epaths, direct=direct_global)

    def get_multi_deps(self, epaths:"List[DepsParam|str]", direct_global:bool=False, dep_graph:bool=False,
                        debug:bool=False, debug_fd:bool=False, use_pipes:bool=False, wrap_deps:bool=False) -> List[libetrace.nfsdbEntryOpenfile]:
        """
        Function returns list of open files that are dependencies of given file path(s).

        :param epaths: extended path or simple string path
        :type epaths: List[DepsParam] | List[str]
        :param direct_global: global direct flag, defaults to False
        :type direct_global: bool, optional
        :param dep_graph: generate dependency graph, defaults to False
        :type dep_graph: bool, optional
        :param debug: enable debug info about dependency generation arguments
        :type debug: bool
        :param debug_fd: enable debug info about process of dependency generation
        :type debug_fd: bool
        :param use_pipes: relation between process pipe will be respected
        :type use_pipes: bool
        :param wrap_deps: process wrappers (like bash) will be respected
        :type wrap_deps: bool
        :return: list of dependency opens
        :rtype: List[List[libetrace.nfsdbEntryOpenfile]]
        """
        ret = []
        for epath in epaths:
            ret += self.get_deps(epath, direct_global=direct_global, dep_graph=dep_graph,
                                    debug=debug, debug_fd=debug_fd, use_pipes=use_pipes, wrap_deps=wrap_deps)[2]
        return ret

    def get_dependency_graph(self, epaths:"List[DepsParam|str]", direct_global=False,
                                debug=False, debug_fd=False, use_pipes=False, wrap_deps=False) -> List[Tuple]:
        """
        Function returns list of dependency graph of given file path(s).

        :param epaths: extended path or simple string path
        :type epaths: List[DepsParam|str]
        :param direct_global: global direct flag, defaults to False
        :type direct_global: bool, optional
        :param debug: enable debug info about dependency generation arguments
        :type debug: bool
        :param debug_fd: enable debug info about process of dependency generation
        :type debug_fd: bool
        :param use_pipes: relation between process pipe will be respected
        :type use_pipes: bool
        :param wrap_deps: process wrappers (like bash) will be respected
        :type wrap_deps: bool
        :return: dependency graph
        :rtype: List[Tuple]
        """
        ret = {}

        def add_cmds_to_pids(val):
            for i, eid in enumerate(val[0]):
                val[0][i] = val[0][i] + ([" ".join(exe.argv) for exe in self.get_entries_with_pid(eid[0]) if len(exe.argv) > 0],)
            return val

        for epath in epaths:
            deps = self.get_deps(epath, direct_global=direct_global, dep_graph=True, debug=debug, debug_fd=debug_fd, use_pipes=use_pipes, wrap_deps=wrap_deps)[3]
            ret.update({k: add_cmds_to_pids(v) for k, v in deps.items()})
        return [x for x in ret.items()]

    def get_rdm(self, file_paths: List[str], recursive=False, sort=False, reverse=False) -> List:
        """
        Function returns list of reverse dependencies of given file path(s).

        :param file_paths: path list
        :type file_paths: List[str]
        :param recursive: enable recursive processing, defaults to False
        :type recursive: bool, optional
        :param sort: sort returned values, defaults to False
        :type sort: bool, optional
        :param reverse: sort in reversed order, defaults to False
        :type reverse: bool, optional
        :return: reverse dependency list
        :rtype: List
        """
        ret = []
        for dep_path in file_paths:
            rdep = self.get_reverse_dependencies(dep_path, recursive)
            if sort:
                ret.append([dep_path, sorted(list(rdep), reverse=reverse)])
            else:
                ret.append([dep_path, list(rdep)])
        return ret


    def get_cdm(self, file_paths: List[str], cdm_exclude_patterns:Optional[List]=None, cdm_exclude_files:Optional[List]=None,
                recursive=False, sort=False, reverse=False) -> List:
        """
        Function returs compiled dependencies of given file path(s).

        :param file_paths: path list
        :type file_paths: List[str]
        :param cdm_exclude_patterns: patterns used to stop processing of deps generation , defaults to None
        :type cdm_exclude_patterns: Optional[List], optional
        :param cdm_exclude_files:  file path list used to stop processing of deps generation, defaults to None
        :type cdm_exclude_files: Optional[List], optional
        :param recursive: enable recursive processing, defaults to False
        :type recursive: bool, optional
        :param sort: sort returned values, defaults to False
        :type sort: bool, optional
        :param reverse: sort in reversed order, defaults to False
        :type reverse: bool, optional
        :return: list of compiled dependencies
        :rtype: List
        """
        lm = self.linked_module_paths()
        ret = []
        for dep_path in file_paths:
            if dep_path in lm:
                excl_patterns, excl_commands, excl_commands_index = self.config.gen_excludes_for_path(dep_path)
                use_pipe_for_path = self.config.get_use_pipe_for_path(dep_path, False)
                if cdm_exclude_patterns is not None and isinstance(cdm_exclude_patterns, list):
                    for ex in cdm_exclude_patterns:
                        excl_commands.append(ex)

                if cdm_exclude_files is not None and isinstance(cdm_exclude_files, list):
                    for ex in cdm_exclude_files:
                        excl_patterns.append(ex)

                if cdm_exclude_patterns or cdm_exclude_files:
                    deps = [ dep for dep in self.db.fdeps(dep_path, exclude_patterns=excl_patterns,
                                        exclude_commands=excl_commands, exclude_commands_index=excl_commands_index,
                                        recursive=recursive,use_pipes=use_pipe_for_path)[2]
                                        if dep.opaque is not None and dep.opaque.compilation_info is not None
                            ]
                else:
                    deps = [ dep for dep in self.db.mdeps(dep_path)
                                        if dep.opaque is not None and dep.opaque.compilation_info is not None
                            ]
                comps = {cf.path
                        for d in deps
                        for cf in d.opaque.compilation_info.files}

                if len(comps) > 0:
                    if sort:
                        ret.append([dep_path, sorted(list(comps), reverse=reverse)])
                    else:
                        ret.append([dep_path, list(comps)])
        return ret

    @staticmethod
    def parse_trace_to_json(tracer_db_filename, json_db_filename, threads=1, debug=False):
        """
        Wrapper function for `libetrace.parse_nfsdb` function that parses trace file into json file.

        :param tracer_db_filename: tracer file path
        :type tracer_db_filename: str
        :param json_db_filename: output json database file path
        :type json_db_filename: str
        :param debug: enable debug info output
        :type debug: bool
        """
        if debug:
            print("Before parse")
            print_mem_usage()
        libetrace.parse_nfsdb(tracer_db_filename, json_db_filename, '-j'+str(threads))
        if debug:
            print("After parse")
            print_mem_usage()

    @staticmethod
    def get_src_root_from_tracefile(filename):
        """
        Function looks up for initial working directory stored in first line of trace file.

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
    def create_db_image(json_db_filename: str, src_root: str, set_version: str, exclude_command_patterns: List[str], shared_argvs: List[str], cache_db_filename: str, debug=False) -> bool:
        """
        Function creates cached database image from json database.

        :param json_db_filename: json database file path
        :type json_db_filename: str
        :param src_root: database source root - directory from where tracing process started
        :type src_root: str
        :param set_version: database version meta information
        :type set_version: str
        :param exclude_command_patterns: list of excluded command patterns
        :type exclude_command_patterns: List[str]
        :param shared_argvs: list of shared library generation switches to be searched in linking command
        :type shared_argvs: List[str]
        :param cache_db_filename: output database file name
        :type cache_db_filename: str
        :param debug: enable debug info output
        :type debug: bool
        :return: True if `libetrace.create_nfsdb` succeds otherwise False
        :rtype: bool
        """
        with open(json_db_filename, "rb") as fil:
            print_mem_usage(debug, "Before load")
            json_db = json.load(fil)
            if len(json_db) > 0:
                json_db[0]["r"]["p"] = 0  # fix parent process
            print_mem_usage(debug, "After load / before create_nfsdb")
            r = libetrace.create_nfsdb(json_db, src_root, set_version, exclude_command_patterns, shared_argvs, cache_db_filename)
            print_mem_usage(debug, "After create_nfsdb")
            return r

    def create_deps_db_image(self, deps_cache_db_filename: str, depmap_filename: str, ddepmap_filename: str, use_pipes:bool, wrap_deps:bool,
                            jobs: int = multiprocessing.cpu_count(), deps_threshold: int = 90000, debug: bool = False):
        """
        Function calculates all modules dependencies and create dependencies image.

        :param deps_cache_db_filename: output dependencies image path
        :type deps_cache_db_filename: str
        :param depmap_filename: intermediate dependencies file path
        :type depmap_filename: str
        :param ddepmap_filename: intermediate direct dependencies file path
        :type ddepmap_filename: str
        :param use_pipes: enable generation dependencies with piped process
        :type use_pipes: bool
        :param wrap_deps: enable generation dependencies with wrapping process
        :type wrap_deps: bool
        :param jobs: number of multiprocessing threads, defaults to multiprocessing.cpu_count()
        :type jobs: int, optional
        :param deps_threshold: max dependencies count - used to detect dependency generation issues, defaults to 90000
        :type deps_threshold: int, optional
        :param debug: enable debug info about dependency generation arguments, defaults to False
        :type debug: bool, optional
        """

        all_modules = sorted([e.linked_path for e in self.filtered_execs_iter(has_linked_file=True) if not e.linked_path.startswith("/dev/")])

        print("Number of all linked modules: %d" % (len(all_modules)))

        if len(all_modules) == 0:
            print("No linked modules found! Check linker patterns in config.")
            exit(0)

        def get_module_dependencies(module_path, dm, done_modules, all_mods):
            linked_modules = [x[2] for x in dm[module_path] if x[2] in all_mods]
            ret = set(linked_modules)
            for module_path in linked_modules:
                if module_path not in done_modules:
                    done_modules.add(module_path)
                    ret |= get_module_dependencies(module_path, dm, done_modules, all_mods)
            return ret

        global calc_deps

        def calc_deps(module_path, direct=True, all_mods=all_modules):
            ret = self.get_deps(module_path, direct_global=direct, all_modules=all_mods, use_pipes=use_pipes, wrap_deps=wrap_deps)
            # print("[%d / %d] %s %s_deps(%d)" % (processed.value, len(allm), module_path, "direct" if direct is True else "full", len(ret[2])), flush=True)
            with processed.get_lock():
                processed.value += 1
                pbar.n = processed.value
                pbar.refresh()
            return module_path, [(x.ptr[0], x.ptr[1], x.path) for x in ret[2]]

        pbar = progressbar(total=len(all_modules), disable=None)
        processed = multiprocessing.Value('i', 0)
        processed.value = 0

        pool = multiprocessing.Pool(jobs)
        results = pool.map(calc_deps, all_modules)
        pool.close()
        pool.join()

        pbar.n = processed.value
        pbar.refresh()
        pbar.close()

        depmap = {d[0]: d[1] for d in results}
        depmap_keys = list(depmap.keys())
        del results
        deplst = {k: len(v) for k, v in depmap.items()}
        print("")
        print("# Sorted list of dependencies count")
        err_count = 0

        for m, q in sorted(deplst.items(), key=lambda item: item[1], reverse=True):
            print("  %s: %d" % (m, q))
            if deps_threshold and q > int(deps_threshold):
                print("ERROR: Number of direct dependencies [%d] for linked module [%s] exceeds the threshold [%d]" % (q, m, int(deps_threshold)))
                err_count += 1

        global get_full_deps
        def get_full_deps(lm):
            return calc_deps(lm, direct=False, all_mods=depmap_keys)

        if err_count > 0:
            print("ERROR: Exiting due dependency mismatch errors (%d errors)" % err_count)
            sys.exit(err_count)

        print("Saving {} ...".format(ddepmap_filename))
        with open(ddepmap_filename, "w", encoding=sys.getfilesystemencoding()) as f:
            json.dump(obj=depmap, fp=f, indent=4)
        printdbg("written {}".format(ddepmap_filename), debug)

        # Compute full dependency list for all modules

        print("")
        print("Computing full dependency list for all modules...")

        pbar = progressbar(total=len(all_modules), disable=None)
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
        printdbg("Written {}".format(depmap_filename), debug)

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
        printdbg("Written {} ".format(deps_cache_db_filename), debug)

    @staticmethod
    @lru_cache(maxsize=1024)
    def is_integrated_clang_compiler(compiler_path: str, test_file: str) -> bool:
        """
        Function checks if provided compiler is integrated clang compiler.

        :param compiler_path: path to compiler binary
        :type compiler_path: str
        :param test_file: file path used for test
        :type test_file: str
        :return: True if compiler is intgrated otherwise False
        :rtype: bool
        """
        pn = subprocess.Popen([compiler_path, "-c", "-Wall", "-o", "/dev/null", test_file, "-###"], shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        out, err = pn.communicate()
        return " (in-process)" in out.decode("utf-8")

    @staticmethod
    @lru_cache(maxsize=1024)
    def have_integrated_cc1(compiler_path: str, is_integrated: bool, test_file: str) -> bool:
        """
        Function checks if provided compiler is integrated clang compiler and is_integrated parameter is True.

        :param compiler_path: path to compiler binary
        :type compiler_path: str
        :param is_integrated: external check - usually checking if "-fno-integrated-cc1" is not patr of args
        :type is_integrated: bool
        :param test_file: file path used for test
        :type test_file: str
        :return: True if compiler is integrated and is_integrate are True otherwise False
        :rtype: bool
        """
        return CASDatabase.is_integrated_clang_compiler(compiler_path, test_file) and is_integrated

    @staticmethod
    def clang_ir_generation(cmds: List[str]) -> bool:
        if "-x" in cmds:
            if cmds[cmds.index("-x")+1] == "ir":
                return True
        return False

    @staticmethod
    def clang_pp_input(cmds: List[str]) -> bool:
        if "-x" in cmds:
            if cmds[cmds.index("-x")+1] == "cpp-output":
                return True
        return False

    @staticmethod
    @lru_cache(maxsize=1024)
    def maybe_compiler_binary(binary_path_or_link: str) -> str:
        """
        Function ensures that given binary path is realpath

        :param binary_path_or_link: path to check
        :type binary_path_or_link: str
        :return: proper path
        :rtype: str
        """
        if binary_path_or_link.endswith("/cc") or binary_path_or_link.endswith("/c++"):
            return os.path.realpath(binary_path_or_link)
        else:
            return binary_path_or_link

    @staticmethod
    def read_path_file_info(path: str) -> Set[str]:
        if os.path.exists(path):
            path = os.path.realpath(path)
            pn = subprocess.Popen(["file", path], shell=False, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            out, _ = pn.communicate()
            return set([x.strip(",") for x in out.decode("utf-8").split()])
        else:
            return set()

    @staticmethod
    @lru_cache(maxsize=1024)
    def is_elf_executable(path: str) -> bool:
        """
        Function check if given path is elf binary

        :param path: path to check
        :type path: str
        :return: True if path leads to elf binary otherwise False
        :rtype: bool
        """
        r = CASDatabase.read_path_file_info(path)
        return "ELF" in r and "executable" in r

    @staticmethod
    @lru_cache(maxsize=1024)
    def is_elf_interpreter(path: str) -> bool:
        """
        Function check if given path is elf interpreter

        :param path: path to check
        :type path: str
        :return: True if path leads to elf binary otherwise False
        :rtype: bool
        """
        r = CASDatabase.read_path_file_info(path)
        return "ELF" in r and "interpreter" in r

    @staticmethod
    def get_effective_args(args: List[str], cwd: str) -> List[str]:
        if "@" in " ".join(args):
            ret = []
            for i, arg in enumerate(args):
                if arg.startswith("@"):
                    f = os.path.normpath(os.path.join(cwd, arg[1:]))
                    if os.path.isfile(f):
                        with open(f, "r") as arg_file:
                            r = arg_file.read()
                            ret.extend(r.split())
                    else:
                        print("Parsing args failed! {}".format(args))
                        print("Argfile does not exists {}".format(f))
                else:
                    ret.append(arg)
            return ret
        else:
            return args


    def post_process(self, json_db_filename, wrapping_bin_list: List[str], workdir,
                     process_linking=True, process_comp=True, process_rbm=True, process_pcp=True,
                     new_database=True, no_update=False, no_auto_detect_icc=False, max_chunk_size=sys.maxsize,
                     allow_pp_in_compilations=False, jobs=multiprocessing.cpu_count(),
                     debug_compilations=False, debug=False, verbose=False):

        start_time = time.time()
        total_start_time = time.time()

        intermediate_db = json_db_filename.replace(".nfsdb.json", ".nfsdb.img.tmp")
        if not os.path.exists(intermediate_db):
            print("NO intermediate database!")
            exit(2)
        self.load_db(intermediate_db, debug=False, quiet=False)
        print_mem_usage(debug, "Postprocess start")
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
        wr_map: Dict[int, Set[str]] = {}
        allow_llvm_bc = True
        gcc_comp_pids: List[int] = []
        gpp_comp_pids: List[int] = []
        integrated_clang_compilers: Set[str] = set()
        clang_execs: List[int] = []
        clangpp_execs: List[int] = []
        clang_pattern_match_execs: List[int] = []
        clangpp_pattern_match_execs: List[int] = []
        clangxx_input_execs = []
        gxx_input_execs = []
        comp_pids: Set[int] = set()
        gcc_spec_patterns = re.compile("|".join([fnmatch.translate(pattern) for pattern in self.config.gcc_spec]))
        gpp_spec_patterns = re.compile("|".join([fnmatch.translate(pattern) for pattern in self.config.gpp_spec]))
        clang_spec_patterns = re.compile("|".join([fnmatch.translate(pattern) for pattern in self.config.clang_spec]))
        clangpp_spec_patterns = re.compile("|".join([fnmatch.translate(pattern) for pattern in self.config.clangpp_spec]))
        cc1_patterns = re.compile("|".join([fnmatch.translate(pattern) for pattern in ["*cc1", "*cc1plus"]]))
        coll_patterns = re.compile("|".join([fnmatch.translate(pattern) for pattern in ["*/collect*"]]))
        comps_filename = os.path.join(workdir, ".nfsdb.comps.json")
        rbm_filename = os.path.join(workdir, ".nfsdb.rbm.json")
        pcp_filename = os.path.join(workdir, ".nfsdb.pcp.json")
        link_filename = os.path.join(workdir, ".nfsdb.link.json")
        manager = multiprocessing.Manager()
        out_compilation_info = manager.dict()  # we will need this for multiprocessing
        out_linked = dict()
        out_pcp_map = dict()
        out_rbm = dict()

        if do_rbm:
            if new_database or not os.path.exists(rbm_filename):
                start_time = time.time()

                def set_children_list(exe: libetrace.nfsdbEntry, ptree: Dict, ignore_binaries: Set[str]) -> None:
                    for chld in exe.childs:
                        if not ignore_binaries or not set([u.binary for u in self.get_entries_with_pid(exe.eid.pid)]) & ignore_binaries:
                            set_children_list(chld, ptree, ignore_binaries)
                    assert exe.eid.pid not in ptree, "process tree have entry for pid {}".format(exe.eid.pid)
                    if len(exe.childs) > 0:
                        ptree[exe.eid.pid] = [c.eid.pid for c in exe.childs]
                    else:
                        ptree[exe.eid.pid] = []

                def process_tree_for_pid(exe: libetrace.nfsdbEntry, ignore_binaries: Set[str]):
                    ptree = {}
                    set_children_list(exe, ptree, ignore_binaries)
                    return ptree

                def update_reverse_binary_mapping(reverse_binary_mapping: Dict[int, int], pid: int, ptree, root_pid, root_binary):
                    if pid in ptree:
                        for chld_pid in ptree[pid]:
                            if chld_pid in reverse_binary_mapping:
                                assert reverse_binary_mapping[chld_pid] == pid, "reverse binary mapping contains: {} (root: {})".format(chld_pid, root_pid)
                            else:
                                exelst = self.get_entries_with_pid(chld_pid)
                                if root_binary not in set([u.binary for u in exelst]):
                                    reverse_binary_mapping[chld_pid] = root_pid
                                update_reverse_binary_mapping(reverse_binary_mapping, chld_pid, ptree, root_pid, root_binary)

                def build_reverse_binary_mapping(bin_lst: List[str]) -> Dict[int, int]:
                    reverse_binary_mapping: Dict[int, int] = dict()
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
                    printdbg("Written generated {}".format(rbm_filename), debug)
                else:
                    print("WARNING: rbm map is empty! len==0")
                print("reverse binary mappings processed (%d binaries, %d entries)   [%.2fs]" % (len(wrapping_bin_list), len(out_rbm.keys()), time.time() - start_time))
                out_rbm = dict()

        if do_pcp:
            start_time = time.time()
            if new_database or not os.path.exists(pcp_filename):
                os.system('setterm -cursor off')
                pcp_map = self.db.precompute_command_patterns(self.config.string_table)
                os.system('setterm -cursor on')
                if pcp_map:
                    for k, v in pcp_map.items():
                        eid_lst = self.get_eid((k[0], k[1]))
                        assert len(eid_lst) == 1
                    out_pcp_map = {str(self.get_exec(k[0], k[1]).ptr): base64.b64encode(v).decode("utf-8") for k, v in pcp_map.items()}  # Can we use only other than "AA=="
                    del pcp_map
                else:
                    out_pcp_map = {}
                with open(pcp_filename, "w", encoding=sys.getfilesystemencoding()) as f:
                    f.write(json.dumps(out_pcp_map, indent=4))
                printdbg("Written generated {}".format(pcp_filename), debug)
                print("command patterns precomputed processed (%d patterns)  [%.2fs]" % (len(self.config.string_table), time.time()-start_time))
                out_pcp_map = dict()

        if do_linked:

            start_time = time.time()

            if new_database or not os.path.exists(link_filename):
                linked_pattern = re.compile("|".join([fnmatch.translate(pattern) for pattern in self.config.ld_spec]))
                ared_pattern = re.compile("|".join([fnmatch.translate(pattern) for pattern in self.config.ar_spec]))
                l_size = 0
                a_size = 0

                for e in self.db.iter():
                    if linked_pattern.match(e.binary):
                        effective_args: List[str] = self.get_effective_args(e.argv, e.cwd)
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
                                out_linked[e.ptr] = {"l": lnkm, "t": 1}
                                l_size += 1
                        elif "--output" in effective_args:
                            outidx = effective_args.index("--output")
                            if outidx < len(effective_args)-1:
                                assert e.ptr not in out_linked
                                out_linked[e.ptr] = {"l": os.path.normpath(os.path.join(e.cwd, effective_args[outidx+1])), "t": 1}
                                l_size += 1
                    if ared_pattern.match(e.binary):
                        effective_args: List[str] = self.get_effective_args(e.argv, e.cwd)
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
                                            out_linked[we_l[0].ptr] = {"l": os.path.normpath(os.path.join(e.cwd, armod[:-4])), "t": 0}
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
                printdbg("Written generated {}".format(link_filename), debug)
                print("computed linked modules (linked: %d, ared: %d)  [%.2fs]" % (l_size, a_size, time.time() - start_time))
                out_linked = dict()

        if do_compilations:


            diag_files={}
            if os.path.exists(".nfsdb.diags/"):
                shutil.move(".nfsdb.diags/",f".nfsdb.diags_{time.time()}/")

            def get_diag_file(exe_idx):
                if not os.path.exists(".nfsdb.diags/"):
                    os.makedirs(".nfsdb.diags", exist_ok= True)
                if exe_idx not in diag_files:
                    diag_files[exe_idx] = open(f".nfsdb.diags/diag_{exe_idx}.txt", "w")
                return diag_files[exe_idx]

            def printd(msg, exe_idx, stdout=False):
                print (msg, file=get_diag_file(exe_idx), flush=True)
                if stdout:
                    print (f"\n[.nfsdb.diags/diag_{exe_idx}.txt] {msg}", flush=True)

            def store_output(exe_idx, msg, cmd, stdout, stderr, ret, show_in_log=True):
                printd (msg, exe_idx, show_in_log)
                printd (f"cmd = {cmd}\n" + f"ret = {ret}\n" +
                f"---STDOUT----------------------------\n{stdout}\n" +
                f"---STDERR----------------------------\n{stderr}\n" +
                "-------------------------------------\n", exe_idx)

            start_time = time.time()
            libetrace_dir = os.path.dirname(os.path.realpath(__file__))

            compilation_start_time = time.time()

            if new_database or not os.path.exists(comps_filename):
                print_mem_usage(debug, "Before processing compilations")
                print("creating compilations input map ...")

                fork_map: Dict[int, Set[int]] = {}
                clang_input_execs = []
                clangpp_input_execs = []
                gcc_input_execs = []
                gpp_input_execs = []

                clangxx_compilers: Set[str] = set()

                pbar = progressbar(total=len(self.db), disable=None)

                # region execs collection
                for n, ex in enumerate(self.db):
                    pbar.n = n
                    pbar.refresh()
                    if ex.return_code != 0:
                        continue
                    written_paths = {op.path for op in ex.opens if op.mode & 3 >= 1}

                    if len(written_paths) > 0:
                        if ex.eid.pid in wr_map:
                            wr_map[ex.eid.pid].update(written_paths)
                        else:
                            wr_map[ex.eid.pid] = written_paths

                    c = {ch.eid.pid for ch in ex.childs}
                    if len(c) > 0:
                        if ex.eid.pid in fork_map:
                            fork_map[ex.eid.pid].update(c)
                        else:
                            fork_map[ex.eid.pid] = c

                    if do_compilations and ex.binary != '':
                        b = os.path.realpath(ex.binary) if ex.binary.endswith("/cc") or ex.binary.endswith("/c++") else ex.binary
                        if clangpp_spec_patterns.match(b):
                            effective_args: List[str] = self.get_effective_args(ex.argv, ex.cwd)
                            clangpp_pattern_match_execs.append(ex.ptr)
                            if ("-cc1" in effective_args or ((("-c" in effective_args) or ("-S" in effective_args)) and self.have_integrated_cc1(ex.binary, "-fno-integrated-cc1" not in effective_args, test_file))) \
                                and "-o" in effective_args and ("-emit-llvm-bc" not in effective_args or allow_llvm_bc) \
                                and not self.clang_ir_generation(effective_args) and not self.clang_pp_input(effective_args) \
                                and "-analyze" not in effective_args and ex.eid.pid not in comp_pids and comp_pids.add(ex.eid.pid) is None:
                                clangpp_execs.append(ex.ptr)
                                if not effective_args[-1] == "-":
                                    clangpp_input_execs.append(ex)
                                if self.have_integrated_cc1(ex.binary, "-fno-integrated-cc1" not in effective_args, test_file):
                                    integrated_clang_compilers.add(ex.binary)
                        elif clang_spec_patterns.match(b):
                            effective_args: List[str] = self.get_effective_args(ex.argv, ex.cwd)
                            clang_pattern_match_execs.append(ex.ptr)
                            if ("-cc1" in effective_args or ((("-c" in effective_args) or ("-S" in effective_args)) and self.have_integrated_cc1(ex.binary, "-fno-integrated-cc1" not in effective_args, test_file))) \
                                and "-o" in effective_args and ("-emit-llvm-bc" not in effective_args or allow_llvm_bc) \
                                and not self.clang_ir_generation(effective_args) and not self.clang_pp_input(effective_args) \
                                and "-analyze" not in effective_args and ex.eid.pid not in comp_pids and comp_pids.add(ex.eid.pid) is None:
                                clang_execs.append(ex.ptr)
                                if not effective_args[-1] == "-":
                                    clang_input_execs.append(ex)
                                if self.have_integrated_cc1(ex.binary, "-fno-integrated-cc1" not in effective_args, test_file):
                                    integrated_clang_compilers.add(ex.binary)
                        elif gcc_spec_patterns.match(b):
                            effective_args: List[str] = self.get_effective_args(ex.argv, ex.cwd)
                            gcc_comp_pids.append(ex.ptr)
                            if "-" not in effective_args:
                                gcc_input_execs.append(ex)
                        elif gpp_spec_patterns.match(b):
                            effective_args: List[str] = self.get_effective_args(ex.argv, ex.cwd)
                            gpp_comp_pids.append(ex.ptr)
                            if "-" not in effective_args:
                                gpp_input_execs.append(ex)

                print("input map created in [%.2fs]" % (time.time()-start_time))
                start_time = time.time()

                rev_fork_map: Dict[int, int] = {}

                for fork_pid in fork_map:
                    for child in fork_map[fork_pid]:
                        if child in rev_fork_map:
                            assert True, "Multiple parents for process %d" % child
                        else:
                            rev_fork_map[child] = fork_pid

                print("reverse fork map created in [%.2fs]" % (time.time()-start_time))

                start_time = time.time()
                print_mem_usage(debug)

                def failed():
                    with failed_comps.get_lock():
                        failed_comps.value += 1
                def skipped():
                    with skipped_comps.get_lock():
                        skipped_comps.value += 1

                computation_finished = multiprocessing.Value("b")
                computation_finished.value = False
                execs_to_process = len(clangpp_execs) + len(clang_execs) + len(gcc_input_execs) + len(gpp_input_execs)
                write_queue = multiprocessing.JoinableQueue(maxsize=execs_to_process)

                # region writer
                def writer():
                    printdbg("Starting writer process...", debug)
                    is_first = True
                    with open(comps_filename, "w") as of:
                        of.write("{\n")
                        while not computation_finished.value:
                            try:
                                rec:Dict = write_queue.get(timeout=5)
                                write_queue.task_done()
                                if not rec:
                                    continue
                                ptr = next(iter(rec.keys()))
                                if is_first:
                                    is_first = False
                                    of.write(f'"{ptr}": {json.dumps(rec[ptr])}')
                                else:
                                    of.write(",\n" + f'"{ptr}": {json.dumps(rec[ptr])}')
                            except Exception:
                                if computation_finished.value:
                                    break
                        of.write("}")
                    os.sync()
                    printdbg("Writer process ends", debug)

                writer_process = multiprocessing.Process(target=writer)
                writer_process.start()
                # region clang compilations
                if True:  
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

                    clangxx_input_execs: List[Tuple] = [
                        (xT[0].ptr, xT[1])
                        for xT in [(x, 1) for x in clang_input_execs] + [(x, 2) for x in clangpp_input_execs]
                        if os.path.join(xT[0].cwd, self.maybe_compiler_binary(xT[0].binary)) in clangxx_compilers
                    ]

                    del clang_input_execs
                    del clangpp_input_execs
                    del clangxx_compilers

                    if len(clangxx_input_execs) > 0:

                        clang_c = clang.clang(verbose, debug, clang_compilers, clangpp_compilers, integrated_clang_compilers, debug_compilations, allow_pp_in_compilations)
                        
                        #region clang_executor
                        def clang_executor(worker_idx):
                            worker = exec_worker.ExecWorker(os.path.join(libetrace_dir, "worker"))

                            printdbg("Worker {} starting...".format(worker_idx), debug)

                            while True:
                                with cur_iter.get_lock():
                                    if cur_iter.value < max_iter:
                                        qpos = cur_iter.value
                                        cur_iter.value += 1
                                    else:
                                        break
                                with processed.get_lock():
                                    processed.value += 1
                                    pbar.n = processed.value
                                    pbar.refresh()
                                ptr, compiler_type = clangxx_input_execs[qpos]
                                exe:libetrace.nfsdbEntry = self.get_exec_at_pos(ptr)

                                if not clang_c.quickcheck(exe.binary, exe.argv):
                                    skipped()
                                    continue

                                argv: List[str] = exe.argv.copy()

                                try:
                                    have_int_cc1 = self.have_integrated_cc1(os.path.join(exe.cwd, exe.binary), "-fno-integrated-cc1" not in argv, test_file)
                                except FileNotFoundError:
                                    failed()
                                    continue
                                
                                try:
                                    stdout0, stderr0, ret_code0 = worker.runCmd(exe.cwd, exe.binary, argv[1:] + ["-###"])
                                except exec_worker.ExecWorkerException:
                                    worker.initialize()
                                    print("Exception while running -###")
                                    print ("[%s] %s" % (exe.cwd, " ".join(argv[1:] + ["-###"])))
                                    failed()
                                    continue

                                lns = stderr0.splitlines()
                                idx = [k for k, u in enumerate(lns) if "(in-process)" in u]
                                if idx and len(lns) >= idx[0] + 2:
                                    ncmd = lns[idx[0] + 1]
                                    try:
                                        argv = [
                                                    u[1:-1].encode('latin1').decode('unicode-escape').encode('latin1').decode('utf-8')
                                                    for u in libetrace.parse_compiler_triple_hash(ncmd)
                                                ]
                                        have_int_cc1 = 1
                                    except Exception:
                                        print(exe.json(), flush=True)
                                        print(lns, flush=True)
                                        print(ncmd, flush=True)

                                if "-cc1as" in argv:
                                    skipped()
                                    # Remove clang invocations with -cc1as (it's not actual C/C++ compilation which generates errors later)
                                    continue

                                arg_fn = clang_c.extract_comp_file(argv, exe.cwd, self.config.clang_tailopts)
                                if os.path.isabs(arg_fn):
                                    fn = arg_fn
                                else:
                                    fn = os.path.join(exe.cwd, arg_fn)

                                if not os.path.exists(fn):
                                    if "tmp" not in fn:
                                        printd (f"Error: {fn} - does not exist", ptr, True)
                                        printd (f"Original cmd: {exe.argv}", ptr)
                                        failed()
                                    else:
                                        skipped()
                                    continue
                                # fn - the path to the compiled file that exists
                                try:
                                    argv = clang_c.fix_argv(argv, compiler_type, arg_fn)
                                except IndexError:
                                    store_output(ptr, "Parameter error - no -o arg ", argv, stdout0, stderr0, ret_code0)
                                    failed()
                                    continue

                                try:
                                    stdout1, stderr1, ret_code1 = worker.runCmd(exe.cwd, exe.binary, argv[1:], "")  # last parameter is empty stdin for clang
                                    if ret_code1 != 0 and debug:
                                        print(f"[ERROR] - running \ncwd: {exe.cwd} \nbin: {exe.binary} \nargs: {argv[1:]}\nstdout:\n {stdout1}\nstderr:\n {stderr1}", flush=True)

                                except exec_worker.ExecWorkerException as e:
                                    worker.initialize()
                                    printd (f"Failed to process defs from clang output.", ptr, True)
                                    printd (f"cmd = {argv}", ptr)
                                    printd (f"err = {e}", ptr)
                                    failed()                                    
                                    continue

                                if not clang_c.allow_pp_in_compilations and '-E' in exe.argv:
                                    store_output(ptr, "PP in compilations are not allowed", argv, stdout1, stderr1, ret_code1, show_in_log=False)
                                    skipped()
                                    continue

                                compiler_path = os.path.join(exe.cwd, self.maybe_compiler_binary(exe.binary))
                                comp_objs = clang_c.get_object_files(exe.eid.pid, have_int_cc1, fork_map, rev_fork_map, wr_map)

                                try:
                                    includes, defs, undefs = clang_c.parse_defs(stderr1.splitlines(), stdout1.splitlines())
                                    includes = [os.path.realpath(os.path.normpath(os.path.join(exe.cwd, x))) for x in includes]
                                    ipaths = clang_c.compiler_include_paths(compiler_path)
                                    for u in ipaths:
                                        if u not in includes:
                                            includes.append(u)
                                    ifiles = clang_c.parse_include_files(exe.argv, exe.cwd, includes)
                                except clang.IncludeParseException as e:
                                    store_output(ptr, "Failed to process defs from clang output",argv, stdout1, stderr1, ret_code1)
                                    failed()
                                    continue

                                src_type = clang_c.get_source_type(argv, compiler_type, os.path.splitext(fn)[1])
                                absfn = os.path.realpath(os.path.normpath(os.path.join(exe.cwd, fn)))

                                if absfn.startswith("/dev/"):
                                    printdbg("\nSkipping bogus file {} ".format(absfn), debug)
                                    skipped()
                                    continue
                                write_queue.put({ptr: {
                                        "f": [absfn],
                                        "i": includes,
                                        "d": [{"n": di[0], "v":di[1]} for di in defs],
                                        "u": [{"n": ui[0], "v":ui[1]} for ui in undefs],
                                        "h": ifiles,
                                        "s": src_type,
                                        "o": comp_objs,
                                        "p": have_int_cc1
                                    }},True,5)
                                with found_comps.get_lock():
                                    found_comps.value += 1

                        print("Searching for clang compilations ... (%d candidates; %d jobs)" % (len(clangxx_input_execs), jobs))
                        print_mem_usage(debug)
                        print(flush=True)

                        pbar = progressbar(total=len(clangxx_input_execs), disable=None)
                        processed = multiprocessing.Value("i")
                        processed.value = 0
                        found_comps = multiprocessing.Value("i")
                        found_comps.value = 0
                        failed_comps = multiprocessing.Value("i")
                        failed_comps.value = 0           
                        skipped_comps = multiprocessing.Value("i")
                        skipped_comps.value = 0                              
                        cur_iter = multiprocessing.Value("i")
                        cur_iter.value = 0          
                        max_iter = len(clangxx_input_execs)              
                        sstart = time.time()
                        procs = []
                        for i in range(jobs):
                            p = multiprocessing.Process(target=clang_executor, args=(i,))
                            procs.append(p)
                            p.start()

                        for proc in procs:
                            proc.join()

                        pbar.n = processed.value
                        pbar.refresh()
                        pbar.close()

                        print(flush=True)
                        print(f"Workers finished in {time.time() - sstart:.2f}s - found={found_comps.value} failed={failed_comps.value} skipped={skipped_comps.value} compilations.")
                        if (found_comps.value + failed_comps.value + skipped_comps.value) != len(clangxx_input_execs):
                            print ("ERROR: Found + skipped + error != all.")
                        print_mem_usage(debug)

                    print("finished searching for clang compilations [%.2fs]" % (time.time()-start_time))

                if True:  # gcc compilations section
                    start_time = time.time()
                    gcc_compilers = [u for u in list(set([os.path.join(x.cwd, self.maybe_compiler_binary(x.binary)) for x in gcc_input_execs])) if self.is_elf_executable(u)]
                    gpp_compilers = [u for u in list(set([os.path.join(x.cwd, self.maybe_compiler_binary(x.binary)) for x in gpp_input_execs])) if self.is_elf_executable(u)]
                    gxx_compilers = set(gcc_compilers + gpp_compilers)

                    gxx_input_execs: List[Tuple] = [(xT[0].ptr, xT[1])
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

                        gcc_c = gcc.gcc(verbose, debug, gcc_compilers, gpp_compilers, debug_compilations)

                        plugin_insert = "-fplugin=" + os.path.realpath(os.path.join(libetrace_dir, "libgcc_input_name.so"))

                        input_compiler_path = None # TODO -make parameter of this!
                        #region gcc_executor
                        def gcc_executor(worker_idx):
                            worker = exec_worker.ExecWorker(os.path.join(libetrace_dir, "worker"))
                            printdbg("Worker {} starting...".format(worker_idx),debug)

                            while True:
                                with cur_iter.get_lock():
                                    if cur_iter.value < max_iter:
                                        qpos = cur_iter.value
                                        cur_iter.value += 1
                                    else:
                                        break
                                with processed.get_lock():
                                    processed.value += 1
                                    pbar.n = processed.value
                                    pbar.refresh()
                                ptr, compiler_type = gxx_input_execs[qpos]
                                exe:libetrace.nfsdbEntry = self.get_exec_at_pos(ptr)
                                argv: List[str] = exe.argv.copy()
                                bin = exe.binary
                                cwd = exe.cwd
                                # GET TRIPLE HASH
                                if len(argv) == 2 and (argv[1] == "--version" or argv[1] == "-dumpmachine" or argv[1] == "-print-file-name=plugin"):
                                    skipped()
                                    continue
                                try:
                                    if os.path.exists(cwd) and os.path.exists(bin):
                                        stdout0, stderr0, ret0 = worker.runCmd(cwd, bin, argv[1:] + ["-###"])
                                        out0 = stdout0 + "\n" + stderr0
                                    else:
                                        print(f"Command or cwd does not exist cwd={cwd} bin={bin} ptr={ptr}")
                                        failed()
                                        continue
                                except exec_worker.ExecWorkerException as e:
                                    worker.initialize()
                                    print("**********************************\n"\
                                        "ERROR = {}\n"\
                                        "cwd   = {}\n"\
                                        "bin   = {}\n"\
                                        "argv  = {}\n"\
                                        "**********************************".format(e,cwd,bin," ".join(argv[1:] + ["-###"])))
                                    failed()
                                    continue

                                lns = [shlex.split(x)
                                        for x in out0.splitlines()
                                        if x.startswith(" ") and re.match(cc1_patterns, x.split()[0])]

                                if len(lns) == 0:
                                    collect = [shlex.split(x)
                                        for x in out0.splitlines()
                                        if x.startswith(" ") and re.match(coll_patterns, x.split()[0])]
                                    if len(collect) > 0:
                                        skipped()
                                    else:
                                        store_output(ptr, f"ERROR: No CC1 patterns gcc compilation command", [bin] + argv[1:] + ['-###'], stdout0, stderr0, ret0)
                                        failed()
                                    continue

                                nargv = [bin if input_compiler_path is None else input_compiler_path] + argv[1:]
                                if '-D__ASSEMBLY__' in nargv:
                                        skip = False
                                        for x in nargv:
                                            if x.endswith(".S"):
                                                skip=True
                                        if skip:
                                            skipped()
                                            continue
                                        else:
                                            print( " ".join(nargv))
                                            pass
                                if '-o' in nargv:
                                    i = nargv.index('-o')
                                    nargv.pop(i)
                                    nargv.pop(i)
                                nargv.append(plugin_insert)
                                while '-pipe' in nargv:
                                    nargv.remove('-pipe')
                                while '-E' in nargv:
                                    nargv.remove('-E')
                                if '-c' not in nargv:
                                    nargv.append('-c')
                                if "-E" in nargv or "-pipe" in nargv or "-o" in nargv or "-c" not in nargv:
                                    printd(f"Params issue: {nargv}", ptr, True)
                                    failed()
                                    continue
                                # GET PLUGIN OUTPUT
                                try:
                                    stdout1, stderr1, ret1 = worker.runCmd(cwd, nargv[0], nargv[1:], "")
                                    output = stdout1 + "\n" + stderr1
                                except exec_worker.ExecWorkerException:
                                    worker.initialize()
                                    printd(f"Exception while running gcc -fplugin command: {nargv}", ptr, True)
                                    failed()
                                    continue
                                if ret1 != 0:
                                    if "Permission denied" in output:
                                        try:
                                            stdout1, stderr1, ret2 = worker.runCmd("/tmp/", nargv[0], nargv[1:], "")
                                            output = stdout1 + "\n" + stderr1
                                        except exec_worker.ExecWorkerException:
                                            worker.initialize()
                                            printd ("Exception while running gcc -fplugin command: {nargv}", ptr, True)
                                            failed()
                                            continue
                                        if ret2 != 0:
                                            store_output(ptr, f"Error getting input files from gcc compilation command (permission retry)", nargv, stdout1, stderr1, ret2)
                                            failed()
                                            continue
                                    # else:
                                    #     store_output(pos, f"Error getting input files from gcc compilation command", nargv, stdout1, stderr1, ret1)
                                    #     failed()
                                    #     continue

                                fns = output.strip().splitlines()

                                if not all((os.path.exists(os.path.join(cwd, x)) for x in fns)):
                                    store_output(ptr, f"Missing gcc compilation input files", nargv, stdout1, stderr1, ret1)
                                    failed()
                                    continue
                                if len(fns) == 0:
                                    store_output(ptr, f"Error getting input files from gcc compilation command", nargv, stdout1, stderr1, ret1)
                                    failed()
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
                                    nargv.insert(i+2, '-v')
                                    nargv.insert(i+3, '-dD')
                                else:
                                    nargv += ['-E', '-P', '-v', '-dD']
                                for x in fns:
                                    li = [i for i, u in enumerate(nargv) if u == x]
                                    if len(li):
                                        nargv.pop(li[-1])
                                    else:
                                        printd(f"CAN'T POP FILENAME \n{fns}\n{nargv}", ptr, True)
                                nargv.append('-')
                                # GET COMPILATION DATA
                                try:
                                    stdout3, stderr3, ret3 = worker.runCmd(cwd, nargv[0], nargv[1:], "")
                                    out3 = stdout3 + "\n" + stderr3
                                except exec_worker.ExecWorkerException:
                                    worker.initialize()
                                    printd(f"Exception {nargv}", ptr)
                                    failed()
                                    continue

                                if ret3 != 0:
                                    store_output(ptr, f"Error getting compilation data", nargv, stdout3, stderr3, ret3)
                                    failed()
                                    continue

                                compiler_path = os.path.join(cwd, self.maybe_compiler_binary(bin))
                                exe_dct = exe.json()
                                comp_objs = gcc_c.get_object_files(exe_dct, fork_map, rev_fork_map, wr_map)

                                #  try:
                                includes, defs, undefs = gcc_c.parse_defs(out3, exe_dct)
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
                                    absfn = os.path.realpath(os.path.normpath(os.path.join(cwd, fn)))
                                    if not absfn.startswith("/dev/"):
                                        out_fns.add(absfn)

                                if len(out_fns) == 0:
                                    if len(fns) == 1 and fns[0] == "/dev/null":
                                        skipped()
                                    else:
                                        store_output(ptr, f"Error getting input files from gcc compilation command\noutput = {fns}\noutput = {out_fns}", nargv, stdout3, stderr3, ret3)
                                        skipped()
                                    continue

                                write_queue.put({ptr: {
                                        "f": list(out_fns),
                                        "i": includes,
                                        "d": [{"n": di[0], "v": di[1]} for di in defs],
                                        "u": [{"n": ui[0], "v": ui[1]} for ui in undefs],
                                        "h": ifiles,
                                        "s": src_type,
                                        "o": comp_objs,
                                        "p": 0
                                    }},True,5)
                                with found_comps.get_lock():
                                    found_comps.value += 1

                        print("Searching for gcc compilations ... (%d candidates; %d jobs)" % (len(gxx_input_execs), jobs))
                        print_mem_usage(debug)
                        print(flush=True)
                        pbar = progressbar(total=len(gxx_input_execs), disable=None)
                        processed = multiprocessing.Value("i")
                        processed.value = 0
                        found_comps = multiprocessing.Value("i")
                        found_comps.value = 0
                        failed_comps = multiprocessing.Value("i")
                        failed_comps.value = 0           
                        skipped_comps = multiprocessing.Value("i")
                        skipped_comps.value = 0                   
                        cur_iter = multiprocessing.Value("i")
                        cur_iter.value = 0
                        max_iter = len(gxx_input_execs)
                        sstart = time.time()
                        procs = []

                        for i in range(jobs):
                            p = multiprocessing.Process(target=gcc_executor, args=(i,))
                            procs.append(p)
                            p.start()

                        for proc in procs:
                            proc.join()

                        pbar.n = processed.value
                        pbar.refresh()
                        pbar.close()
                        print(flush=True)
                        print(f"Workers finished in {time.time() - sstart:.2f}s - found={found_comps.value} failed={failed_comps.value} skipped={skipped_comps.value} compilations.")
                        if (found_comps.value + failed_comps.value + skipped_comps.value) != len(gxx_input_execs):
                            print ("ERROR: Found + skipped + error != all.")
                        print_mem_usage(debug)

                    print("finished searching for gcc compilations [%.2fs]" % (time.time() - start_time))

                with computation_finished.get_lock():
                    computation_finished.value = True
                
                write_queue.join()
                writer_process.join()

                print("computed compilations  [%.2fs]" % (time.time()-compilation_start_time))

        if not no_update:
            if len(out_linked.keys()) == 0 and os.path.exists(link_filename):
                printdbg("Linked info empty - loading from file...", debug)
                with open(link_filename, "r") as input_json:
                    out_linked = json.load(input_json)
                printdbg("DEBUG: loaded pre-generated {}".format(link_filename), debug)

            if len(out_pcp_map.keys()) == 0 and os.path.exists(pcp_filename):
                printdbg("PCP map empty - loading from file...", debug)
                with open(pcp_filename, "r") as input_json:
                    out_pcp_map = json.load(input_json)
                printdbg("DEBUG: loaded pre-generated {}".format(pcp_filename), debug)

            if len(out_rbm.keys()) == 0 and os.path.exists(rbm_filename):
                printdbg("RBM info empty - loading from file...", debug)
                with open(rbm_filename, "r") as input_json:
                    out_rbm = json.load(input_json)
                printdbg("DEBUG: loaded pre-generated {}".format(rbm_filename), debug)

            if len(out_compilation_info.keys()) == 0 and os.path.exists(comps_filename):
                printdbg("Compilation info empty - loading from file...", debug)
                with open(comps_filename, "r") as input_json:
                    out_compilation_info = json.load(input_json)
                printdbg("DEBUG: loaded pre-generated {}".format(comps_filename), debug)
            print("Rewriting {} ...".format(json_db_filename))

            with open(json_db_filename + ".tmp", "wb") as ofile:
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
                        print("\n Updating {} {}".format(ptr, u))

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


def print_mem_usage(debug: bool = False, message: Optional[str] = None):
    # Check psutil._pslinux.Process.memory_info() for details
    if debug:
        import psutil
        if message is not None:
            print(message, flush=True)
        mi = psutil.Process().memory_info()

        def fmt(by):
            return f"{by / (1024 * 1024):.2f} MB"

        print(f"RAM || rss {fmt(mi.rss)} || vms {fmt(mi.vms)} || "
                f"shared {fmt(mi.shared)} || text {fmt(mi.text)} || "
                f"lib {fmt(mi.lib)} || data {fmt(mi.data)} "
                f"|| dirty {fmt(mi.dirty)}", flush=True)


def printdbg(msg, debug: bool = False, file=sys.stderr, flush=True):
    if debug:
        print("DEBUG: "+msg, file=file, flush=flush)


def printerr(msg, file=sys.stderr, flush=True):
    print("ERROR: "+msg, file=file, flush=flush)
