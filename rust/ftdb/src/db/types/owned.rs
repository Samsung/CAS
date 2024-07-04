use super::type_entry_impl;
use crate::{
    db::{FtdbHandle, Handle, InnerRef, Owned, ToBorrowed},
    macros::impl_inner_handle,
};
use ftdb_sys::ftdb::ftdb_type_entry;

pub struct TypeEntry(Owned<ftdb_type_entry>);

type_entry_impl!(TypeEntry);

impl std::fmt::Display for TypeEntry {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "id:{}|class:{}", self.id(), self.classname())
    }
}

impl From<Owned<ftdb_type_entry>> for TypeEntry {
    fn from(value: Owned<ftdb_type_entry>) -> Self {
        Self(value)
    }
}

impl_inner_handle!(TypeEntry);

impl<'s> InnerRef<'s, 's, ftdb_type_entry> for TypeEntry {
    fn inner_ref(&'s self) -> &'s ftdb_type_entry {
        self.0.inner_ref()
    }
}

impl<'a> ToBorrowed<'a> for TypeEntry {
    type Type = super::TypeEntry<'a>;

    fn to_borrowed(&'a self) -> Self::Type {
        self.inner_ref().into()
    }
}
