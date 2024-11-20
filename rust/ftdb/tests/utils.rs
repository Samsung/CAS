#![allow(dead_code)]

use ftdb::{Ftdb, FuncDeclEntry, FunctionEntry, GlobalEntry, TypeEntry};
use std::path::{Path, PathBuf};

pub struct FtdbTestCase {
    pub db: Ftdb,
    dir: PathBuf,
}

impl FtdbTestCase {
    pub fn root_dir(&self) -> &str {
        self.dir.as_path().to_str().unwrap()
    }

    pub fn test_file<T: AsRef<str>>(&self, file_name: T) -> Box<Path> {
        self.dir.join(file_name.as_ref()).into_boxed_path()
    }

    pub fn expect_func(&self, name: &str) -> FunctionEntry {
        self.db
            .funcs_by_name(name)
            .next()
            .unwrap_or_else(|| panic!("Function {name} expected in funcs section"))
    }

    pub fn expect_funcdecl(&self, name: &str) -> FuncDeclEntry {
        self.db
            .funcdecls_iter()
            .find(|f| f.name() == name)
            .unwrap_or_else(|| panic!("Function {name} expected in funcdecls section"))
    }

    pub fn expect_global(&self, name: &str) -> GlobalEntry {
        self.db
            .globals_by_name(name)
            .next()
            .unwrap_or_else(|| panic!("Global {name} expected in globals section"))
    }

    pub fn expect_type<'a>(&'a self, name: &str) -> TypeEntry<'a> {
        self.db
            .types_by_name(name)
            .next()
            .unwrap_or_else(|| panic!("Type {name} expected in types section"))
    }
}

/// Load FTDB database file `db.img` from project_dir/tests/`name` directory
///
/// # Panics
///
/// In case of any errors (file system or ftdb) this function panics.
///
pub fn load_test_case<T: AsRef<str>>(name: T) -> FtdbTestCase {
    let root_dir = root_test_dir(name);
    let db_path = root_dir.join("db.img");
    if !db_path.exists() {
        panic!(
            "Database {0} not built yet! Consider documentation for building steps.",
            db_path.to_string_lossy()
        );
    }
    match ::ftdb::load(db_path) {
        Ok(db) => FtdbTestCase { db, dir: root_dir },
        Err(err) => panic!("Database could not be loaded: {err}"),
    }
}

fn root_test_dir<T: AsRef<str>>(name: T) -> PathBuf {
    let mut path: PathBuf = env!("CARGO_MANIFEST_DIR").into();
    path.pop();
    path.push("tests");
    path.push(name.as_ref());
    if !path.is_dir() {
        panic!("Test directory {0} does not exist", path.to_string_lossy());
    }
    path
}
