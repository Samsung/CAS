use super::global_entry_impl;
use crate::db::{InnerRef, Literals, Location};
use ftdb_sys::ftdb::ftdb_global_entry;
use std::fmt::Display;

#[derive(Debug)]
pub struct GlobalEntry<'a>(pub(crate) &'a ftdb_global_entry);

global_entry_impl!(GlobalEntry<'a>);

impl<'a> Display for GlobalEntry<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "<ftdb:GlobalEntry id:{}|name:{}>",
            self.id(),
            self.name()
        )
    }
}

impl<'a> From<&'a ftdb_global_entry> for GlobalEntry<'a> {
    fn from(inner: &'a ftdb_global_entry) -> Self {
        GlobalEntry(inner)
    }
}

impl<'s, 'r> InnerRef<'s, 'r, ftdb_global_entry> for GlobalEntry<'r> {
    fn inner_ref(&'s self) -> &'r ftdb_global_entry {
        self.0
    }
}
