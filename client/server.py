import os
import time
from os.path import basename, dirname, abspath, join, exists
from glob import glob
from argparse import Namespace
from typing import Dict, List, Optional
from client.cmdline import process_commandline
from client.exceptions import EndpointException, DatabaseException
from client.misc import get_config_path
import libcas
import libft_db


class DBProvider:
    db_map: Dict[str, Dict] = {}

    def __init__(self) -> None:
        self.args = Namespace()
        self.multi_instance = False
    
    def get_dbs(self) -> Dict[str, Dict]:
        self.refresh_dbs()
        return self.db_map

    def lazy_init(self, args: Namespace):
        self.args = args
        self.multi_instance = args.dbs is not None
        self.refresh_dbs()

    def refresh_dbs(self):
        tmp_db_map: Dict[str, Dict] = {}
        if self.multi_instance:
            files = glob(f'{self.args.dbs}/*/.nfsdb.img')
            sub = glob(f'{self.args.dbs}/*/*/.nfsdb.img')
        else:
            if self.args.casdb:
                files = [self.args.casdb]
                sub = []
            elif "DB_DIR" in os.environ:
                files = glob(os.path.abspath(f'{os.environ["DB_DIR"]}/.nfsdb.img'))
                sub = glob(os.path.abspath(f'{os.environ["DB_DIR"]}/*/.nfsdb.img'))
            else:
                files = glob(os.path.abspath(f'{os.getcwd()}/.nfsdb.img'))
                sub = glob(os.path.abspath(f'{os.getcwd()}/*/.nfsdb.img'))

        if self.multi_instance and self.args.ftd:
            ftfiles = glob(f'{self.args.ftd}/*/*.img')
        else:
            if self.args.ftdb:
                ftfiles = [self.args.ftdb]
            elif "FTDB_DIR" in os.environ:
                ftfiles = glob(f'{os.environ["FTDB_DIR"]}/*.img')
            else:
                ftfiles = glob(f'{os.getcwd()}/*.img')

            ftfiles = [os.path.abspath(f) for f in ftfiles]

        for nfsdb_path in files:
            db_dir = dirname(abspath(nfsdb_path))
            db_key = basename(db_dir).replace("_DONE", "") if self.multi_instance else ""
            ft_dir = self.args.ftd if self.args.ftd else os.environ["FTDB_DIR"] if "FTDB_DIR" in os.environ else os.getcwd()
            config_file = get_config_path(join(db_dir, ".bas_config"))
            deps_file = join(db_dir, ".nfsdb.deps.img")
            ftdb_files = [x for x in ftfiles if os.path.join(ft_dir, db_key) in x]
            tmp_db_map[db_key] = {
                "db_dir": db_dir,
                "nfsdb_path": nfsdb_path,
                "deps_path": deps_file,
                "ftdb_files": ftdb_files,
                "config_file": config_file if exists(config_file) else None,
                "nfsdb": None,
                "ftdb": None,
                "config": None,
                "last_access": None,
                "db_verison": libcas.CASDatabase.get_db_version(nfsdb_path),
                "image_version": libcas.CASDatabase.get_img_version(nfsdb_path)
            }
            tmp_db_map[db_key]["ftdb_files"] = [x for x in ftfiles if os.path.join(ft_dir, db_key) in x]
        for nfsdb_path in sub:
            db_dir = dirname(abspath(nfsdb_path))
            sub_dir = basename(dirname(nfsdb_path))
            db_key = basename(dirname(db_dir)).replace("_DONE", "") if self.multi_instance else ""
            db_key = db_key + "-" + sub_dir
            ft_dir = self.args.ftd if self.args.ftd else os.environ["FTDB_DIR"] if "FTDB_DIR" in os.environ else os.getcwd()
            config_file = get_config_path(join(db_dir, ".bas_config"))
            deps_file = join(db_dir, ".nfsdb.deps.img")
            ftdb_files = [x for x in ftfiles if os.path.join(ft_dir, db_key) in x]
            tmp_db_map[db_key] = {
                "db_dir": db_dir,
                "nfsdb_path": nfsdb_path,
                "deps_path": deps_file,
                "ftdb_files": ftdb_files,
                "config_file": config_file if exists(config_file) else None,
                "nfsdb": None,
                "ftdb": None,
                "config": None,
                "last_access": None,
                "db_verison": libcas.CASDatabase.get_db_version(nfsdb_path),
                "image_version": libcas.CASDatabase.get_img_version(nfsdb_path)
            }

        for removed in set(self.db_map) - set(tmp_db_map):
            del self.db_map[removed]

        for added in set(tmp_db_map) - set(self.db_map):
            self.db_map[added] = tmp_db_map[added]

    def ensure_db(self, db_name):
        self.refresh_dbs()
        if db_name in self.db_map:
            if not self.db_map[db_name]["nfsdb"]:
                self.db_map[db_name]["nfsdb"] = libcas.CASDatabase()
                if self.db_map[db_name]["config_file"]:
                    self.db_map[db_name]["config"] = libcas.CASConfig(self.db_map[db_name]["config_file"])
                    self.db_map[db_name]["nfsdb"].set_config(self.db_map[db_name]["config"])
                    try:
                        self.db_map[db_name]["nfsdb"].load_db(self.db_map[db_name]["nfsdb_path"], debug=self.args.debug, quiet=not self.args.verbose)
                        self.db_map[db_name]["nfsdb"].load_deps_db(self.db_map[db_name]["deps_path"], debug=self.args.debug, quiet=not self.args.verbose)
                        self.db_map[db_name]["db_version"] = libcas.CASDatabase.get_db_version(self.db_map[db_name]["nfsdb_path"])
                    except Exception as e:
                        self.db_map[db_name]["nfsdb"] = None
                        raise DatabaseException(str(e)) from e
                else:
                    self.db_map[db_name]["nfsdb"] = None
                    raise DatabaseException("Database config '.bas_config' not found!")
        else:
            raise EndpointException(f"Endpoint database '{db_name}' does not exists!")

    def process_command(self, db_name: str, cmd: List[str]):
        self.ensure_db(db_name)
        self.db_map[db_name]["last_access"] = time.time()
        return process_commandline(self.db_map[db_name]["nfsdb"], cmd, None, is_server=True)

    def get_nfsdb(self, db_name: str) -> libcas.CASDatabase:
        self.ensure_db(db_name)
        return self.db_map[db_name]["nfsdb"]

    def get_ft_db(self, db_name: str) -> libft_db.FTDatabase:
        self.ensure_db(db_name)
        return self.db_map[db_name]["ftdb"]

    def switch_ftdb(self, db_name: str, ftdb_name: str):
        self.ensure_db(db_name)
        if not self.db_map[db_name]["ftdb"]:
            self.db_map[db_name]["ftdb"] = libft_db.FTDatabase()
        self.db_map[db_name]["ftdb"].unload_db()
        if ftdb_name in self.db_map[db_name]["ftdb_files"]:
            self.db_map[db_name]["ftdb"].load_db(ftdb_name)
            return True
        return False

    def json(self, db: Optional[str] = None):
        self.refresh_dbs()
        ret = {}
        if db and db in self.db_map:
            return {
                db: {
                    "db_dir": self.db_map[db]['db_dir'],
                    "nfsdb_path": self.db_map[db]["nfsdb_path"],
                    "deps_path": self.db_map[db]["deps_path"],
                    "ftdb_paths": self.db_map[db]["ftdb_files"],
                    "config_path": self.db_map[db]["config_file"],
                    "loaded_nfsdb": self.db_map[db]["nfsdb"].db_path if self.db_map[db]["nfsdb"] and self.db_map[db]["nfsdb"].db_loaded else None,
                    "loaded_ftdb": self.db_map[db]["ftdb"].db_path if self.db_map[db]["ftdb"] and self.db_map[db]["nfsdb"].db_loaded else None,
                    "loaded_config": self.db_map[db]["config"].config_file if self.db_map[db]["config"] else None,
                    "last_access": self.db_map[db]["last_access"],
                    "image_version": self.db_map[db]["image_version"],
                    "db_verison": self.db_map[db]["db_verison"]
                }
            }
        for db_name in self.db_map:
            ret[db_name] = {
                "db_dir": self.db_map[db_name]['db_dir'],
                "nfsdb_path": self.db_map[db_name]["nfsdb_path"],
                "deps_path": self.db_map[db_name]["deps_path"],
                "ftdb_paths": self.db_map[db_name]["ftdb_files"],
                "config_path": self.db_map[db_name]["config_file"],
                "loaded_nfsdb": self.db_map[db_name]["nfsdb"].db_path if self.db_map[db_name]["nfsdb"] and self.db_map[db_name]["nfsdb"].db_loaded else None,
                "loaded_ftdb": self.db_map[db_name]["ftdb"].db_path if self.db_map[db_name]["ftdb"] and self.db_map[db_name]["nfsdb"].db_loaded else None,
                "loaded_config": self.db_map[db_name]["config"].config_file if self.db_map[db_name]["config"] else None,
                "last_access": self.db_map[db_name]["last_access"],
                "image_version": self.db_map[db_name]["image_version"],
                "db_verison": self.db_map[db_name]["db_verison"]
            }
        if db and db not in self.db_map:
            raise EndpointException(f"Endpoint database '{db}' does not exists!")
        return ret
