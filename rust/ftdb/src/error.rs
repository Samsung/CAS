use thiserror::Error;

#[derive(Error, Debug)]
pub enum FtdbError {
    #[error("Invalid magic number: {0}")]
    InvalidMagicNumber(u64),

    #[error("Invalid FTDB version: {0}")]
    InvalidVersion(u64),

    #[error("Database not loaded: {0}")]
    LoadError(String),
}

pub type Result<T> = std::result::Result<T, FtdbError>;
