use super::{Fops, FuncDecls, Functions, Globals, Modules, Sources, Types, UnresolvedFuncs};
use crate::error::FtdbError;
use crate::utils::{ptr_to_slice, ptr_to_str};
use std::ffi::{CStr, CString};
use std::fmt::Display;
use std::ops::Deref;
use std::path::Path;

const DEF_FTDB_DEBUG: i32 = 0;
const DEF_FTDB_QUIET: i32 = 1;

#[derive(Debug)]
pub struct Ftdb {
    db: *const ::ftdb_sys::ftdb::ftdb,
    handle: ::ftdb_sys::ftdb::CFtdb,
}

impl Deref for Ftdb {
    type Target = ::ftdb_sys::ftdb::ftdb;

    fn deref(&self) -> &Self::Target {
        unsafe { &*(self.db as *const ::ftdb_sys::ftdb::ftdb) }
    }
}

impl Ftdb {
    ///
    ///
    pub fn inner(&self) -> &::ftdb_sys::ftdb::ftdb {
        self.deref()
    }

    /// ftdb object directory
    ///
    pub fn directory(&self) -> &str {
        let x = unsafe { CStr::from_ptr(self.inner().directory) };
        x.to_str().expect("File format is valid")
    }

    /// ftdb fops object
    ///
    pub fn fops(&self) -> Fops<'_> {
        self.inner().into()
    }

    /// ftdb funcdecls object
    ///
    pub fn funcdecls(&self) -> FuncDecls<'_> {
        self.inner().into()
    }

    /// ftdb funcs object
    ///
    pub fn funcs(&self) -> Functions<'_> {
        self.inner().into()
    }

    /// ftdb globals object
    ///
    pub fn globals(&self) -> Globals<'_> {
        self.inner().into()
    }

    /// ftdb object module
    ///
    pub fn module(&self) -> &str {
        let x = unsafe { CStr::from_ptr(self.inner().module) };
        x.to_str().expect("File format is valid")
    }

    /// ftdb modules object
    ///
    pub fn modules(&self) -> Modules<'_> {
        ptr_to_slice(
            self.inner().moduleindex_table,
            self.inner().moduleindex_table_count,
        )
        .iter()
        .map(|x| ptr_to_str(*x))
        .collect::<Vec<_>>()
        .into()
    }

    /// ftdb sources object
    ///
    pub fn sources(&self) -> Sources<'_> {
        ptr_to_slice(
            self.inner().sourceindex_table,
            self.inner().sourceindex_table_count,
        )
        .iter()
        .map(|x| ptr_to_str(*x))
        .collect::<Vec<_>>()
        .into()
    }

    /// ftdb modules object
    ///
    pub fn release(&self) -> &str {
        let x = unsafe { CStr::from_ptr(self.inner().release) };
        x.to_str().expect("File format is valid")
    }

    /// ftdb types object
    ///
    pub fn types(&self) -> Types<'_> {
        self.inner().into()
    }

    /// ftdb unresolvedfuncs object
    ///
    pub fn unresolved_funcs(&self) -> UnresolvedFuncs<'_> {
        self.inner().into()
    }

    /// ftdb object version
    ///
    pub fn version(&self) -> &str {
        let x = unsafe { CStr::from_ptr(self.inner().version) };
        x.to_str().expect("File format is valid")
    }

    pub fn new<T: AsRef<Path> + Clone>(path: T) -> crate::error::Result<Self> {
        let path = path.as_ref();
        let path = path.to_str().expect("valid string");
        let path = CString::new(path.as_bytes()).expect("No NULL byte in a path");
        let path = path.as_ptr();
        let debug = match std::env::var("FTDB_DEBUG") {
            Ok(v) => v.parse().unwrap_or(DEF_FTDB_DEBUG),
            Err(_) => DEF_FTDB_DEBUG,
        };
        let quiet = match std::env::var("FTDB_QUIET") {
            Ok(v) => v.parse().unwrap_or(DEF_FTDB_QUIET),
            Err(_) => DEF_FTDB_QUIET,
        };
        let handle = unsafe { ::ftdb_sys::ftdb::libftdb_c_ftdb_load(path, quiet, debug) };
        if handle.is_null() {
            return Err(FtdbError::LoadError(
                "libftdb_c_ftdb_load failed".to_string(),
            ));
        }
        let db = unsafe { ::ftdb_sys::ftdb::libftdb_c_ftdb_object(handle) };
        if db.is_null() {
            return Err(FtdbError::LoadError(
                "libftdb_c_ftdb_object failed".to_string(),
            ));
        }
        Ok(Self { db, handle })
    }
}

impl Display for Ftdb {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "<ftdb: funcs:{}|types:{}|globals:{}|sources:{}>",
            self.funcs_count, self.types_count, self.globals_count, self.sourceindex_table_count
        )
    }
}

impl Drop for Ftdb {
    fn drop(&mut self) {
        unsafe { ::ftdb_sys::ftdb::libftdb_c_ftdb_unload(self.handle) }
    }
}
