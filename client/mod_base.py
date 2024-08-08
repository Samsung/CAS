import fnmatch
from functools import lru_cache
import sys
import re
from abc import abstractmethod
from typing import Any, List, Dict, Tuple, Generator, Callable
import argparse
import libetrace
import libcas
import libftdb
from libft_db import FTDatabase
from client.filtering import CommandFilter, OpenFilter, FilterException, FtdbSimpleFilter
from client.misc import get_output_renderers, printdbg, fix_cmd
from client.output_renderers.output import DataTypes
from client.exceptions import ArgumentException, LibFtdbException, LibetraceException, PipelineException

class ModulePipeline:
    def __init__(self, modules: list) -> None:
        self.modules: List[Module] = modules
        self.last_module = None

    def subject(self, ent) -> "str | None":
        return self.last_module.subject(ent) if self.last_module is not None else None

    def render(self):
        data = []
        renderer = None
        sort_lambda: "Callable | None" = None
        pipe_type: "type | None" = None
        for mdl in self.modules:
            printdbg("DEBUG: START pipeline step {}".format(mdl.args.module.__name__), mdl.args)
            if mdl.args.is_piped and self.last_module is not None and pipe_type is not None:
                printdbg("DEBUG: >>>>>> piping {} -->> {} -->> {}".format(self.last_module.args.module.__name__, pipe_type.__name__, mdl.args.module.__name__), mdl.args)
                if not isinstance(data, list):
                    raise PipelineException("Input data '{}' module is not list - cannot proceed with next pipeline step!".format(mdl.args.module.__name__))
                elif len(data) == 0:
                    raise PipelineException("Input data to '{}' module is empty - cannot proceed with next pipeline step!".format(mdl.args.module.__name__))
                if isinstance(mdl, PipedModule):
                    mdl.set_piped_arg(data, pipe_type)
                else:
                    raise PipelineException("Can't pipe to '{}' module - this module is not accepting any input data.".format(mdl.args.module.__name__))


            mdl.check_required(mdl.required_args, mdl.args)

            if "open_filter" in mdl.args and mdl.args.open_filter:
                mdl.open_filter = OpenFilter(mdl.args.open_filter, mdl, mdl.config, mdl.source_root)

            if "command_filter" in mdl.args and mdl.args.command_filter:
                mdl.command_filter = CommandFilter(mdl.args.command_filter, mdl, mdl.config, mdl.source_root)

            if "ftdb_simple_filter" in mdl.args and mdl.args.ftdb_simple_filter:
                mdl.ftdb_simple_filter = FtdbSimpleFilter(mdl.args.ftdb_simple_filter, mdl, mdl.config, mdl.source_root, mdl.ft_db)

            try:
                printdbg("DEBUG:   generating data...", mdl.args)
                data, renderer, sort_lambda, pipe_type = mdl.get_data()
                if isinstance(data, list):
                    printdbg("DEBUG:   data len == {}".format(len(data)), mdl.args)
                printdbg("DEBUG:   data renderer == {}".format(renderer.name), mdl.args)
                printdbg("DEBUG:   entry data type == {}".format(pipe_type), mdl.args)
            except libetrace.error as exc:
                raise LibetraceException("libetrace.error : {}".format(str(exc))) from exc
            except libftdb.FtdbError as exc:
                raise LibFtdbException("libftdb.FtdbError : {}".format(str(exc))) from exc
            self.last_module = mdl
            printdbg("DEBUG: END pipeline step {}".format(mdl.args.module.__name__), mdl.args)
        if self.last_module is not None:
            if self.last_module.args.ide:
                return data
            if self.last_module.args.ftdb_create:
                return data
            if self.last_module.args.save_zip_archive or self.last_module.args.save_tar_archive:
                return data
            output_engine = self.last_module.get_output_engine()(data, self.last_module.args, self.last_module, renderer, sort_lambda)
            return output_engine.render_data()
        return None


class Module:
    """Abstract class of all API modules."""
    required_args: List[str] = []
    open_filter = None
    command_filter = None
    ftdb_simple_filter = None

    def __init__(self, args: argparse.Namespace, nfsdb: libcas.CASDatabase, config, ft_db: FTDatabase = None) -> None:
        self.args: argparse.Namespace = args
        self.config = config
        self.nfsdb = nfsdb
        self.ft_db = ft_db
        self.source_root = nfsdb.source_root if hasattr(nfsdb, "source_root") else ""

        self.has_pipe_path = True if "pipe_path" in self.args and self.args.pipe_path and len(self.args.pipe_path) > 0 else False
        self.has_open_filter = True if "open_filter" in self.args and self.args.open_filter else False
        self.has_command_filter = True if "command_filter" in self.args and self.args.command_filter else False
        self.has_ftdb_simple_filter = True if "ftdb_simple_filter" in self.args and self.args.ftdb_simple_filter else False
        self.has_select = True if "select" in self.args and self.args.select and len(self.args.select) > 0 else False
        self.has_append = True if "append" in self.args and self.args.append and len(self.args.append) > 0 else False
        self.has_exclude = True if "exclude" in self.args and self.args.exclude and len(self.args.exclude) > 0 else False
        self.has_pid = True if "pid" in self.args and self.args.pid and len(self.args.pid) > 0 else False
        self.has_command = True if "show_commands" in self.args and self.args.show_commands else False
        self.has_generate = True if "generate" in self.args and self.args.generate else False
        self.has_all = True if "all" in self.args and self.args.all else False
        self.relative_path = True if "relative" in self.args and self.args.relative else False
        self.original_path = True if "original_path" in self.args and self.args.original_path else False

    @abstractmethod
    def subject(self, ent) -> str:
        """
        Function returns simple value of object which depends on context.

        :param ent: _description_
        :type ent: `libetrace.nfsdbEntry` | `libetrace.nfsdbEntryOpenfile`
        :return: simple value
        :rtype: Any
        """

    @abstractmethod
    def select_subject(self, ent) -> str:
        """
        Function returns simple value of object used in select which depends on context.

        :param ent: _description_
        :type ent: `libetrace.nfsdbEntry` | `libetrace.nfsdbEntryOpenfile`
        :return: simple value
        :rtype: Any
        """

    @abstractmethod
    def exclude_subject(self, ent) -> str:
        """
        Function returns simple value of object used in exclude which depends on context.

        :param ent: _description_
        :type ent: `libetrace.nfsdbEntry` | `libetrace.nfsdbEntryOpenfile`
        :return: simple value
        :rtype: Any
        """

    @staticmethod
    def add_args(args: List, mod):
        from client.argparser import args_map
        parser = argparse.ArgumentParser(description=mod.__doc__)
        arg_group = parser.add_argument_group("Module specific arguments")
        for arg in args:
            args_map[arg](arg_group)
        return parser

    @staticmethod
    @abstractmethod
    def get_argparser() -> argparse.ArgumentParser:
        """
        Function returns class customized argument parser.

        :return: argument parser
        :rtype: argparse.ArgumentParser
        """

    @abstractmethod
    def get_data(self) -> Tuple[Any, DataTypes, "Callable|None", "type|None"]:
        """
        Function returns `data` records with `DataType` and optional pipe type and sorting lambda.

        :return: _description_
        :rtype: tuple ( list, DataType, type, Callable )
        """

    def get_relative(self, open_path: str) -> str:
        """
        Function replaces source root in given path and returns only relative path.

        :param open_path: path to process
        :type open_path: str
        :return: relative path or original path if there where no source root in it
        :rtype: str
        """
        if self.relative_path:
            if self.source_root in open_path:
                op = open_path.replace(self.source_root, "")
                if len(op) > 1 and op[0] == '/':
                    op = op[1:]
                return op
            else:
                return open_path
        else:
            return open_path

    def get_path(self, open_path: str) -> str:
        """
        Function returns desired path. If --relative-path parameter is provided it removes source_root from path.

        :param open_path: file path
        :type open_path: str
        :return: Desired file path
        :rtype: str
        """
        return self.get_relative(open_path)

    def filename_matcher(self, filename: str, patterns: list) -> bool:
        """
        Function matches `filename` with given `patterns`.

        :param filename: file path to check
        :type filename: str
        :param patterns: list of string patterns
        :type patterns: list
        :return: `True` if any of patterns are matched otherwise `False`
        :rtype: bool
        """
        for pattern in patterns:
            if "*" in pattern:
                if fnmatch.fnmatch(filename, pattern):
                    return True
            else:
                if pattern == filename:
                    return True
        return False

    def filter_output(self, data:List) -> List["libetrace.nfsdbEntryOpenfile | str"]:
        """
        Function check all data paths and matches it with --unique-output-list parttern,
        if more than one record matches it is dropped from data list.

        :param data: data list to check
        :type data: List
        :return: data list
        :rtype: List
        """
        if self.args.unique_output_list:
            to_unique = set()
            marked_for_deletion = []
            patterns = [re.compile(p) for p in self.args.unique_output_list]
            for d in data:
                path = d.path if isinstance(d, libetrace.nfsdbEntryOpenfile) else d
                for p in patterns:
                    m = p.match(path)
                    if m:
                        groups = m.groups()
                        if (groups[0], groups[1]) in to_unique:
                            marked_for_deletion.append(d)
                        else:
                            to_unique.add((groups[0], groups[1]))
            for d in marked_for_deletion:
                data.remove(d)
        return data

    def needs_open_filtering(self) -> bool:
        """
        Function check if advanced filtering is necessary. This is optimization function for large element sets.

        :return: `True` if filtering is needed otherwise `False`
        :rtype: bool
        """
        return self.has_pipe_path or self.has_exclude or self.has_open_filter or self.has_select or self.has_append

    def filter_open(self, opn: libetrace.nfsdbEntryOpenfile) -> bool:
        """
        Function check if provided open element matches filters.

        :param opn: open element
        :type opn: `libetrace.nfsdbEntryOpenfile`
        :return: `True` if filtering allow given element otherwise `False`
        :rtype: bool
        """
        ret = True

        if self.has_pipe_path:
            ret = ret and (self.filename_matcher(self.subject(opn), self.args.pipe_path))

        if self.has_exclude:
            ret = ret and (not self.filename_matcher(self.exclude_subject(opn), self.args.exclude))

        if self.has_open_filter and self.open_filter is not None:
            ret = ret and self.open_filter.resolve_filters(opn)

        if self.has_select:
            ret = ret and (opn.path in self.args.select)

        return ret

    def needs_exec_filtering(self) -> bool:
        """
        Function check if advanced filtering is necessary. 
        This is optimization function for large element sets.

        :return: True if filtering is needed otherwise False
        :rtype: bool
        """
        return self.has_exclude or self.has_pid or self.has_command_filter or self.has_select

    def filter_exec(self, ent: libetrace.nfsdbEntry) -> bool:
        """
        Function check if provided exec element matches filters.

        :param ent: exec element
        :type ent: `libetrace.nfsdbEntryOpenfile`
        :return: `True` if filtering allow given element otherwise `False`
        :rtype: bool
        """
        ret = True

        if self.has_generate:
            if not self.has_all:
                ret = ret and (ent.compilation_info is not None)

        if self.has_exclude:
            ret = ret and (not self.filename_matcher(self.exclude_subject(ent), self.args.exclude))

        if self.has_pid:
            ret = ret and (ent.eid.pid in self.args.pid)

        if self.has_command_filter and self.command_filter is not None:
            ret = ret and self.command_filter.resolve_filters(ent)

        if self.has_select:
            ret = ret and (self.filename_matcher(self.select_subject(ent), self.args.select))

        return ret
    
    def filter_ftdb(self, ent) -> bool:

        ret = True

        if self.has_ftdb_simple_filter and self.ftdb_simple_filter is not None:
            ret = ret and self.ftdb_simple_filter.resolve_filters(ent)
        return ret

    def get_reverse_dependencies_opens(self, file_paths: "str | List[str]", recursive: bool = False) -> List[libetrace.nfsdbEntryOpenfile]:
        """
        Function returns reverse dependencies of given paths.

        :param file_paths: path(s)
        :type file_paths: str | List[str]
        :param recursive: look for dependencies in whole tree not only direct modules, defaults to False
        :type recursive: bool, optional
        :return: list of reverse dependencies opens object
        :rtype: List[libetrace.nfsdbEntryOpenfile]
        """
        lm_map = {lm.path: lm for lm in self.nfsdb.linked_modules()}
        return [lm_map[r] for r in list(self.nfsdb.get_reverse_dependencies(file_paths, recursive=recursive))]

    @lru_cache(maxsize=1)
    def get_src_types(self) -> Dict[str, int]:
        """
        Function returns compiled_file:compilation_type dictionary.

        :return: dictionary with compiled file types.
        :rtype: Dict[str, int]
        """
        return {ent.compilation_info.files[0].path: ent.compilation_info.type for ent in self.nfsdb.get_compilations()}

    def yield_path_from_pid(self, pids: "List[Tuple[int, int]] | List[Tuple[int,]]") -> Generator:
        """
        Function yields open paths of process with given pid(s)

        :param pids: pids
        :type pids: List[Tuple[int, int]] | List[Tuple[int,]]
        :yield: opens paths  of given process pids
        :rtype: Generator
        """
        for ent in self.nfsdb.get_entries_with_pids(pids):
            for o in (ent.opens_with_children if self.args.with_children else ent.opens):
                if self.filter_open(o):
                    yield o.path

    def yield_open_from_pid(self, pids: "List[Tuple[int, int]] | List[Tuple[int,]]") -> Generator:
        """
        Function yields opens of process with given pid(s)

        :param pids: pids
        :type pids: List[Tuple[int, int]] | List[Tuple[int,]]
        :yield: opens of given process pids
        :rtype: Generator
        """
        for ent in self.nfsdb.get_entries_with_pids(pids):
            for o in (ent.opens_with_children if self.args.with_children else ent.opens):
                if self.filter_open(o):
                    yield o

    def get_multi_deps(self, epaths:"List[libcas.DepsParam | str]") -> List[libetrace.nfsdbEntryOpenfile]:
        """
        Function runs proper dependency generation function based on --cache argument.

        :param paths: list of extended deps parameters or single path
        :type paths: List[libcas.DepsParam|str]
        :return: list of opens objects
        :rtype: List[List[libetrace.nfsdbEntryOpenfile]]
        """
        if self.args.cached:
            return self.nfsdb.get_module_dependencies(epaths, direct=self.args.direct_global)
        else:
            return self.nfsdb.get_multi_deps(epaths, direct_global=self.args.direct_global, dep_graph=self.args.dep_graph,
                                            debug=self.args.debug, debug_fd=self.args.debug_fd, use_pipes=self.args.with_pipes,
                                            wrap_deps=self.args.wrap_deps)

    def get_dependency_graph(self, epaths:"List[libcas.DepsParam | str]"):
        """
        Function returns dependency graph of given file list

        :param epaths: list of extended deps parameters or single path
        :type epaths: List[libcas.DepsParam] | List[str]
        :return: dependency graph
        :rtype: List[Tuple]
        """
        return self.nfsdb.get_dependency_graph(epaths, direct_global=self.args.direct_global, debug=self.args.debug,
                                                debug_fd=self.args.debug_fd, use_pipes=self.args.with_pipes,
                                                wrap_deps=self.args.wrap_deps)

    def get_revdeps(self, file_paths):
        return self.nfsdb.get_reverse_dependencies(file_paths, recursive=self.args.recursive)

    def get_rdm(self, file_paths):
        return self.nfsdb.get_rdm(file_paths, recursive=self.args.recursive, sort=self.args.sorted, reverse=self.args.reverse)

    def get_cdm(self, file_paths)-> List[List]:
        cdm_exclude_patterns = None
        if self.args.cdm_exclude_patterns is not None:
            cdm_exclude_patterns = []
            if isinstance(self.args.cdm_exclude_patterns, list):
                for ex in self.args.cdm_exclude_patterns:
                    cdm_exclude_patterns.append(ex)
            else:
                cdm_exclude_patterns.append(self.args.cdm_exclude_patterns)

        cdm_exclude_files = None
        if self.args.cdm_exclude_files is not None:
            cdm_exclude_files = []
            if isinstance(self.args.cdm_exclude_files, list):
                for ex in self.args.cdm_exclude_files:
                    cdm_exclude_files.append(ex)
            else:
                cdm_exclude_files.append(self.args.cdm_exclude_files)

        return self.nfsdb.get_cdm(file_paths, cdm_exclude_files=cdm_exclude_files, cdm_exclude_patterns=cdm_exclude_patterns,
                                recursive=self.args.recursive, sort=self.args.sorted, reverse=self.args.reverse)

    def cdb_fix_multiple(self, data: List[libetrace.nfsdbEntry]) -> Generator:
        for exe in data:
            if exe.compilation_info is not None:
                if len(exe.compilation_info.file_paths) > 1:
                    for comp_file in exe.compilation_info.file_paths:
                        yield {"filename": comp_file, "cmd": fix_cmd(exe.argv), "dir": exe.cwd}
                else:
                    yield {"filename": exe.compilation_info.file_paths[0], "cmd": fix_cmd(exe.argv), "dir": exe.cwd}

    def expath_to_dict(self, expath_string: str) -> dict:
        parameters_schema = {
            "file": None,
            "exclude_cmd": None,
            "exclude": None,
            "direct": ["true", "false"],
            "negate_pattern": ["true", "false", "0", "1"]
        }
        expath_string = expath_string.replace("(", "").replace(")", "")
        ret = {}
        for pair in expath_string.split(","):
            if len(pair.split("=")) == 2:
                key, val = pair.split("=")
                if key not in ret:
                    ret[key] = val
                else:
                    if isinstance(ret[key], str):
                        ret[key] = [ret[key]]
                    else:
                        ret[key].append(val)
            else:
                ret["file"] = pair

        for key in ret:
            if key not in parameters_schema:
                raise FilterException("Unknown extended path parameter {} ! Allowed parameters: {}".format(key, ", ".join(parameters_schema.keys())))
        for key in [k for k in parameters_schema if k in ret]:
            if parameters_schema[key] is not None and ret[key] not in parameters_schema[key]:
                raise FilterException("'{}' is not proper '{}=' parameter value! Allowed values: {} ".format(ret[key], key, ", ".join(parameters_schema[key])))
        return ret

    def expand_to_deps_param(self, expath_string: str) -> "libcas.DepsParam | str":
        dct = self.expath_to_dict(expath_string)
        if len(dct) == 1 and "file" in dct:
            return expath_string
        return libcas.DepsParam(file=dct["file"],
                                direct=(dct["direct"] == "true" if "direct" in dct else None),
                                exclude_cmd=(dct["exclude_cmd"] if "exclude_cmd" in dct else None),
                                exclude_pattern=(dct["exclude_pattern"] if "exclude_pattern" in dct else None),
                                negate_pattern=(dct["negate_pattern"] if "negate_pattern" in dct else False))

    def get_ext_paths(self, paths) -> "List[libcas.DepsParam|str]":
        if len(paths) > 0:
            for i, path in enumerate(paths):
                if (path.startswith("(") or path.startswith("[")) and (path.endswith(")") or path.endswith("]")):
                    path = path.replace('[', '(').replace(']', ')')
                    for _ in range(path.count(" ")):
                        path = path.replace("( ", "(").replace(" (", "(").replace(") ", ")").replace(" )", ")")
                    paths[i] = self.expand_to_deps_param(path if path.startswith("(") else "(file={})".format(path))
        return paths

    @staticmethod
    def check_required(required_args:List[str], args: argparse.Namespace):
        """
        Function check if required parameters are valid.

        :param required_args: list of required args
        :type required_args: List[str]
        :param args: parsed commandline args
        :type args: argparse.Namespace
        :raises ArgumentException: if required args are missing, or have wrong count
        """

        if len(required_args) > 0:
            missing_args = [k.split(":")[0] for k in required_args if not getattr(args, k.split(":")[0])]
            if len(missing_args) > 0:
                raise ArgumentException("Missing required args '{}'".format(",".join(missing_args)))
            for k in required_args:
                k_v = k.split(":")
                if len(k_v) == 2:
                    if k_v[1] == "+":
                        if len(getattr(args, k_v[0])) < 1:
                            raise ArgumentException("Argument '{}' wrong count - provided {} should be at least {} ".format(k_v[0], len(getattr(args, k_v[0])), 1))
                    elif not k_v[1].endswith("+") and len(getattr(args, k_v[0])) != int(k_v[1]):
                        raise ArgumentException("Argument '{}' wrong count - provided {} should be {} ".format(k_v[0], len(getattr(args, k_v[0])), k_v[1]))
                    elif k_v[1].endswith("+"):
                        if len(getattr(args, k_v[0])) < int(k_v[1].replace("+", "")):
                            raise ArgumentException("Argument '{}' wrong count - provided {} should be at least {} ".format(k_v[0], len(getattr(args, k_v[0])), int(k_v[1].replace("+", ""))))

    def render(self):
        """
        Function used to render data records.
        Checks for required parameters, constructing filter and query data.

        :return: rendered data
        :rtype: str
        """

        self.check_required(self.required_args, self.args)

        if self.args.open_filter:
            self.open_filter = OpenFilter(self.args.open_filter, self, self.config, self.source_root)
        if self.args.command_filter:
            self.command_filter = CommandFilter(self.args.command_filter, self, self.config, self.source_root)
        if self.args.ftdb_simple_filter:
            self.ftdb_simple_filter = FtdbSimpleFilter(self.args.ftdb_simple_filter, self, self.config, self.source_root)

        try:
            data, output_type, sort_lambda, pipe_type = self.get_data()
        except libetrace.error as exc:
            raise LibetraceException("libetrace.error : {}".format(str(exc)))


        output_engine = self.get_output_engine()(data, self.args, self, output_type, sort_lambda)
        return output_engine.render_data()

    def get_output_engine(self):
        output_renderers = get_output_renderers()

        for name in get_output_renderers():
            if getattr(self.args, name, False):
                return output_renderers[name].Renderer
        return output_renderers['plain'].Renderer

    def should_display_open(self, ent: libetrace.nfsdbEntryOpenfile) -> bool:
        if self.args.generate:
            if self.args.all:
                return True
            else:
                return ent.opaque is not None and ent.opaque.compilation_info is not None
        else:
            return True

    def should_display_exe(self, ent: libetrace.nfsdbEntry) -> bool:
        if self.args.generate:
            if self.args.all:
                return True
            else:
                return ent.compilation_info is not None
        else:
            return True

    def get_exec_of_open(self, ent: libetrace.nfsdbEntryOpenfile) -> libetrace.nfsdbEntry:
        if self.args.generate and not self.args.all and ent.opaque is not None:
            return ent.opaque
        else:
            return ent.parent


class FilterableModule:
    pass

class PipedModule:
    @abstractmethod
    def set_piped_arg(self, data, data_type:type) -> None:
        """
        Function modifies input path argument considering previous data.
        """

class UnknownModule(Module):
    def render(self):
        return "ERROR: Unknown Module '{}'".format(self.args.module)
