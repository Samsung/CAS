
from typing import Any, Tuple, Callable
from client.mod_base import Module, PipedModule, FilterableModule
from client.misc import printdbg
from client.output_renderers.output import DataTypes
from client.exceptions import PipelineException
import libetrace

class Faccess(Module, PipedModule, FilterableModule):
    """File access - returns all processes that read given path(s) with access mode information."""
    required_args = ["path:1+"]

    @staticmethod
    def get_argparser():
        return Module.add_args([
            "filter", "command-filter", "select", "append",
            "details", "commands",
            "path", "cdb"
            ], Faccess)

    def select_subject(self, ent) -> str:
        return self.subject(ent)

    def exclude_subject(self, ent) -> str:
        return self.subject(ent)

    def subject(self, ent) -> str:
        return ent.original_path if self.args.original_path else ent.path

    def set_piped_arg(self, data, data_type:type):
        if data_type == str:
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = data
        if data_type == libetrace.nfsdbEntryOpenfile:
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = list({o.path for o in data})
        if data_type == libetrace.nfsdbEntry:
            raise PipelineException("Wrong pipe input data {}.".format(data_type))

    def get_data(self) -> Tuple[Any, DataTypes, "Callable|None", "type|None"]:
        if self.args.show_commands:
            data = list({
                self.get_exec_of_open(o)
                for o in self.nfsdb.get_opens_of_path(self.args.path)
                if self.filter_open(o) and self.filter_exec(self.get_exec_of_open(o))
            })
            if self.args.cdb:
                data = list(self.cdb_fix_multiple(data))
                return data, DataTypes.compilation_db_data, lambda x: x['filename'], str
            return data, DataTypes.commands_data, lambda x: x.eid.pid,  libetrace.nfsdbEntry
        elif self.args.details:
            data = list({
                o
                for o in self.nfsdb.get_opens_of_path(self.args.path)
                if self.filter_open(o)
            })
            return data, DataTypes.file_data, lambda x: x.parent.eid.pid ,libetrace.nfsdbEntryOpenfile
        else:
            data = list({
                (o.parent.eid.pid, o.mode)
                for o in self.nfsdb.get_opens_of_path(self.args.path)
                if self.filter_open(o) and self.filter_exec(o.parent)
            })
            return data, DataTypes.process_data, lambda x: x[0], int


class ProcRef(Module, PipedModule, FilterableModule):
    """Process references - returns opens referenced by given process."""
    required_args = ["pid:1+"]

    @staticmethod
    def get_argparser():
        return Module.add_args([
            "filter", "command-filter", "select", "append",
            "details", "commands",
            "pid",
            "with-children",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            "revdeps",
            "rdm",
            "recursive", "cdb"], ProcRef)

    def select_subject(self, ent) -> str:
        return self.subject(ent)

    def exclude_subject(self, ent) -> str:
        return self.subject(ent)

    def subject(self, ent) -> str:
        return ent.original_path if self.args.original_path else ent.path

    def set_piped_arg(self, data, data_type:type):
        if data_type == str:
            try:
                self.args.pid = [int(d) for d in data]
                printdbg("DEBUG: accepting {} as args.pid".format(data_type), self.args)
            except ValueError:
                raise PipelineException("Wrong pipe input data - not numerical!")
        elif data_type == libetrace.nfsdbEntry:
            printdbg("DEBUG: accepting {} as args.pid".format(data_type), self.args)
            self.args.pid = list({ex.eid.pid for ex in data})
        else:
            raise PipelineException("Wrong pipe input data {}.".format(data_type))

    def get_data(self) -> Tuple[Any, DataTypes, "Callable|None", "type|None"]:
        if self.args.show_commands:
            data = list({ self.get_exec_of_open(o)
                for o in self.yield_open_from_pid([(int(pid),) for pid in self.args.pid])
                if self.filter_exec(self.get_exec_of_open(o)) and self.filter_open(o)
            })
            if self.args.cdb:
                data = list(self.cdb_fix_multiple(data))
                return data, DataTypes.compilation_db_data, lambda x: x['filename'],None
            return data, DataTypes.commands_data, lambda x: x.eid.pid, libetrace.nfsdbEntry
        elif self.args.details:
            data = list(set(self.yield_open_from_pid([(int(pid),) for pid in self.args.pid])))
            return data, DataTypes.file_data, lambda x: x.path, libetrace.nfsdbEntryOpenfile
        else:
            data = list(set(self.yield_path_from_pid([(int(pid),) for pid in self.args.pid])))
            if self.args.rdm:
                data = self.get_rdm(data)
                return data, DataTypes.rdm_data, lambda x: x[0],None
            if self.args.cdm:
                data = self.get_cdm(data)
                return data, DataTypes.cdm_data, lambda x: x[0],None
            if self.args.revdeps:
                data = self.get_revdeps(data)
                return data, DataTypes.file_data, lambda x: x[0],None

            return data, DataTypes.file_data, None, str


class RefFiles(Module, FilterableModule):
    """Referenced files - returns files referenced during traced process execution."""

    @staticmethod
    def get_argparser():
        return Module.add_args([
            "filter", "command-filter", "select", "append",
            "details", "commands",
            "with-children",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            "revdeps",
            "rdm",
            "recursive", "cdb"], RefFiles)

    def select_subject(self, ent) -> str:
        return self.subject(ent)

    def exclude_subject(self, ent) -> str:
        return self.subject(ent)

    def subject(self, ent):
        if self.args.show_commands:
            return ent.binary
        elif self.args.details:
            return ent
        else:
            return ent.path

    def get_data(self) -> Tuple[Any, DataTypes, "Callable|None", "type|None"]:
        if self.args.show_commands:
            if self.open_filter:
                data = list({
                        self.get_exec_of_open(o)
                        for o in self.nfsdb.filtered_opens_iter(self.open_filter.libetrace_filter)
                    })
            else:
                data = list({
                        self.get_exec_of_open(o)
                        for o in self.nfsdb.opens_iter()
                    })

            if self.command_filter:
                data = [ex for ex in data if self.filter_exec(ex)]

            if self.args.cdb:
                data = list(self.cdb_fix_multiple(data))
                return data, DataTypes.compilation_db_data, lambda x: x['filename'], None
            return data, DataTypes.commands_data, lambda x: x.eid.pid , libetrace.nfsdbEntry

        elif self.args.details:
            if self.open_filter:
                data = self.nfsdb.filtered_opens_iter(self.open_filter.libetrace_filter)
            else:
                data = self.nfsdb.opens_iter()

            if self.has_select:
                data = [o for o in data if o.path in self.args.select]

            return data, DataTypes.file_data, lambda x: x.path, libetrace.nfsdbEntryOpenfile

        else:
            if self.open_filter:
                data = self.nfsdb.filtered_paths_iter(file_filter=self.open_filter.libetrace_filter)
            else:
                data = self.nfsdb.opens_paths()

            if self.has_select:
                data = [o for o in data if o in self.args.select]

            if self.has_append:
                data += self.args.append

            if self.args.rdm:
                data = self.get_rdm(data)
                return data, DataTypes.rdm_data, lambda x: x[0], None
            if self.args.cdm:
                data = self.get_cdm(data)
                return data, DataTypes.cdm_data, lambda x: x[0], None
            if self.args.revdeps:
                data = self.get_revdeps(data)
                return data, DataTypes.file_data, lambda x: x.path, libetrace.nfsdbEntryOpenfile

            return data, DataTypes.file_data, lambda x: x,str
