from client.mod_base import Module
from client.output_renderers.output import DataTypes


class CompilerPattern(Module):
    """Compiler Pattern - patterns used to categorize exec as compiler."""

    @staticmethod
    def get_argparser():
        return Module.add_args([], CompilerPattern)

    def get_data(self) -> tuple:
        return {
            "armcc_spec": self.config.armcc_spec,
            "clang_spec": self.config.clang_spec,
            "clangpp_spec": self.config.clangpp_spec,
            "gcc_spec": self.config.gcc_spec,
            "gpp_spec": self.config.gcc_spec,
        }, DataTypes.config_part_data, None, None


class LinkerPattern(Module):
    """Linker Pattern - patterns used to categorize exec as linker."""

    @staticmethod
    def get_argparser():
        return Module.add_args([], LinkerPattern)

    def get_data(self) -> tuple:
        return {
            "ar_spec": self.config.ar_spec,
            "ld_spec": self.config.ld_spec
        }, DataTypes.config_part_data, None, None


class VersionInfo(Module):
    """Version info - prints version meta information (set in 'cas cache' step)."""

    @staticmethod
    def get_argparser():
        return Module.add_args([], VersionInfo)

    def get_data(self) -> tuple:
        return self.nfsdb.get_version(), DataTypes.dbversion_data, None, None


class RootPid(Module):
    """Root pid - prints pid of first process that was started during tracing."""

    @staticmethod
    def get_argparser():
        return Module.add_args([], RootPid)

    def get_data(self) -> tuple:
        return self.nfsdb.db[0].eid.pid, DataTypes.root_pid_data, None, None


class SourceRoot(Module):
    """Source root - prints directory where first process started work (set in 'cas parse' step)."""

    @staticmethod
    def get_argparser():
        return Module.add_args([], SourceRoot)

    def get_data(self) -> tuple:
        return self.source_root, DataTypes.source_root_data, None, None


class ShowConfig(Module):
    """Show config - prints parsed config."""

    @staticmethod
    def get_argparser():
        return Module.add_args([], ShowConfig)

    def get_data(self) -> tuple:
        return self.config, DataTypes.config_data, None, None


class ShowStat(Module):
    """Show statistics - prints database statistics."""

    @staticmethod
    def get_argparser():
        return Module.add_args([], ShowStat)

    def get_data(self) -> tuple:
        return {
            "execs": self.nfsdb.execs_num(),
            "execs_commands": len(self.nfsdb.db.filtered_execs_iter(has_command=True)),
            "execs_compilations": len(self.nfsdb.db.filtered_execs_iter(has_comp_info=True)),
            "execs_linking": len(self.nfsdb.db.filtered_execs_iter(has_linked_file=True)),
            "binaries": len(self.nfsdb.db.binary_paths()),
            "opens": self.nfsdb.opens_num(),
            "linked": len(self.nfsdb.get_linked_files()),
            "compiled": len(self.nfsdb.get_compiled_files()),
            "compiled_paths": len(self.nfsdb.get_compiled_file_paths()),
            "linked_paths": len(self.nfsdb.get_linked_file_paths())
        }, DataTypes.stat_data, None, None
