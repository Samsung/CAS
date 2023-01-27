import argparse
import sys
from client.mod_base import Module
from client.misc import printdbg
from client.output_renderers.output import DataTypes
from client.argmap import add_args as add_args

class Compiled(Module):
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION ", epilog="TODO EPILOG")
        g = module_parser.add_argument_group("Compiled files arguments")
        add_args( [
            "filter", "select", "append",
            "details", "commands",
            "revdeps"]
            , g)
        return module_parser

    def subject(self, ent) -> str:
        if self.args.show_commands:
            return ent.compilation_info.files[0].path
        else:
            return ent.original_path if self.args.original_path else ent.path

    def get_data(self) -> tuple:
        if self.args.show_commands:
            data = list({
                ent
                for ent in self.nfsdb.get_execs_filtered(has_comp_info=True)
                for o in ent.compilation_info.files
                if self.filter_open(o) and self.filter_exec(ent)
            })
            return data, DataTypes.commands_data, lambda x: x.compilation_info.files[0].path
        elif self.args.details:
            data = list({
                o
                for ent in self.nfsdb.get_execs_filtered(has_comp_info=True)
                for o in ent.compilation_info.files
                if self.filter_open(o)
            })
            return data, DataTypes.compiled_data, lambda x: x.path
        else:
            data = list({
                o.path
                for ent in self.nfsdb.get_execs_filtered(has_comp_info=True)
                for o in ent.compilation_info.files
                if self.filter_open(o)
            })

            if self.args.revdeps:
                data = self.get_revdeps(data)
                return data, DataTypes.file_data, None

            return data, DataTypes.compiled_data, None


class RevCompsFor(Module):
    required_args = ["path:1+"]

    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION ", epilog="TODO EPILOG")
        g = module_parser.add_argument_group("Reverse compilation arguments")
        add_args( [
            "filter", "select", "append",
            "details", "commands",
            "path",
            "revdeps",
            "match"]
            , g)
        return module_parser

    def subject(self, ent) -> str:
        return ent.compilation_info.files[0].path

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

        if self.args.show_commands or self.args.details:
            data = list({
                oo.opaque
                for opn in self.nfsdb.get_opens_of_path(self.args.path)
                for oo in opn.parent.parent.opens_with_children
                if oo.opaque is not None and oo.opaque.compilation_info and self.filter_open(oo)
            })

            return data, DataTypes.compiled_data, lambda x: x.compilation_info.files[0]
        else:
            data = list({
                oo.opaque.compilation_info.files[0].path
                for opn in self.nfsdb.get_opens_of_path(self.args.path)
                for oo in opn.parent.parent.opens_with_children
                if oo.opaque is not None and oo.opaque.compilation_info and self.filter_open(oo)
            })
            if self.args.revdeps:
                data = self.get_revdeps(data)
                return data, DataTypes.compiled_data, None

            return data, DataTypes.compiled_data, None


class CompilationInfo(Module):
    required_args = ["path:1+"]

    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="This module display extended compilation information.", epilog="TODO EPILOG")
        g = module_parser.add_argument_group("Compilation info arguments")
        add_args( ["path"], g)
        return module_parser

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
        data = list({
            ent
            for ent in self.nfsdb.get_execs_filtered(has_comp_info=True)
            if len(ent.compilation_info.files) > 0 and ent.compilation_info.files[0].path in self.args.path and self.filter_exec(ent)
        })

        return data, DataTypes.compilation_info_data, lambda x: x.compilation_info.files[0].path
