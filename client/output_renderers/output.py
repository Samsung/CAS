from abc import abstractmethod
from enum import Enum
from types import LambdaType


class DataTypes(Enum):
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

    # Dict view
    config_data = 21
    stat_data = 22
    config_part_data = 23

    # Single string values
    source_root_data = 101
    root_pid_data = 102
    dbversion_data = 103

    null_data = 104


class OutputRenderer:
    default_entries_count = 0

    def __init__(self, data, args, origin, output_type: DataTypes, sort_lambda:LambdaType) -> None:
        self.args = args
        self.num_entries = len(data) if isinstance(data, list) else -1

        if self.args.entries_per_page is None:
            self.args.entries_per_page = self.default_entries_count

        if self.args.range:
            parts = self.args.range.replace("[","").replace("]","").split(":")
            if len(parts) == 1:
                self.data = [data[int(parts[0])]]
            elif len(parts) == 2:
                self.data = data[int(parts[0]) if parts[0] != '' else None:int(parts[1]) if parts[1] != '' else None]
            elif len(parts) == 3:
                self.data = data[int(parts[0]) if parts[0] != '' else None:int(parts[1]) if parts[1] != '' else None:int(parts[2]) if parts[2] != '' else None]
            else:
                assert False, "Wrong range!"
        else:
            if isinstance(data, list) and self.args.entries_per_page != 0:
                self.data = data[self.args.page * self.args.entries_per_page:(self.args.page + 1) * self.args.entries_per_page]
            else:
                self.data = data

        self.count = len(self.data) if isinstance(self.data, list) else -1
        self.origin_module = origin
        self.output_type = output_type
        self.output_renderer = self.assign_renderer()
        self.sort_lambda = sort_lambda
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
            DataTypes.rdm_data: self.rdm_data_renderer,
            DataTypes.cdm_data: self.cdm_data_renderer,
            DataTypes.dep_graph_data: self.dep_graph_data_renderer,
            DataTypes.source_root_data: self.source_root_data_renderer,
            DataTypes.root_pid_data: self.root_pid_data_renderer,
            DataTypes.dbversion_data: self.dbversion_data_renderer,
            DataTypes.config_data: self.config_data_renderer,
            DataTypes.stat_data: self.stat_data_renderer,
            DataTypes.config_part_data: self.config_part_data_renderer,
            DataTypes.null_data: self.null_data_renderer
        }[self.output_type]

    def render_data(self):
        if self.args.sorted and self.output_type.value < DataTypes.config_data.value:
            self.data = sorted(self.data, key=self.sort_lambda)
        return self.output_renderer()

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
    def config_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def stat_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def config_part_data_renderer(self):
        self.assert_not_implemented()

    @abstractmethod
    def null_data_renderer(self):
        pass

    def assert_not_implemented(self):
        assert False, "This view is not implemented in target output!"
