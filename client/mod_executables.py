from client.mod_base import Module, PipedModule, FilterableModule
from client.misc import printdbg
from client.output_renderers.output import DataTypes


class Binaries(Module, FilterableModule):
    """Binaries - returns binaries list or execs that use given binaries."""

    @staticmethod
    def get_argparser():
        return Module.add_args([
            "filter", "command-filter", "select", "append",
            "details", "commands"], Binaries)

    def select_subject(self, ent) -> str:
        return ent.bpath

    def exclude_subject(self, ent) -> str:
        return ent.bpath

    def subject(self, ent) -> str:
        if self.args.details or self.args.show_commands:
            return ent.bpath
        else:
            return ent

    def prepare_args(self) -> dict:
        args: dict = {
            "exec_filter": self.command_filter.libetrace_filter if self.command_filter else None,
            "has_command": True
        }
        return args

    def get_data(self) -> tuple:

        args = self.prepare_args()

        if self.args.details or self.args.show_commands:
            data = list({ e 
                         for e in self.nfsdb.filtered_execs_iter(**args)
                         if self.should_display_exe(e)
            })
            return data, DataTypes.commands_data, lambda x: x.bpath
        else:
            data = list({
                ex.bpath
                for ex in self.nfsdb.filtered_execs_iter(**args)
            })

            return data, DataTypes.binary_data, lambda x: x


class Commands(Module, PipedModule, FilterableModule):
    """Commands - returns execs that are commands (bin is not empty)"""

    @staticmethod
    def get_argparser():
        return Module.add_args([
            "filter", "command-filter", "select", "append",
            "details", "commands",
            "pid", "binary",
            "extended",
            "parent",
            "filerefs", "cdb"], Commands)

    def select_subject(self, ent) -> str:
        return " ".join(ent.argv)

    def exclude_subject(self, ent) -> str:
        return " ".join(ent.argv)

    def subject(self, ent) -> str:
        return " ".join(ent.argv) if not self.args.raw_command else ent.argv

    def set_piped_arg(self, data, data_type):
        if data_type == "str":
            printdbg("DEBUG: accepting {} as args.binary".format(data_type), self.args)
            self.args.binary = data
        elif data_type == "nfsdbEntryOpenfile":
            printdbg("DEBUG: accepting {} as args.binary".format(data_type), self.args)
            self.args.binary = list({o.path for o in data})
        elif data_type == "nfsdbEntry":
            printdbg("DEBUG: accepting {} as args.binary".format(data_type), self.args)
            self.args.binary = list({ex.bpath for ex in data})

    def prepare_args(self) -> dict:
        args: dict = {}
        if self.command_filter:
            args["exec_filter"] = self.command_filter.libetrace_filter
        if self.args.generate and not self.args.all:
            args["has_comp_info"] = True
        else:
            args["has_command"] = True
        if self.args.binary:
            args["bins"] = self.args.binary
        if self.args.pid:
            args["pids"] = self.args.pid
        return args

    def get_data(self) -> tuple:
        args = self.prepare_args()
        data = self.nfsdb.filtered_execs(**args)

        if self.args.cdb:
            data = list(self.cdb_fix_multiple(data))
            return data, DataTypes.compilation_db_data, lambda x: x['filename']

        return data, DataTypes.commands_data, lambda x: x.argv
