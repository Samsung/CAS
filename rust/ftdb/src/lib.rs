//! Bindings to FTDB library
//!
//! This crate provides safe interface to the FTDB library which is part of
//! [CAS](https://github.com/Samsung/CAS) (Code Aware Services).
//!
//! FTDB library allows to read data from binary format of Function/Type
//! databases created by CAS tools. These are C structures flattened with
//! [uflat](https://github.com/Samsung/kflat/) project.
//!
//! # Building
//!
//! Both FTDB library and headers are required to build this crate.
//!
//! A set of environment variables can be used to point ftdb-sys crate
//! towards FTDB (CAS) installation.
//!
//! * `FTDB_INCLUDE_DIR` - points to include paths to find headers from.
//!   This should be set to root directory of CAS project.
//! * `FTDB_LIB_DIR` - points to location of a directory containing
//!   `libftdb_c.so` file. This path is only used for building purposes.
//!
//! # Adding dependency
//!
//! To add `ftdb`, modify your `Cargo.toml` file with following snippet:
//!
//! ```toml
//! [dependencies.ftdb]
//! git = "https://github.com/Samsung/CAS.git"
//! branch = "master"
//! ```
//!
//! # Running
//!
//! When running your program remember to put `libftdb_c.so` in a place
//! available to `ld` or use `LD_LIBRARY_PATH`.
//!
//! # Example
//!
//! To start working with `ftdb` library open up flattened database file:
//!
//! ```rust
//! use ftdb::load;
//! use crate::ftdb::FtdbCollection;
//!
//! # fn run() -> ftdb::FtdbResult<()> {
//!    let fdb = ftdb::load("/path/to/database.img")?;
//!    println!("\nMetadata:");
//!    println!("  Directory:   {}", fdb.directory());
//!    println!("  Module:      {}", fdb.module());
//!    println!("  Release:     {}", fdb.release());
//!    println!("  Version:     {}", fdb.version());
//!
//!    println!("\nStats:");
//!    println!("  Files:       {:>10}", fdb.sources().len());
//!    println!("  Functions:   {:>10}", fdb.funcs().iter().count());
//!    println!("  Globals:     {:>10}", fdb.globals().len());
//!    println!("  Types:       {:>10}", fdb.types().len());
//! #   Ok(())
//! # }
//! ````
//!
//! Consider visiting `examples/` directory for more use cases of this crate.
//!
//! # About FTDB
//!
//! To get more knowledge of FTDB library background consider visting
//! [CAS repository](https://github.com/Samsung/CAS). Its README provides
//! description on how to setup environment and what the project is all about..
//! There is also link to a conference talk about CAS which might provide you
//! better insights.
//!

mod db;
pub(crate) mod macros;

pub use self::db::*;

/// TODO: Replace occurences of crate::utils with crate::db::utils
pub(crate) mod utils {
    pub use super::db::utils::*;
}

/// Loads FTDB cache file to memory and returns a handle to it
///
/// # Arguments
///
/// * `path` - Path to FTDB cache file
///
/// # Errors
///
/// In case of file missing a valid magic number, the `FtdbError::InvalidMagicNumber`
/// error is returned to indicate that this might not be a FTDB cache file.
///
/// If FTDB cache file is in older version, the `FtdbError::InvalidVersion`
/// error is returned to state that the FTDB cache file might need to be regenerated
/// with the recent software.
///
/// In case of other errors, the `FtdbError::LoadError` is returned with a string
/// describing an issue that happened.
///
///
/// # Examples
///
/// ```
/// # fn test() -> ftdb::FtdbResult<()> {
///     let db = ftdb::load("/tmp/my_database.img")?;
///     println!("{}", db.module());
/// # Ok(())
/// # }
/// ```
///
pub fn load<T: AsRef<std::path::Path>>(path: T) -> FtdbResult<crate::Ftdb> {
    db::FtdbHandle::from_path(path).and_then(|handle| handle.into_ftdb())
}

pub type FtdbResult<T> = std::result::Result<T, FtdbError>;

#[derive(thiserror::Error, Debug, Clone)]
pub enum FtdbError {
    #[error("Invalid magic number: {0}")]
    InvalidMagicNumber(u64),

    #[error("Invalid FTDB version: {0}")]
    InvalidVersion(u64),

    #[error("Database not loaded: {0}")]
    LoadError(String),
}

#[cfg(feature = "sqlx")]
mod sqlx;
