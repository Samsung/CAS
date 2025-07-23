from abc import abstractmethod
import itertools
from enum import IntEnum
from typing import Iterator, Callable

import libetrace
from client.exceptions import ParameterException
from client.misc import fix_cmd_makefile

class DataTypes(IntEnum):
    # List values
    linked_data = 1
    file_data = 2
    process_data = 3
    binary_data = 4
    commands_data = 5
    compiled_data = 6
    compilation_info_data = 7
    rdm_data = 8
    dep_graph_data = 9
    cdm_data = 10
    compilation_db_data = 11
    sources_data = 12
    modules_data = 13
    function_data = 14
    global_data = 15
    type_data = 16
    funcdecl_data = 17

    # Dict view
    config_data = 21
    stat_data = 22
    config_part_data = 23

    # Single string values
    source_root_data = 101
    root_pid_data = 102
    dbversion_data = 103
    module_name_data = 105
    dir_name_data = 106
    release_name_data = 107

    null_data = 104


class OutputRenderer:
    default_entries_count = 0

    def __init__(self, data, args, origin, output_type: DataTypes, sort_lambda: Callable) -> None:
        self.args = args
        self.data = data
        self.count = len(data) if isinstance(data, list) or isinstance(data, Iterator) else -1
        self.sort_lambda = self.get_sorting_lambda(sort_lambda)

        if self.args.entries_per_page is None:
            self.args.entries_per_page = self.default_entries_count

        self.origin_module = origin
        self.output_type = output_type
        self.output_renderer = self.assign_renderer()
        assert False if self.output_renderer is None else True

    def assign_renderer(self):
        return {
            DataTypes.linked_data: self.linked_data_renderer,
            DataTypes.file_data: self.file_data_renderer,
            DataTypes.process_data: self.process_data_renderer,
            DataTypes.binary_data: self.binary_data_renderer,
            DataTypes.commands_data: self.commands_data_renderer,
            DataTypes.compiled_data: self.compiled_data_renderer,
            DataTypes.compilation_info_data: self.compilation_info_data_renderer,
            DataTypes.compilation_db_data: self.compilation_db_data_renderer,
            DataTypes.rdm_data: self.rdm_data_renderer,
            DataTypes.cdm_data: self.cdm_data_renderer,
            DataTypes.dep_graph_data: self.dep_graph_data_renderer,
            DataTypes.source_root_data: self.source_root_data_renderer,
            DataTypes.root_pid_data: self.root_pid_data_renderer,
            DataTypes.dbversion_data: self.dbversion_data_renderer,
            DataTypes.module_name_data: self.modulename_data_renderer,
            DataTypes.dir_name_data: self.dirname_data_renderer,
            DataTypes.release_name_data: self.releasename_data_renderer,
            DataTypes.config_data: self.config_data_renderer,
            DataTypes.stat_data: self.stat_data_renderer,
            DataTypes.config_part_data: self.config_part_data_renderer,
            DataTypes.null_data: self.null_data_renderer,
            DataTypes.sources_data: self.sources_data_renderer,
            DataTypes.modules_data: self.modules_data_renderer,
            DataTypes.function_data: self.function_data_renderer,
            DataTypes.global_data: self.global_data_renderer,
            DataTypes.funcdecl_data: self.funcdecl_data_renderer,
            DataTypes.type_data: self.types_data_renderer
        }[self.output_type]

    def get_sorting_lambda(self, original_sort_lambda):
        if self.args.sorted and isinstance(self.data, list) and self.count > 0:
            if isinstance(self.data[0], libetrace.nfsdbEntry):
                if self.args.sorting_key is not None:
                    if self.args.sorting_key == "bin":
                        return lambda x: x.bpath
                    elif self.args.sorting_key == "cwd":
                        return lambda x: x.cwd
                    elif self.args.sorting_key == "cmd":
                        return lambda x: " ".join(x.argv)
                    else:
                        raise ParameterException("Wrong sorted-key value! Allowed [ bin, cwd, cmd ]")

            if isinstance(self.data[0], libetrace.nfsdbEntryOpenfile):
                if self.args.sorting_key is not None:
                    if self.args.sorting_key == "mode":
                        return lambda x: x.mode
                    if self.args.sorting_key == "path":
                        return lambda x: x.path
                    if self.args.sorting_key == "original_path":
                        return lambda x: x.original_path
                    else:
                        raise ParameterException("Wrong sorted-key value! Allowed [ path, original_path, mode ]")

        return original_sort_lambda

    def render_data(self):
        if self.args.count:
            return self.count_renderer()
        if not self.args.count and self.args.sorted and self.output_type.value < DataTypes.config_data.value:
            self.data = sorted(self.data, key=self.sort_lambda, reverse=self.args.reverse)

        if self.args.range:
            parts = self.args.range.replace("[", "").replace("]", "").split(":")
            start = None
            stop = None
            step = None
            if len(parts) > 0:
                start = int(parts[0]) if parts[0] != '' else 0
            if len(parts) > 1:
                stop = int(parts[1]) if parts[1] != '' else None
            if len(parts) > 2:
                step = int(parts[2]) if parts[2] != '' else None

            if isinstance(self.data, list):
                if len(parts) == 1 and start is not None:
                    self.data = [self.data[start]]
                elif len(parts) == 2:
                    self.data = self.data[start: stop]
                elif len(parts) == 3:
                    self.data = self.data[start:stop:step]
                else:
                    raise ParameterException("Wrong range!")


            elif isinstance(self.data, Iterator):
                if len(parts) == 1:
                    self.data = list(itertools.islice(self.data, start, start))
                elif len(parts) == 2:
                    self.data = list(itertools.islice(self.data, start, stop))
                elif len(parts) == 3:
                    self.data = list(itertools.islice(self.data, start, stop, step))
                else:
                    raise ParameterException("Wrong range!")

        elif self.args.entries_per_page != 0:
            if isinstance(self.data, list) or isinstance(self.data, Iterator):
                if self.count < (self.args.page * self.args.entries_per_page):
                    self.args.page = int(self.count/self.args.entries_per_page)

                if isinstance(self.data, list):
                    self.data = self.data[self.args.page * self.args.entries_per_page: (self.args.page + 1) * self.args.entries_per_page]
                elif isinstance(self.data, Iterator):
                    self.data = list(itertools.islice(self.data, self.args.page * self.args.entries_per_page,(self.args.page + 1) * self.args.entries_per_page))

        if not self.args.plain:
            self.num_entries = len(self.data) if isinstance(self.data, list) or isinstance(self.data, Iterator) else -1

        return self.output_renderer()

    @abstractmethod
    def count_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def linked_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def file_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def process_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def binary_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def commands_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def compiled_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def compilation_info_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def rdm_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def cdm_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def compilation_db_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def dep_graph_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def source_root_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def root_pid_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def dbversion_data_renderer(self):
        self.assert_not_implemented()
    
    @abstractmethod
    def modulename_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def dirname_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def releasename_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def config_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def stat_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def config_part_data_renderer(self):
        self.assert_not_implemented()
    
    @abstractmethod
    def sources_data_renderer(self):
        self.assert_not_implemented()
    
    @abstractmethod
    def modules_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def function_data_renderer(self):
        self.assert_not_implemented()
    
    @abstractmethod
    def global_data_renderer(self):
        self.assert_not_implemented()
    
    @abstractmethod
    def funcdecl_data_renderer(self):
        self.assert_not_implemented()
    
    @abstractmethod
    def types_data_renderer(self):
        self.assert_not_implemented()

    def null_data_renderer(self):
        pass

    def makefile_data_renderer(self):
        length = len(self.data)
        if not self.args.all or self.args.static:
            yield ".PHONY: all"
            for i in range(0, len(self.data)):
                yield f".PHONY: comp_{i}"
            yield "all: {}".format(" ".join([f"comp_{i}" for i in range(0, len(self.data))]))
            yield "\t@echo Done!"
            for i, exe in enumerate(self.data):
                yield f"comp_{i}:"
                yield "\t@echo \"{} {}\"".format("CC" if exe.compilation_info.type == 1 else "CXX", exe.compilation_info.objects[0].path if len(exe.compilation_info.objects) > 0 else "--")
                cmd = ("$(CC) {}" if exe.compilation_info.type == 1 else "$(CXX) {}").format(fix_cmd_makefile(exe.argv, self.args.static))
                yield f"\t@(cd {exe.cwd} && {cmd})"
        elif self.args.all and not self.args.static:
            yield ".PHONY: all"
            yield "ADDITIONAL_OPTS_PREFIX:="
            yield "ADDITIONAL_OPTS_POSTFIX:="
            for i in range(0, length):
                yield ".PHONY: cmd_%d" % i
            yield "all: {}\n\t@echo Done!\n".format(" ".join(["cmd_%d" % i for i in range(0, length)]))
            for i, exe in enumerate(self.data):
                yield "cmd_%d:" % i
                yield "\t@echo \"%s %d / %d\"" % ("CMD", i, length)
                yield "\t@(cd %s && $(ADDITIONAL_OPTS_PREFIX)%s)" % (exe.cwd, fix_cmd_makefile(exe.argv)+" $(ADDITIONAL_OPTS_POSTFIX)")

    def assert_not_implemented(self):
        assert False, "This view is not implemented in target output!"
