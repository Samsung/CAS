import argparse
from client.mod_base import Module
from client.misc import printdbg
from client.output_renderers.output import DataTypes
from client.argmap import add_args as add_args

class Binaries(Module):
    """
    Module used to get list of binaries used during tracing.
    """
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION", epilog="TODO EPILOG")
        g = module_parser.add_argument_group("Dependency generation arguments")
        add_args( [
            "filter", "select", "append",
            "details", "commands"]
            , g)
        return module_parser

    def subject(self, ent) -> str:
        if isinstance(ent, str):
            return ent
        return ent.bpath

    def get_data(self) -> tuple:
        if self.args.details or self.args.show_commands:
            data = list({
                ex
                for ex in self.nfsdb.get_execs_filtered(has_command=True)
                if self.filter_exec(ex)}
            )

            return data, DataTypes.commands_data, lambda x: x.bpath
        else:
            data = list({
                ex.bpath
                for ex in self.nfsdb.get_execs_filtered(has_command=True)
                if self.filter_exec(ex)}
            )

            return data, DataTypes.binary_data, None


class Commands(Module):
    """
    Module used to get list of commands invoked during tracing.
    """
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION ", epilog="TODO EPILOG")
        g = module_parser.add_argument_group("Dependency generation arguments")
        add_args( [
            "filter", "select", "append",
            "details", "commands",
            "pid", "binary",
            "extended",
            "raw-command",
            "parent",
            "filerefs"]
            , g)
        return module_parser

    def subject(self, ent) -> str:
        return " ".join(ent.argv) if not self.args.raw_command else ent.argv

    def set_piped_arg(self, data, data_type):
        if data_type == "str":
            printdbg("DEBUG: accepting {} as args.binary".format(data_type), self.args)
            self.args.binary = data
        if data_type == "nfsdbEntryOpenfile":
            printdbg("DEBUG: accepting {} as args.binary".format(data_type), self.args)
            self.args.binary = list({o.path for o in data})
        if data_type == "nfsdbEntry":
            printdbg("DEBUG: accepting {} as args.binary".format(data_type), self.args)
            self.args.binary = list({ex.eid.pid for ex in data})

    def get_data(self) -> tuple:
        data = list({
            ent
            for ent in self.nfsdb.get_execs_using_binary(self.args.binary)
            if self.filter_exec(ent)
        } if self.args.binary else {
            ent
            for ent in self.nfsdb.get_execs_filtered(has_command=True)
            if self.filter_exec(ent)
        })

        return data, DataTypes.commands_data, lambda x: x.argv
