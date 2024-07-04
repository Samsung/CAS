use crate::{Ftdb, FtdbError, FtdbResult};
use ftdb_sys::ftdb::{libftdb_c_ftdb_load, libftdb_c_ftdb_object};
use std::ffi::CString;
use std::path::Path;
use std::ptr::NonNull;
use std::sync::Arc;

const DEF_FTDB_DEBUG: i32 = 0;
const DEF_FTDB_QUIET: i32 = 1;

/// Stores a handle to FTDB memory
///
#[derive(Debug, Clone)]
pub struct FtdbHandle(std::ptr::NonNull<::libc::c_void>);

unsafe impl Send for FtdbHandle {}
unsafe impl Sync for FtdbHandle {}

impl Drop for FtdbHandle {
    fn drop(&mut self) {
        unsafe { ::ftdb_sys::ftdb::libftdb_c_ftdb_unload(self.0.as_ptr()) }
    }
}

impl FtdbHandle {
    /// Loads FTDB database to memory from a given path
    ///
    pub fn from_path<T: AsRef<Path>>(path: T) -> FtdbResult<Self> {
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

        // Memory is aligned
        let handle = unsafe { libftdb_c_ftdb_load(path, quiet, debug) };
        if handle.is_null() {
            return Err(FtdbError::LoadError(
                "libftdb_c_ftdb_load failed".to_string(),
            ));
        }
        Ok(Self(NonNull::new(handle).expect("guaranteed non-null")))
    }

    pub(crate) fn into_ftdb(self) -> FtdbResult<Ftdb> {
        let db = unsafe { libftdb_c_ftdb_object(self.0.as_ptr()) };
        if db.is_null() {
            return Err(crate::FtdbError::LoadError(
                "libftdb_c_ftdb_object failed".to_string(),
            ));
        }
        let db = NonNull::new(db).expect("guaranteed non-null");
        Ok(Ftdb::new(db, Arc::new(self)))
    }
}
