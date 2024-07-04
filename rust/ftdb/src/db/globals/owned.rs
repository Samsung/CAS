use super::global_entry_impl;
use crate::{
    db::{FtdbHandle, Handle, InnerRef, Literals, Location, Owned, ToBorrowed},
    macros::impl_inner_handle,
};
use ftdb_sys::ftdb::ftdb_global_entry;
use std::fmt::Display;

#[derive(Debug)]
pub struct GlobalEntry(Owned<ftdb_global_entry>);

global_entry_impl!(GlobalEntry);

impl Display for GlobalEntry {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<Global id:{}|name:{}>", self.id(), self.name())
    }
}

impl From<Owned<ftdb_global_entry>> for GlobalEntry {
    fn from(value: Owned<ftdb_global_entry>) -> Self {
        Self(value)
    }
}

impl_inner_handle!(GlobalEntry);

impl<'s> InnerRef<'s, 's, ftdb_global_entry> for GlobalEntry {
    fn inner_ref(&'s self) -> &'s ftdb_global_entry {
        self.0.inner_ref()
    }
}

impl<'a> ToBorrowed<'a> for GlobalEntry {
    type Type = super::GlobalEntry<'a>;

    fn to_borrowed(&'a self) -> Self::Type {
        self.inner_ref().into()
    }
}
