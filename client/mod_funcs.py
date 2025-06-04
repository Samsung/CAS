import libftdb
import libft_db
from argparse import ArgumentParser
from typing import Any, Callable, Tuple
from client.mod_base import Module, PipedModule, FilterableModule
from client.output_renderers.output import DataTypes

class FunctionsModule(Module, FilterableModule, PipedModule):
    """ Functions - returns list of functions from Function Type Database """
    @staticmethod
    def get_argparser() -> ArgumentParser:
        return Module.add_args(["ftdb-simple-filter", "details", "body", "ubody"], FunctionsModule)
    
    def set_piped_arg(self, data, data_type: type) -> None:
        if data_type in (libft_db.ftdbSourceEntry, libft_db.ftdbModuleEntry):
            self.args.fids = [d.fid for d in data]
    
    def get_data(self) -> Tuple[Any, DataTypes, "Callable|None", "type|None"]:
        if self.has_ftdb_simple_filter:
            funcs = [func for func in self.ft_db.get_funcs(getattr(self.args, 'fids', None)) if self.filter_ftdb(func)]
        else:
            funcs = [func for func in self.ft_db.get_funcs(getattr(self.args, 'fids', None))]
        return funcs, DataTypes.function_data, lambda x: x.name, None


class FuncDeclModule(Module, FilterableModule, PipedModule):
    """ Functions - returns list of functions' declarations from Function Type Database """
    @staticmethod
    def get_argparser() -> ArgumentParser:
        return Module.add_args(["ftdb-simple-filter", "details", "declbody"], FuncDeclModule)
    
    def set_piped_arg(self, data, data_type: type) -> None:
        if data_type in (libft_db.ftdbSourceEntry, libft_db.ftdbModuleEntry):
            self.args.fids = [d.fid for d in data]

    def get_data(self) -> Tuple[Any, DataTypes, "Callable|None", "type|None"]:
        if self.has_ftdb_simple_filter:
            funcdecls = [fd for fd in self.ft_db.get_funcdecls(getattr(self.args, 'fids', None)) if self.filter_ftdb(fd)]
        else:
            funcdecls = [fd for fd in self.ft_db.get_funcdecls(getattr(self.args, 'fids', None))]
        return funcdecls, DataTypes.funcdecl_data, lambda x: x.name, None
