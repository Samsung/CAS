import libft_db
import libftdb
from argparse import ArgumentParser
from typing import Any, Callable, Tuple
from client.mod_base import Module, PipedModule, FilterableModule
from client.output_renderers.output import DataTypes

class GlobalsModule(Module, FilterableModule, PipedModule):
    """ Globals - returns list of global variables from Function Type Database """
    @staticmethod
    def get_argparser() -> ArgumentParser:
        return Module.add_args(["details", "ftdb-simple-filter", "definition"], GlobalsModule)

    def set_piped_arg(self, data, data_type: type) -> None:
        if data_type in (libft_db.ftdbSourceEntry, libft_db.ftdbModuleEntry):
            self.args.fids = [d.fid for d in data]

    def get_data(self) -> Tuple[Any, DataTypes, "Callable|None", "type|None"]:
        if self.has_ftdb_simple_filter:
            globs = [glob for glob in self.ft_db.get_globs(getattr(self.args, 'fids', None)) if self.filter_ftdb(glob)]
        else:
            globs = [glob for glob in self.ft_db.get_globs(getattr(self.args, 'fids', None))]
        return globs, DataTypes.global_data, lambda x: x.name, libftdb.ftdbGlobalEntry 


class TypesModule(Module, FilterableModule):
    """ Types - returns list of types from Function Type Database """
    @staticmethod
    def get_argparser() -> ArgumentParser:
        return Module.add_args(["details", "ftdb-simple-filter"], TypesModule)

    def get_data(self) -> Tuple[Any, DataTypes, "Callable|None", "type|None"]:
        if self.has_ftdb_simple_filter:
            types = [t for t in self.ft_db.get_types() if self.filter_ftdb(t)]
        else:
            types = [t for t in self.ft_db.get_types()]
        return types, DataTypes.type_data, lambda x: x.name, libftdb.ftdbTypeEntry 

