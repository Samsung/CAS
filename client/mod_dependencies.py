import argparse
import sys
from client.mod_base import Module
from client.misc import printdbg, printerr
from client.output_renderers.output import DataTypes
from client.argmap import add_args as add_args

class DepsFor(Module):
    required_args = ["path:1+"]

    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION", epilog="TODO EPILOG")
        g = module_parser.add_argument_group("Dependency generation arguments")
        add_args( [
            "filter", "select", "append",
            "details", "commands",
            "path",
            "dep-graph",
            "wrap-deps",
            "with-pipes",
            "timeout",
            "direct-global",
            "rdm",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            "revdeps",
            "recursive",
            "dry-run",
            "debug-fd",
            "match",
            "link-type",
            "cached"
            ]
            , g)
        return module_parser

    def subject(self, ent) -> str:
        if self.args.show_commands:
            return " ".join(ent.argv)
        else:
            return ent.path

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
        paths = self.get_ext_paths(self.args.path)
        self.args.path = None

        if self.args.dep_graph:
            data = self.get_dependency_graph(paths)
            return data, DataTypes.dep_graph_data, lambda x: x[0]
        elif self.args.show_commands:
            data = list({
                self.get_exec_of_open(d)
                for o in self.get_multi_deps(paths)
                for d in o
                if self.should_display(d)
            })

            return data, DataTypes.commands_data, lambda x: x.eid.pid
        elif self.args.details:
            data = list({
                d
                for o in self.get_multi_deps(paths)
                for d in o
                if self.filter_open(d)
            })
            return data, DataTypes.file_data, None
        else:
            data = list({
                d.path
                for o in self.get_multi_deps(paths)
                for d in o
                if self.filter_open(d)
            })
            if self.args.rdm:
                data = self.get_rdm(data)
                return data, DataTypes.rdm_data, lambda x: x[0]
            if self.args.cdm:
                data = self.get_cdm(data)
                return data, DataTypes.cdm_data, lambda x: x[0]
            if self.args.revdeps:
                data = self.get_revdeps(data)
                return data, DataTypes.file_data, None
            return data, DataTypes.file_data, None


class RevDepsFor(Module):
    required_args = ["path:1+"]

    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION", epilog="TODO EPILOG")
        g = module_parser.add_argument_group("Dependency generation arguments")
        add_args( [
            "filter", "select", "append",
            "details", "commands",
            "path",
            "recursive",
            "match",
            "cdm", "cdm-ex-pt", "cdm-ex-fl",
            ]
            , g)
        return module_parser

    def subject(self, ent) -> str:
        if self.args.show_commands:
            return " ".join(ent.argv)
        else:
            return ent.path

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

        rdeps = self.get_reverse_dependencies_opens(self.args.path, recursive=self.args.recursive)

        if self.args.show_commands:
            data = list({
                o.opaque
                for o in rdeps
                if self.filter_exec(o.opaque)
            })

            return data, DataTypes.commands_data, lambda x: x.eid.pid
        elif self.args.details:
            data = list({
                o
                for o in rdeps
                if self.filter_open(o)
            })

            return data, DataTypes.file_data, lambda x: x.path
        else:
            data = list({
                o.path
                for o in rdeps
                if self.filter_open(o)
            })

            if self.args.cdm:
                data = self.get_cdm(data)
                return data, DataTypes.cdm_data, lambda x: x[0]

            return data, DataTypes.file_data, None
