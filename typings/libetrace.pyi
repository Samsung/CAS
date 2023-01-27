# pylint: disable=used-before-assignment multiple-statements invalid-name unused-argument
"""
Libetrace module  - API of nfsdb database.
"""
from typing import overload, Iterator, List, Dict, Tuple, Set, Optional

class error(Exception):
    pass

class nfsdb:
    """
    Database class - used to load database files and fetch data.
    """
    source_root: str
    """
    Directory where tracing process started execution
    """
    dbversion: str
    """
    database version - set at cache creation
    """

    filemap: Dict[str,List[nfsdbEntryOpenfile]]
    """
    Map of path:str -> list: nfsdbEntryOpenfile
    """

    def __len__(self) -> int: ...

    def __next__(self) -> nfsdbEntry: ...

    @overload
    def __getitem__(self, idx: int) -> nfsdbEntry:
        """
        Get single nfsdbEntry at given index.

        :param idx: get nfsdbEntry at index
        :return: nfsdbEntry object
        """
    @overload
    def __getitem__(self, pid: Tuple[int, Optional[int]]) -> List[nfsdbEntry]:
        """
        Get list of nfsdbEntry matching given pid(,index) pair.

        :param pid: tuple (pid,) or (pid,index)
        :return: List of nfsdbEntry objects
        """
    @overload
    def __getitem__(self, pids: List[Tuple[int, Optional[int]]]) -> List[nfsdbEntry]:
        """
        Get list of nfsdbEntry matching given list of pid(,index) pairs.

        :param pids: List of tuples (pid,) or (pid,index)
        :return: List of nfsdbEntry objects
        """
    @overload
    def __getitem__(self, bpath: str) -> List[nfsdbEntry]:
        """
        Get list of nfsdbEntry that utilize given binary path.

        :param bpath: binary path
        :return: List of nfsdbEntry objects
        """
    @overload
    def __getitem__(self, bpaths: List[str]) -> List[nfsdbEntry]:
        """
        Get list of nfsdbEntry that utilize given binary paths.

        :param bpath: List of binary paths
        :return: List of nfsdbEntry objects
        """

    def __repr__(self): ...

    def __iter__(self) -> Iterator[nfsdbEntry]:  ...

    def __init__(self) -> None: ...

    def load(self, cache_filename: str, debug: bool, quiet: bool, mp_safe: bool, no_map_memory: bool = False) -> bool:
        """
        Loads database from given filename

        :param cache_filename: database file path
        :type cache_filename: str
        :param debug: print debugging information
        :type debug: bool
        :param quiet: prevent any verbosity
        :type quiet: bool
        :param mp_safe: load database in read-only mode slower but safer when using multiprocessing
        :type mp_safe: bool
        :param no_map_memory: ??
        :type no_map_memory: bool
        :return: True if load succeed otherwise False
        """

    def load_deps(self, cache_filename: str, debug: bool, quiet: bool, mp_safe: bool, no_map_memory: bool = False) -> bool:
        """
        Loads dependencies database from given filename

        :param cache_filename: database file path
        :type cache_filename: str
        :param debug: print debugging information
        :type debug: bool
        :param quiet: prevent any verbosity
        :type quiet: bool
        :param mp_safe: load database in read-only mode slower but safer when using multiprocessing
        :type mp_safe: bool
        :param no_map_memory: ??
        :type no_map_memory: bool
        :return: True if load succeed otherwise False
        """

    def create_deps_cache(self, depmap, direct_depmap, deps_cache_db_filename, show_stats=False) -> bool:
        """
        Function create dependencies cache file based on provided dependency maps.

        :param depmap: dependency map
        :type depmap: dict
        :param direct_depmap: direct dependency map
        :type direct_depmap: dict
        :param db_cache_filename: output filename
        :type db_cache_filename: str
        :param show_stats: show statistics, defaults to False
        :type show_stats: bool, optional
        """

    def iter(self, ) -> Iterator[nfsdbEntry]:
        """
        Returns interator over executables
        """

    def filtered(self, has_comp_info: bool = ..., has_linked_file: bool = ..., has_command: bool = ...) -> Iterator[nfsdbEntry]:
        """
        Filter executables depends on given parameters.

        :param has_comp_info: database file path
        :param has_linked_file: print debugging information
        :param has_command: prevent any verbosity
        :return: list of filtered executables
        """

    def opens_iter(self, ) -> Iterator[nfsdbEntryOpenfile]:
        """
        Returns list of opens as nfsdbEntryOpenfile iterator.
        """

    def opens(self, ) -> List[nfsdbEntryOpenfile]:
        """
        Returns list of opens as `nfsdbEntryOpenfile` instances list.

        :return: List of `nfsdbEntryOpenfile` objects
        :rtype: list[str]
        """

    def opens_paths(self, ) -> List[str]:
        """
        Returns list of opens as str list.

        :return: List of file paths
        :rtype: list[str]
        """

    def pcp_list(self, ) -> List[str]:
        """
        Returns precompiled command patterns.

        :return: List of patterns
        :rtype: list[str]
        """

    def pids(self, ) -> List[int]:
        """
        Returns list of all pids

        :return: List of pid numbers
        :rtype: list[int]
        """

    def bpaths(self, ) -> List[str]:
        """
        Returns list of all binaries paths

        :return: List of binaries paths
        :rtype: list[str]
        """

    def fpaths(self, ) -> List[str]:
        """
        Returns list of all opened file paths

        :return: List of file paths
        :rtype: list[str]
        """

    def linked_modules(self, ) -> List[Tuple[nfsdbEntryOpenfile, int]]:
        """
        Returns list of all linked modules (executions matched with config ld_spec)

        :return: List of tuples containing `nfsdbEntryOpenfile` and `linked type`
        :rtype: list[ tuple (nfsdbEntryOpenfile, int) ]
        """

    def linked_module_paths(self, ) -> List[str]:
        """
        Returns list of all linked modules paths (executions matched with config ld_spec)
        """

    def fdeps(self, file_path, debug: bool = False, dry_run: bool = False, direct: bool = False, debug_fd: bool = False,
        negate_pattern: bool = False, dep_graph: bool = False, wrap_deps: bool = False, use_pipes: bool = False,
        recursive: bool = ..., timeout: int = ..., exclude_patterns: List[str] = ..., exclude_commands: List[str] = ...,
        exclude_commands_index: List[str] = ..., all_modules: List[str] = ...) -> Tuple[List[int],List[str],List[nfsdbEntryOpenfile],Dict]: ...


    def mdeps(self, module_path:"str|List[str]", direct:bool=False) -> List[nfsdbEntryOpenfile]:
        """
        Returns list of file dependencies of given module

        :param module_path: module filename(s)
        :type module_path: str | list[str]
        :param direct: get direct or full dependencies
        :type direct: bool
        :return: list of open objects
        :rtype: list[nfsdbEntryOpenfile]
        """

    def mdeps_count(self, module_path:"str|List[str]", direct:bool=False) -> int:
        """
        Returns dependency module count of given module

        :param module_path: module filename(s)
        :type module_path: str | list[str]
        :param direct: consider direct or full dependencies
        :type direct: bool
        :return: dependency count
        :rtype: int
        """

    def rdeps(self, file_path:"str|List[str]", recursive:bool=False) -> Set[str]:
        """
        Returns reversed dependencies of given file

        :param file_path: filename(s)
        :type file_path: str | list[str]
        :param recursive: get recursive dependencies
        :type recursive: bool
        :return: list of file names
        :rtype: set[str]
        """

    def path_exists(self, path:str) -> bool:
        """
        Returns information about path existence

        :param path: file path
        :type path: str
        :return: True if path exists during postprocess otherwise False
        :rtype: bool
        """

    def path_read(self, ) -> bool:
        """
        Returns information if path was open for reading

        :param path: file path
        :type path: str
        :return: True if path was open for reading otherwise False
        :rtype: bool
        """

    def path_write(self, ) -> bool:
        """
        Returns information if path was open for writing

        :param path: file path
        :type path: str
        :return: True if path was open for writing otherwise False
        :rtype: bool
        """

    def path_regular(self, ) -> bool:
        """
        Returns information if path is regular file

        :param path: file path
        :type path: str
        :return: True if path is regular file (not dir or special) otherwise False
        :rtype: bool
        """

    def precompute_command_patterns(self, excl_commands:List[str])-> "dict | None":
        """
        Function calculate exclude command patterns for executables using database nfsdbEntry

        :param excl_commands: patterns of excluded commands
        :type excl_commands: List[str]
        :return: precomputed command patterns dict or None if function failed
        :rtype: dict | None
        """


def parse_nfsdb(tracer_db_filename:str, json_db_filename:str) -> "int | None":
    """
    Function parse raw tracer file (.nfsdb), translate it to dict object and store as json file (.nfsdb.json).

    :param tracer_db_filename: tracer file path
    :type tracer_db_filename: str
    :param json_db_filename: output json database file path
    :type json_db_filename: str
    :return: return code of parse process or None
    :rtype: int | None
    """


def precompute_command_patterns(excl_commands:List[str], proccess_map:Dict, debug:bool=False) -> "dict | None":
    """
    Function calculate exclude command patterns for executables using process map dict

    :param excl_commands: patterns of excluded commands
    :type excl_commands: list[str]
    :param proccess_map: process map eg. { PID: [execs0, execs1, ...] }
    :type proccess_map: Dict
    :param debug: display debug information on stdout, defaults to False
    :type debug: bool, optional
    :return: precomputed command patterns dict or None if function failed
    :rtype: dict | None
    """


def is_ELF_or_LLVM_BC_file(written_files:List[str]):...

def is_ELF_file(written_files:List[str]):...

def create_nfsdb(db:dict, src_root:str, set_version:str, pcp_patterns:list, cache_db_filename:str, show_stats:bool=False) -> bool:
    """
    Function takes database in dict format and stores it to highly-optimized image file (.nfsdb.img).

    :param db: dict representation of database
    :type db: dict
    :param src_root: tracing process start dir
    :type src_root: str
    :param set_version: database version
    :type set_version: str
    :param pcp_patterns: precompiled patterns list
    :type pcp_patterns: list
    :param cache_db_filename: filename of cache database
    :type cache_db_filename: str
    :param show_stats: show stats while building database, defaults to False
    :type show_stats: bool, optional
    :return: True if success otherwise False
    :rtype: bool
    """

def parse_compiler_triple_hash(command:str)-> List[str]:
    """
    Parse clang output for spawned commands arguments used in internal compilations

    :param command: clang command output
    :type command: str
    :return: list of internal command arguments
    :rtype: list[str]
    """

class nfsdbEntryOpenfile:
    """
    Object representation of single file operation done by `parent` process.
    """
    opaque: nfsdbEntry
    """In case of compiled or linked file this value points to process where compilation or linking was processed."""
    path: str
    """Open path"""
    original_path: str
    """If open was symlink this value points to symlink target location"""
    parent: nfsdbEntry
    """Process that made open"""
    size: int
    """File size"""
    ptr: Tuple[int,int]
    mode: int
    """Access mode 0xemmmmss"""

    def is_read(self) -> bool:
        """
        Function returns True if path was open for read.

        :return: True if nfsdbEntryOpenfile was open for read
        :rtype: bool
        """

    def is_write(self) -> bool:
        """
        Function returns True if path was open for write.

        :return: True if nfsdbEntryOpenfile was open for write
        :rtype: bool
        """

    def is_reg(self):
        """
        Function returns True if path is regular file.

        :return: True if nfsdbEntryOpenfile is regular file
        :rtype: bool
        """

    def is_dir(self) -> bool:
        """
        Function returns True if path is directory.

        :return: True if nfsdbEntryOpenfile is directory
        :rtype: bool
        """

    def is_chr(self) -> bool:
        """
        Function returns True if path is character device.

        :return: True if nfsdbEntryOpenfile is character device
        :rtype: bool
        """

    def is_block(self) -> bool:
        """
        Function returns True if path is block device.

        :return: True if nfsdbEntryOpenfile is block device
        :rtype: bool
        """

    def is_symlink(self) -> bool:
        """
        Function returns True if path is symlink.

        :return: True if nfsdbEntryOpenfile is symlink
        :rtype: bool
        """

    def is_fifo(self) -> bool:
        """
        Function returns True if path is fifo.

        :return: True if nfsdbEntryOpenfile is fifo
        :rtype: bool
        """

    def is_socket(self) -> bool:
        """
        Function returns True if path is socket.

        :return: True if nfsdbEntryOpenfile is socket
        :rtype: bool
        """

    def exists(self) -> bool:
        """
        Function returns True if object represented by path exist.

        :return: True if nfsdbEntryOpenfile is exist
        :rtype: bool
        """

    def path_modified(self) -> bool:
        """
        Function checks if open path was modified

        :return: True if path is not the same as original path otherwise False
        :rtype: bool
        """

    def has_opaque_entry(self) -> bool:
        """
        Function returns True if nfsdbEntryOpenfile was either compiled or linked.

        :return: True if nfsdbEntryOpenfile was assessed as compiled or linked
        :rtype: bool
        """

    def json(self) -> Dict:
        """
        Function return dict representation of nfsdbEntryOpenfile object

        :return: dict representation of object
        :rtype: Dict
        """

class nfsdbEntryEid:
    """
    Extended pid class. Contains pid and index pair.
    """
    pid: int
    """process pid"""
    index: int
    """process index"""

class nfsdbEntryCid:
    """
    Class contains child process spawn information
    """
    pid: int
    """process pid"""
    flags: int
    """process spawn flags"""

class nfsdbEntryCompilationInfo:
    """
    Class used to wrap all compilation data of process
    """
    file_paths: List[str]
    """List of compiled file paths"""
    files: List[nfsdbEntryOpenfile]
    """List of compiled files as nfsdbEntryOpenfile"""
    object_paths: List[str]
    """List of compiled objects paths"""
    objects: List[nfsdbEntryOpenfile]
    """List of object files as nfsdbEntryOpenfile"""
    header_paths: List[str]
    headers: list
    ipaths: list
    type: int
    defs: list
    undefs: list

    def is_integrated(self) -> bool:
        """
        Check if compilation is integrated

        :return: True if compilation is integrated otherwise False
        :rtype: bool
        """

class nfsdbEntry:
    """
    Object representing single process that was called or spawned during tracing.
    """
    ptr: int
    """Index of executable in database"""
    eid: nfsdbEntryEid
    """Process extended pid"""
    etime: int
    """Execution length in ms."""
    parent_eid: nfsdbEntryEid
    """Parent process Eid"""
    parent: nfsdbEntry
    """Parent process object"""
    child_cids: List[nfsdbEntryCid]
    """List of child identification objects"""
    childs: List[nfsdbEntry]
    """List of child objects"""
    binary: str
    """Binary path"""
    cwd: str
    """Working directory path"""
    bpath: str
    """Binary path"""
    argv: List[str]
    """List of argument passed to binary"""
    opens: List[nfsdbEntryOpenfile]
    """List of opens referenced by this process"""
    openpaths: List[str]
    """List of opens paths referenced by this process"""
    opens_with_children: List[nfsdbEntryOpenfile]
    """List of opens referenced by this process and its children"""
    openpaths_with_children: List[str]
    """List of opens paths referenced by this process and its children"""
    pcp: list
    wpid: list
    pipe_eids: List[nfsdbEntryEid]
    linked_file: nfsdbEntryOpenfile
    linked_path: str
    linked_ptr: int
    linked_type: int
    compilation_info: nfsdbEntryCompilationInfo

    def json(self) -> Dict:
        """
        Function return dict representation of nfsdbEntry object

        :return: dict representation of object
        :rtype: Dict
        """

    def has_pcp(self) -> bool:
        """
        Function return True if nfsdbEntry process was excluded in any precomputed patterns

        :return: True if process was excluded by patterns otherwise False
        :rtype: bool
        """

    def pcp_index(self) -> List[int]:
        """
        Returns list of entry precomputed command pattern index

        :return: list of pcp indexes
        :rtype: list[int]
        """

    def is_wrapped(self) -> bool:
        """
        Function return True if nfsdbEntry process was wrapped (eg with /bin/bash )

        :return: True if process was wrapped otherwise False
        :rtype: bool
        """

    def has_compilations(self) -> bool:
        """
        Function return True if nfsdbEntry process was classified as compiler

        :return: True if process is compiler otherwise False
        :rtype: bool
        """        

    def is_linking(self) -> bool:
        """
        Function return True if nfsdbEntry process was classified as linker

        :return: True if process is linker otherwise False
        :rtype: bool
        """

    def open_stat(self) -> bool: ...

    def open_access(self) -> bool: ...

class clang:

    allow_pp_in_compilations:bool

    def __init__(self, verbose:bool, debug:bool, debug_compilations:bool):
        """_summary_

        :param verbose: _description_
        :type verbose: bool
        :param debug: _description_
        :type debug: bool
        :param debug_compilations: _description_
        :type debug_compilations: bool
        """
    def get_compilations(self, compilations:List[Tuple], jobs:int, tailopts:List[str], allowpp:bool):
        """_summary_

        :param compilations: _description_
        :type compilations: List[Tuple]
        :param jobs: _description_
        :type jobs: int
        :param tailopts: _description_
        :type tailopts: List[str]
        :param allowpp: _description_
        :type allowpp: bool
        """

class gcc:
    def __init__(self, verbose:bool, debug:bool, debug_compilations:bool):
        """_summary_

        :param verbose: _description_
        :type verbose: bool
        :param debug: _description_
        :type debug: bool
        :param debug_compilations: _description_
        :type debug_compilations: bool
        """
    def get_compilations(self, compilations:List[Tuple], jobs:int, tailopts:List[str], allowpp:bool):
        """_summary_

        :param compilations: _description_
        :type compilations: List[Tuple]
        :param jobs: _description_
        :type jobs: int
        :param tailopts: _description_
        :type tailopts: List[str]
        :param allowpp: _description_
        :type allowpp: bool
        """