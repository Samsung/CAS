import libftdb
import libft_db
from argparse import ArgumentParser
from client.mod_base import Module, FilterableModule, PipedModule
from client.output_renderers.output import DataTypes
from client.misc import printdbg
from typing import List, Tuple, Any, Callable


class SourcesModule(Module, FilterableModule, PipedModule):
    """
        Sources from FTDatabase
    """
    @staticmethod
    def get_argparser():
        return Module.add_args(["details", "ftdb-simple-filter"], SourcesModule)
    
    def set_piped_arg(self, data, data_type: type) -> None:
        if type(data[0]).__name__ == 'ftdbFuncEntry':
            printdbg(f"DEBUG: accepting type {data_type}", self.args)
            fids = set()
            for d in data:
                fids = fids.union(set(d.fids))
            self.args.fids = fids
        elif type(data[0]).__name__ == 'ftdbFuncdeclEntry' or type(data[0]).__name__ == 'ftdbGlobalEntry':
            self.args.fids = list(set([fd.fid for fd in data]))

    def get_data(self) -> Tuple[Any, DataTypes, "Callable|None", "type|None"]:
        if self.has_ftdb_simple_filter:
            srcs: List[libft_db.ftdbSourceEntry] = [src
                                                for src in self.ft_db.get_sources(getattr(self.args, 'fids', None))
                                                if self.filter_ftdb(src)]
        else:
            srcs: List[libft_db.ftdbSourceEntry] = [src for src in self.ft_db.get_sources(getattr(self.args, 'fids', None))]
        return srcs, DataTypes.sources_data, None, libft_db.ftdbSourceEntry

class FTModules(Module, FilterableModule, PipedModule):
    """
        Modules from FTDatabase
    """

    @staticmethod
    def get_argparser() -> ArgumentParser:
        return Module.add_args(["details", "ftdb-simple-filter"], FTModules)
    
    def set_piped_arg(self, data, data_type: type) -> None:
        if type(data[0]).__name__ in ('ftdbFuncEntry', 'ftdbGlobalEntry'):
            printdbg(f"DEBUG: accepting type {data_type}", self.args)
            mids = set()
            for d in data:
                mids = mids.union(set(d.mids))
            self.args.mids = mids

    def get_data(self) -> Tuple[Any, DataTypes, "Callable|None", "type|None"]:
        if self.has_ftdb_simple_filter:
            mds: List[Tuple[int,str]] = [md for md in self.ft_db.get_modules(getattr(self.args, 'mids', None)) if self.filter_ftdb(md)]
        else:
            mds: List[Tuple[int,str]] = [md for md in self.ft_db.get_modules(getattr(self.args, 'mids', None))]
        return mds, DataTypes.modules_data, None, libft_db.ftdbModuleEntry
 