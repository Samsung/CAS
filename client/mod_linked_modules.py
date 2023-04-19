import argparse
from client.mod_base import Module, PipedModule
from client.misc import printdbg
from client.output_renderers.output import DataTypes
from client.argparser import add_args


class LinkedModules(Module, PipedModule):
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="Linked modules displays list of linked files.")
        arg_group = module_parser.add_argument_group("Module arguments")
        add_args( [
            "filter", "select", "append",
            "details", "commands",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            "revdeps",
            "rdm",
            "recursive",
            "link-type"]
            ,arg_group)
        return module_parser

    def select_subject(self, ent) -> str:
        return ent.path

    def exclude_subject(self, ent) -> str:
        return ent.path

    def subject(self, ent) -> str:
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
            self.args.pipe_path = list({ex.linked_file for ex in data})

    def get_data(self) -> tuple:
        if self.args.show_commands:
            data = list({
                o.parent
                for o in self.nfsdb.linked_modules()
                if self.filter_open(o)
            } if not self.args.generate else {
                o.parent
                for o in self.nfsdb.linked_modules()
                if (o.parent.compilation_info is not None or self.args.all) and self.filter_open(o)
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


class ModDepsFor(Module, PipedModule):
    required_args = ["path:1+"]

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

    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION ")
        arg_group = module_parser.add_argument_group("Dependency generation arguments")
        add_args( [
            "filter", "select", "append",
            "details", "commands",
            "pid", "binary", "path",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            "revdeps",
            "rdm",
            "recursive",
            "link-type",
            "match",
            "with-children",
            "extended",
            "direct",
            "parent",
            "cached"]
            ,arg_group)
        return module_parser

    def select_subject(self, ent) -> str:
        return ent.path

    def exclude_subject(self, ent) -> str:
        return ent.path

    def subject(self, ent) -> str:
        return ent.path if not self.args.show_commands else ent

    def get_data(self) -> tuple:
        linked_modules = {x.path for x in self.nfsdb.linked_modules()}

        if self.args.show_commands:
            data = list({
                o.parent if self.args.all else o.opaque
                for o in self.nfsdb.get_module_dependencies(self.args.path, direct=self.args.direct)
                if o.path in linked_modules and (self.args.all or o.opaque is not None) and self.filter_open(o)
            } if self.args.generate else {
                o.parent
                for o in self.nfsdb.get_module_dependencies(self.args.path, direct=self.args.direct)
                if o.path in linked_modules and self.filter_open(o)
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


class RevModDepsFor(Module, PipedModule):
    required_args = ["path:1+"]

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

    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION ")
        arg_group = module_parser.add_argument_group("Dependency generation arguments")
        add_args( [
            "filter", "select", "append",
            "details", "commands",
            "pid", "binary", "path",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            "rdm",
            "recursive",
            "link-type",
            "match",
            "with-children",
            "extended",
            "direct",
            "parent",
            "cached"]
            ,arg_group)
        return module_parser

    def select_subject(self, ent) -> str:
        return ent.path

    def exclude_subject(self, ent) -> str:
        return ent.path

    def subject(self, ent) -> str:
        return ent.path if not self.args.show_commands else ent

    def get_data(self) -> tuple:

        if self.args.show_commands:
            data = list({
                o.parent if self.args.all else o.opaque
                for o in self.get_reverse_dependencies_opens(self.args.path,recursive=self.args.recursive)
                if (self.args.all or o.opaque is not None) and self.filter_open(o) and self.filter_exec(o.parent if self.args.all else o.opaque)
            } if self.args.generate else {
                o.parent
                for o in self.get_reverse_dependencies_opens(self.args.path,recursive=self.args.recursive)
                if self.filter_open(o) and self.filter_exec(o.parent)
            })

            return data, DataTypes.commands_data, lambda x: x.eid.pid
        elif self.args.details:
            data = list({
                o
                for o in self.get_reverse_dependencies_opens(self.args.path,recursive=self.args.recursive)
                if self.filter_open(o)
            })

            return data, DataTypes.file_data, lambda x: x.path
        else:
            data = list({
                o.path
                for o in self.get_reverse_dependencies_opens(self.args.path,recursive=self.args.recursive)
                if self.filter_open(o)
            })

            return data, DataTypes.file_data, None
