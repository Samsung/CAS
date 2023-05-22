import argparse
import sys
from client.mod_base import Module, PipedModule
from client.misc import printdbg
from client.output_renderers.output import DataTypes
from client.argparser import add_args


class Faccess(Module, PipedModule):
    """
    Module get process list that referenced given file path(s)
    """
    required_args = ["path:1+"]

    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="File access module displays all processes that read given path(s) with access mode information.")
        arg_group = module_parser.add_argument_group("File access module generation arguments")
        add_args([
            "filter", "select", "append",
            "details", "commands",
            "path"
            ], arg_group)
        return module_parser

    def select_subject(self, ent) -> str:
        return self.subject(ent)

    def exclude_subject(self, ent) -> str:
        return self.subject(ent)

    def subject(self, ent) -> str:
        return ent.original_path if self.args.original_path else ent.path

    def set_piped_arg(self, data, data_type):
        if data_type == "str":
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = data
        if data_type == "nfsdbEntryOpenfile":
            printdbg("DEBUG: accepting {} as args.path".format(data_type), self.args)
            self.args.path = list({o.path for o in data})
        if data_type == "nfsdbEntry":
            print("ERROR: Wrong pipe input data {}.".format(data_type))
            sys.exit(2)

    def get_data(self) -> tuple:
        if self.args.show_commands:
            data = list({
                self.get_exec_of_open(o)
                for o in self.nfsdb.get_opens_of_path(self.args.path)
                if self.filter_exec(self.get_exec_of_open(o)) and self.filter_open(o)
            })
            return data, DataTypes.commands_data, lambda x: x.eid.pid
        elif self.args.details:
            data = list({
                o
                for o in self.nfsdb.get_opens_of_path(self.args.path)
                if self.filter_open(o)
            })
            return data, DataTypes.file_data, lambda x: x.parent.eid.pid
        else:
            data = list({
                (o.parent.eid.pid, o.mode)
                for o in self.nfsdb.get_opens_of_path(self.args.path)
                if self.filter_open(o)
            })
            return data, DataTypes.process_data, lambda x: x[0]


class ProcRef(Module, PipedModule):
    """
    Module used to get files referenced by given process.
    """
    required_args = ["pid:1+"]

    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION")
        arg_group = module_parser.add_argument_group("Process references module arguments")
        add_args([
            "filter", "select", "append",
            "details", "commands",
            "pid",
            "with-children",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            "revdeps",
            "rdm",
            "recursive",
            "link-type"], arg_group)
        return module_parser

    def select_subject(self, ent) -> str:
        return self.subject(ent)

    def exclude_subject(self, ent) -> str:
        return self.subject(ent)

    def subject(self, ent) -> str:
        return ent.original_path if self.args.original_path else ent.path

    def set_piped_arg(self, data, data_type):
        if data_type == "str":
            try:
                self.args.pid = [int(d) for d in data]
                printdbg("DEBUG: accepting {} as args.pid".format(data_type), self.args)
            except ValueError:
                print("ERROR: Wrong pipe input data - not numerical!")
                sys.exit(2)
        elif data_type == "nfsdbEntry":
            printdbg("DEBUG: accepting {} as args.pid".format(data_type), self.args)
            self.args.pid = list({ex.eid.pid for ex in data})
        else:
            print("ERROR: Wrong pipe input type {}".format(data_type))
            sys.exit(2)

    def get_data(self) -> tuple:
        if self.args.show_commands:
            data = list({
                ent
                for ent in self.nfsdb.get_pids([(pid,) for pid in self.args.pid])
                if self.filter_exec(ent)
            })
            return data, DataTypes.commands_data, lambda x: x.eid.pid
        elif self.args.details:
            data = list(set(self.yield_open_from_pid([(pid,) for pid in self.args.pid])))
            return data, DataTypes.file_data, lambda x: x.path
        else:
            data = list(set(self.yield_path_from_pid([(pid,) for pid in self.args.pid])))
            if self.args.rdm:
                data = self.get_rdm(data)
                return data, DataTypes.rdm_data, lambda x: x[0]
            if self.args.cdm:
                data = self.get_cdm(data)
                return data, DataTypes.cdm_data, lambda x: x[0]
            if self.args.revdeps:
                data = self.get_revdeps(data)
                return data, DataTypes.file_data, lambda x: x[0]

            return data, DataTypes.file_data, None


class RefFiles(Module):
    """
    Module used to get referenced files.
    """

    @staticmethod
    def get_argparser():
        parser = argparse.ArgumentParser(description="This module is used to query all files referenced during traced process execution.")
        arg_group = parser.add_argument_group("Referenced Files arguments")
        add_args([
            "filter", "select", "append",
            "details", "commands",
            "with-children",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            "revdeps",
            "rdm",
            "recursive",
            "link-type", "cdb"], arg_group)
        return parser

    def select_subject(self, ent) -> str:
        return self.subject(ent)

    def exclude_subject(self, ent) -> str:
        return self.subject(ent)

    def subject(self, ent) -> "str | nfsdbEntryOpenfile":
        if self.args.show_commands:
            return ent.binary
        elif self.args.details:
            return ent
        else:
            return ent.path

    def get_data(self) -> tuple:
        if self.args.show_commands:
            data = list({
                self.get_exec_of_open(o)
                for o in self.nfsdb.opens_iter()
                if self.filter_open(o)
            } if self.needs_open_filtering() else {
                self.get_exec_of_open(o)
                for o in self.nfsdb.opens_iter()
            })
            if self.args.cdb:
                data = list(self.cdb_fix_multiple(data))
                return data, DataTypes.compilation_db_data, lambda x: x['filename']
            return data, DataTypes.commands_data, lambda x: x.eid.pid
        elif self.args.details:
            data = [
                o
                for o in self.nfsdb.opens_iter()
                if self.filter_open(o)
            ] if self.needs_open_filtering() else [
                o
                for o in self.nfsdb.opens_iter()
            ]
            return data, DataTypes.file_data, lambda x: x.path
        else:
            data = list({
                o.path
                for o in self.nfsdb.opens_iter()
                if self.filter_open(o)
            } if self.needs_open_filtering() else
                self.nfsdb.opens_list()
            )

            if self.args.rdm:
                data = self.get_rdm(data)
                return data, DataTypes.rdm_data, lambda x: x[0]
            if self.args.cdm:
                data = self.get_cdm(data)
                return data, DataTypes.cdm_data, lambda x: x[0]
            if self.args.revdeps:
                data = self.get_revdeps(data)
                return data, DataTypes.file_data, lambda x: x.path

            return data, DataTypes.file_data, None
