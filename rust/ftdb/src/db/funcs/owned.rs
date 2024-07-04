use super::{asms::Asms, calls::Calls, entry::func_decl_entry_impl};
use crate::{
    db::{FtdbHandle, Handle, InnerRef, Owned, ToBorrowed},
    macros::impl_inner_handle,
};
use ftdb_sys::ftdb::ftdb_func_entry;
use std::fmt::Display;

#[derive(Debug, Clone)]
pub struct FunctionEntry(Owned<ftdb_func_entry>);

func_decl_entry_impl!(FunctionEntry);

impl Display for FunctionEntry {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<Function: {} | {}>", self.id(), self.name())
    }
}

impl From<Owned<ftdb_func_entry>> for FunctionEntry {
    fn from(value: Owned<ftdb_func_entry>) -> Self {
        Self(value)
    }
}

impl_inner_handle!(FunctionEntry);

impl<'s> InnerRef<'s, 's, ftdb_func_entry> for FunctionEntry {
    #[inline]
    fn inner_ref(&'s self) -> &'s ftdb_func_entry {
        self.0.inner_ref()
    }
}

impl<'a> ToBorrowed<'a> for FunctionEntry {
    type Type = super::FunctionEntry<'a>;

    #[inline]
    fn to_borrowed(&'a self) -> Self::Type {
        self.inner_ref().into()
    }
}
