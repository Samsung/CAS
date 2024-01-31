#![allow(unused_unsafe)]
#![allow(unused_variables)]
#![allow(dead_code)]

use db::Ftdb;
use std::path::Path;

pub mod db;
pub mod error;
pub(crate) mod utils;

/// Load cache file
///
pub fn load_ftdb_cache<T: AsRef<Path>>(path: T) -> error::Result<db::Ftdb> {
    Ftdb::new(path.as_ref())
}
