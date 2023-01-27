import argparse
from client.mod_base import Module
from client.output_renderers.output import DataTypes


class CompilerPattern(Module):
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION", epilog="TODO EPILOG")
        g = module_parser.add_argument_group("CompilerPattern arguments")
        return module_parser

    def get_data(self) -> tuple:
        return {
            "armcc_spec": self.config.armcc_spec,
            "clang_spec": self.config.clang_spec,
            "clangpp_spec": self.config.clangpp_spec,
            "gcc_spec": self.config.gcc_spec,
            "gpp_spec": self.config.gcc_spec,
        }, DataTypes.config_part_data, None


class LinkerPattern(Module):
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION", epilog="TODO EPILOG")
        g = module_parser.add_argument_group("LinkerPattern arguments")
        return module_parser    

    def get_data(self) -> tuple:
        return {
            "ar_spec": self.config.ar_spec,
            "ld_spec": self.config.ld_spec
        }, DataTypes.config_part_data, None


class VersionInfo(Module):
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION", epilog="TODO EPILOG")
        module_parser.add_argument_group("VersionInfo arguments")
        return module_parser

    def get_data(self) -> tuple:
        return self.nfsdb.get_version(), DataTypes.dbversion_data, None


class RootPid(Module):
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION", epilog="TODO EPILOG")
        module_parser.add_argument_group("RootPid arguments")
        return module_parser

    def get_data(self) -> tuple:
        return self.nfsdb.get_execs()[0].eid.pid, DataTypes.root_pid_data, None


class SourceRoot(Module):
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION", epilog="TODO EPILOG")
        module_parser.add_argument_group("SourceRoot arguments")
        return module_parser

    def get_data(self) -> tuple:
        return self.source_root, DataTypes.source_root_data, None


class ShowConfig(Module):
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION", epilog="TODO EPILOG")
        module_parser.add_argument_group("ShowConfig arguments")
        return module_parser

    def get_data(self) -> tuple:
        return self.config, DataTypes.config_data, None


class ShowStat(Module):
    @staticmethod
    def get_argparser():
        module_parser = argparse.ArgumentParser(description="TODO DESCRIPTION", epilog="TODO EPILOG")
        g = module_parser.add_argument_group("ShowStat arguments")
        return module_parser

    def get_data(self) -> tuple:
        return {
            "execs": self.nfsdb.execs_num(),
            "execs_commands": len(self.nfsdb.get_execs_filtered(has_command=True)),
            "execs_compilations": len(self.nfsdb.get_execs_filtered(has_comp_info=True)),
            "execs_linking": len(self.nfsdb.get_execs_filtered(has_linked_file=True)),
            "opens": self.nfsdb.opens_num(),
            "linked_modules": len(self.nfsdb.linked_modules())
        }, DataTypes.stat_data, None
