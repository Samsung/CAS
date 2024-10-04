//! This module contains structures representing various unique identifiers.
//!
//! Some of them
//!
//! Note that ID is unique within a database file. For two databases taken
//! from a various points of development time, the same object in both databases
//! might have different ID.
//!
use crate::macros::impl_identifier;

// Id of a function
impl_identifier!(FunctionId);

// Id of a global variable
impl_identifier!(GlobalId);

// Id of a type
impl_identifier!(TypeId);

// Id of a file
impl_identifier!(FileId);

// Id of a module
impl_identifier!(ModuleId);

// Id of a local variable
impl_identifier!(LocalId);

// Compound Statement Id
impl_identifier!(CsId);

// Id of a function deref
impl_identifier!(DerefId);

impl FunctionId {
    /// Create non-existing FunctionId
    ///
    pub fn none() -> Self {
        FunctionId(0)
    }

    /// Check whether this instance is a valid one or not
    ///
    pub fn is_none(&self) -> bool {
        self.0 == 0
    }
}
