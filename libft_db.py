import libftdb
from libcas import CASConfig
from typing import List, Optional
from functools import lru_cache

class ftdbSourceEntry:
    
    def __init__(self, fid: int, path: str) -> None:
        self.fid = fid
        self.path = path


class ftdbModuleEntry:

    def __init__(self, mid: int, path: str) -> None:
        self.mid = mid
        self.path = path


class FTDatabase:
    """
    Ftdb database wrapper
    """

    db: libftdb.ftdb
    db_loaded: bool
    config: CASConfig

    def __init__(self) -> None:
        self.db = libftdb.ftdb()
        self.db_loaded = False
    
    def load_db(self, db_path: str, quiet:bool=True, debug:bool=False) -> bool:
        self.db_loaded = self.db.load(db_path, quiet=quiet, debug=debug)
        return self.db_loaded

    def unload_db(self) -> bool:
        del self.db
        self.db = libftdb.ftdb()
        return True

    def get_version(self) -> str:
        return self.db.version

    def get_module_name(self) -> str:
        return self.db.module

    def get_dir(self) -> str:
        return self.db.directory
    
    def get_release(self) -> str:
        return self.db.release

    def get_sources(self, fids=None) -> List[ftdbSourceEntry]:
        if fids is None:
            return [ftdbSourceEntry(s[0], s[1]) for s in list(self.db.sources)]
        return [ftdbSourceEntry(fid, self.db.sources[fid]) for fid in set(fids) if fid < len(self.db.sources)]
    
    def get_modules(self, mids=None) -> List[ftdbModuleEntry]:
        if mids is None:
            return [ftdbModuleEntry(md[0], md[1]) for md in list(self.db.modules)]
        return [ftdbModuleEntry(mid, self.db.modules[mid]) for mid in set(mids) if mid < len(self.db.modules)]

    def get_funcs(self, fids: Optional[List[int]]=None) -> List[libftdb.ftdbFuncEntry]:
        if fids is None:
            return self.db.funcs
        return [func for func in self.db.funcs if set(func.fids).intersection(fids)]

    def get_funcdecls(self, fids:Optional[List[int]]=None) -> List[libftdb.ftdbFuncdeclEntry]:
        if fids is None:
            return self.db.funcdecls
        return [fd for fd in self.db.funcdecls if fd.fid in fids]

    def get_globs(self, fids: Optional[List[int]]=None) -> List[libftdb.ftdbGlobalEntry]:
        if fids is None:
            return self.db.globals
        return [glob for glob in self.db.globals if glob.fid in fids]

    def get_types(self) -> libftdb.ftdbTypes:
        return self.db.types
