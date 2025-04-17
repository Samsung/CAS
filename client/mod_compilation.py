from typing import Any, List, Tuple, Callable
from client.mod_base import Module, PipedModule, FilterableModule
from client.exceptions import PipelineException
from client.misc import printdbg
from client.output_renderers.output import DataTypes
import libetrace

class Compiled(Module, FilterableModule):
    """Compiled - Returns compiled file list."""

    @staticmethod
    def get_argparser():
        return Module.add_args([
            "filter", "command-filter", "select", "append",
            "details", "commands",
            "revdeps", "cdb", "nostdinc"], Compiled)

    def select_subject(self, ent) -> str:
        if self.args.show_commands:
            return ent.compilation_info.files[0].path
        else:
            return ent.original_path if self.args.original_path else ent.path

    def exclude_subject(self, ent) -> str:
        if self.args.show_commands:
            return ent.compilation_info.files[0].path
        else:
            return ent.original_path if self.args.original_path else ent.path

    def subject(self, ent) -> str:
        if self.args.show_commands:
            return ent.compilation_info.files[0].path
        else:
            return ent.original_path if self.args.original_path else ent.path

    def get_data(self) -> Tuple[Any, DataTypes, "Callable|None", "type|None"]:
        if self.args.show_commands:
            data = list({
                ent
                for ent in self.nfsdb.filtered_execs_iter(self.command_filter.libetrace_filter if self.command_filter else None, has_comp_info=True)
                for o in ent.compilation_info.files
                if self.filter_open(o) and self.filter_exec(ent)
            })
            if self.args.cdb:
                data = list(self.cdb_fix_multiple(data))
                return data, DataTypes.compilation_db_data, lambda x: x['filename'], None
            return data, DataTypes.commands_data, lambda x: x.compilation_info.files[0].path, libetrace.nfsdbEntry
        elif self.args.details:
            data = list({
                o
                for ent in self.nfsdb.filtered_execs_iter(self.command_filter.libetrace_filter if self.command_filter else None, has_comp_info=True)
                for o in ent.compilation_info.files
                if self.filter_open(o)
            })
            return data, DataTypes.compiled_data, lambda x: x.path, libetrace.nfsdbEntryOpenfile
        else:
            data = list({
                o.path
                for ent in self.nfsdb.filtered_execs_iter(self.command_filter.libetrace_filter if self.command_filter else None, has_comp_info=True)
                for o in ent.compilation_info.files
                if self.filter_open(o)
            })

            if self.args.revdeps:
                data = self.get_revdeps(data)
                return data, DataTypes.file_data, None, str

            return data, DataTypes.compiled_data, None, str


class RevCompsFor(Module, PipedModule, FilterableModule):
    """Reversed compilations - returns sources list used in compilation that in some point referenced given file."""

    required_args = ["path:1+"]

    @staticmethod
    def get_argparser():
        return Module.add_args([
            "filter", "command-filter", "select", "append",
            "details", "commands",
            "path",
            "revdeps", "rcm",
            "match", "cdb"], RevCompsFor)

    def select_subject(self, ent) -> str:
        return ent.compilation_info.files[0].path

    def exclude_subject(self, ent) -> str:
        return ent.compilation_info.files[0].path

    def subject(self, ent) -> str:
        return ent.compilation_info.files[0].path

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
        if self.args.show_commands or self.args.details:
            data = list({
                oo.opaque
                for opn in self.nfsdb.get_opens_of_path(self.args.path)
                for oo in opn.parent.parent.opens_with_children
                if oo.opaque is not None and oo.opaque.compilation_info and self.filter_open(oo) and self.filter_exec(oo.opaque)
            })
            return data, DataTypes.commands_data, lambda x: x.compilation_info.files[0], libetrace.nfsdbEntry
        elif self.args.rcm:
            data:List = []
            for p in self.args.path:
                data.append( [p, list({
                    oo.opaque.compilation_info.files[0].path
                    for opn in self.nfsdb.get_opens_of_path(p)
                    for oo in opn.parent.parent.opens_with_children
                    if oo.opaque is not None and oo.opaque.compilation_info and self.filter_open(oo)
                })]
                )
            return data, DataTypes.cdm_data, None, None
        else:
            data = list({
                oo.opaque.compilation_info.files[0].path
                for opn in self.nfsdb.get_opens_of_path(self.args.path)
                for oo in opn.parent.parent.opens_with_children
                if oo.opaque is not None and oo.opaque.compilation_info and self.filter_open(oo)
            })
            if self.args.revdeps:
                data = self.get_revdeps(data)
                return data, DataTypes.compiled_data, None, str

            return data, DataTypes.compiled_data, None, str


class CompilationInfo(Module, PipedModule, FilterableModule):
    """Compilation info - display extended compilation information."""

    required_args = ["path:1+"]

    @staticmethod
    def get_argparser():
        return Module.add_args(["command-filter", "path"], CompilationInfo)

    def subject(self, ent):
        pass

    def select_subject(self, ent):
        pass

    def exclude_subject(self, ent):
        pass

    def set_piped_arg(self, data, data_type:type):
        if data_type == str:
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = data
        if data_type == libetrace.nfsdbEntryOpenfile:
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = list({o.path for o in data})
        if data_type == libetrace.nfsdbEntry:
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = list({o.path for ent in data for o in ent.compilation_info.files if ent.compilation_info is not None})

    def get_data(self) -> Tuple[Any, DataTypes, "Callable|None", "type|None"]:
        data = list({opn
                    for opn in self.nfsdb.get_opens_of_path(self.args.path)
                    if opn.opaque is not None and opn.opaque.compilation_info is not None and self.filter_exec(opn.opaque)
                    })

        return data, DataTypes.compilation_info_data, lambda x: x.path, libetrace.nfsdbEntryOpenfile
