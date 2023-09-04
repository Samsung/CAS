from client.mod_base import Module, PipedModule, FilterableModule
from client.misc import printdbg
from client.output_renderers.output import DataTypes


class LinkedModules(Module, PipedModule, FilterableModule):
    """Linked modules - returns list of files created by linker process."""

    @staticmethod
    def get_argparser():
        return Module.add_args([
            "filter", "command-filter", "select", "append",
            "details", "commands",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            "revdeps",
            "rdm",
            "recursive"], LinkedModules)

    def select_subject(self, ent) -> str:
        return ent.path

    def exclude_subject(self, ent) -> str:
        return ent.path

    def subject(self, ent):
        if self.args.show_commands:
            return ent.parent
        else:
            return ent.path

    def set_piped_arg(self, data, data_type):
        if data_type == "str":
            printdbg("DEBUG: accepting {} as args.pipe_path".format(data_type), self.args)
            self.args.pipe_path = data
        if data_type == "nfsdbEntryOpenfile":
            printdbg("DEBUG: accepting {} as args.pipe_path".format(data_type), self.args)
            self.args.pipe_path = list({o.path for o in data})
        if data_type == "nfsdbEntry":
            printdbg("DEBUG: accepting {} as args.pipe_path".format(data_type), self.args)
            self.args.pipe_path = list({ex.linked_file for ex in data if ex.linked_file is not None})

    def get_data(self) -> tuple:
        if self.args.show_commands:
            data = list({
                o.opaque
                for o in self.nfsdb.linked_modules()
                if o.opaque is not None and self.filter_open(o) and self.filter_exec(o.opaque)
            } if self.needs_open_filtering() or self.needs_exec_filtering() else {
                o.parent
                for o in self.nfsdb.linked_modules()
            })

            return data, DataTypes.commands_data, lambda x: x.eid.pid
        elif self.args.details:
            data = list({
                o
                for o in self.nfsdb.linked_modules()
                if self.filter_open(o)
            }) if self.needs_open_filtering() else list({
                o
                for o in self.nfsdb.linked_modules()
            })

            return data, DataTypes.linked_data, None
        else:
            data = list({
                o.path
                for o in self.nfsdb.linked_modules()
                if self.filter_open(o)
            }) if self.needs_open_filtering() else list({
                o.path
                for o in self.nfsdb.linked_modules()
            })
            if self.args.cdm:
                data = self.get_cdm(data)
                return data, DataTypes.cdm_data, None
            if self.args.rdm:
                data = self.get_rdm(data)
                return data, DataTypes.rdm_data, None

            return data, DataTypes.linked_data, None


class ModDepsFor(Module, PipedModule, FilterableModule):
    """Modules dependency - returns module dependency of given linked file(s)"""

    required_args = ["path:1+"]

    @staticmethod
    def get_argparser():
        return Module.add_args([
            "filter", "command-filter", "select", "append",
            "details", "commands",
            "pid", "binary", "path",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            "revdeps",
            "rdm",
            "recursive",
            "match",
            "with-children",
            "extended",
            "direct",
            "parent",
            "cached"], ModDepsFor)

    def select_subject(self, ent) -> str:
        return ent.path

    def exclude_subject(self, ent) -> str:
        return ent.path

    def subject(self, ent) -> str:
        return ent.path if not self.args.show_commands else ent

    def set_piped_arg(self, data, data_type):
        if data_type == "str":
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = data
        if data_type == "nfsdbEntryOpenfile":
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = list({o.path for o in data})
        if data_type == "nfsdbEntry":
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = list({ex.linked_file for ex in data})

    def get_data(self) -> tuple:
        linked_modules = {x.path for x in self.nfsdb.linked_modules()}

        if self.args.show_commands:
            data = list({
                o.parent if self.args.all else o.opaque
                for o in self.nfsdb.get_module_dependencies(self.args.path, direct=self.args.direct)
                if o.path in linked_modules and (self.args.all or o.opaque is not None) and self.filter_open(o) and self.filter_exec(o.parent)
            } if self.args.generate else {
                o.parent
                for o in self.nfsdb.get_module_dependencies(self.args.path, direct=self.args.direct)
                if o.path in linked_modules and self.filter_open(o) and self.filter_exec(o.parent)
            })

            return data, DataTypes.commands_data, lambda x: x.eid.pid
        elif self.args.details:
            data = list({
                o
                for o in self.nfsdb.get_module_dependencies(self.args.path, direct=self.args.direct)
                if o.path in linked_modules and self.filter_open(o)
            })

            return data, DataTypes.file_data, lambda x: x.path
        else:
            data = list({
                o.path
                for o in self.nfsdb.get_module_dependencies(self.args.path, direct=self.args.direct)
                if o.path in linked_modules and self.filter_open(o) and o.path not in self.args.path
            })

            return data, DataTypes.file_data, None


class RevModDepsFor(Module, PipedModule, FilterableModule):
    """Reverse modules dependency - returns list of files that depend on given linked file(s)"""
    required_args = ["path:1+"]

    @staticmethod
    def get_argparser():
        return Module.add_args([
            "filter", "command-filter", "select", "append",
            "details", "commands",
            "pid", "binary", "path",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            "rdm",
            "recursive",
            "match",
            "with-children",
            "extended",
            "direct",
            "parent",
            "cached"], RevModDepsFor)

    def select_subject(self, ent) -> str:
        return ent.path

    def exclude_subject(self, ent) -> str:
        return ent.path

    def subject(self, ent) -> str:
        return ent.path if not self.args.show_commands else ent

    def set_piped_arg(self, data, data_type):
        if data_type == "str":
            printdbg("DEBUG: accepting {} as args.pipe_path".format(data_type), self.args)
            self.args.pipe_path = data
        if data_type == "nfsdbEntryOpenfile":
            printdbg("DEBUG: accepting {} as args.pipe_path".format(data_type), self.args)
            self.args.pipe_path = list({o.path for o in data})
        if data_type == "nfsdbEntry":
            printdbg("DEBUG: accepting {} as args.pipe_path".format(data_type), self.args)
            self.args.pipe_path = list({ex.linked_file for ex in data})

    def get_data(self) -> tuple:

        if self.args.show_commands:
            data = list({
                o.parent if self.args.all else o.opaque
                for o in self.get_reverse_dependencies_opens(self.args.path, recursive=self.args.recursive)
                if (self.args.all or o.opaque is not None) and self.filter_open(o) and self.filter_exec(o.parent if self.args.all else o.opaque)
            } if self.args.generate else {
                o.parent
                for o in self.get_reverse_dependencies_opens(self.args.path, recursive=self.args.recursive)
                if self.filter_open(o) and self.filter_exec(o.parent)
            })

            return data, DataTypes.commands_data, lambda x: x.eid.pid
        elif self.args.details:
            data = list({
                o
                for o in self.get_reverse_dependencies_opens(self.args.path, recursive=self.args.recursive)
                if self.filter_open(o)
            })

            return data, DataTypes.file_data, lambda x: x.path
        else:
            data = list({
                o.path
                for o in self.get_reverse_dependencies_opens(self.args.path, recursive=self.args.recursive)
                if self.filter_open(o)
            })

            return data, DataTypes.file_data, None
