use super::func_decl_entry_impl;
use crate::{
    db::{FtdbHandle, Handle, InnerRef, Owned, ToBorrowed},
    macros::impl_inner_handle,
};
use ftdb_sys::ftdb::ftdb_funcdecl_entry;

pub struct FuncDeclEntry(Owned<ftdb_funcdecl_entry>);

func_decl_entry_impl!(FuncDeclEntry);

impl From<Owned<ftdb_funcdecl_entry>> for FuncDeclEntry {
    fn from(value: Owned<ftdb_funcdecl_entry>) -> Self {
        Self(value)
    }
}

impl_inner_handle!(FuncDeclEntry);

impl<'s> InnerRef<'s, 's, ftdb_funcdecl_entry> for FuncDeclEntry {
    fn inner_ref(&'s self) -> &'s ftdb_funcdecl_entry {
        self.0.inner_ref()
    }
}

impl<'a> ToBorrowed<'a> for FuncDeclEntry {
    type Type = super::FuncDeclEntry<'a>;

    fn to_borrowed(&'a self) -> Self::Type {
        self.inner_ref().into()
    }
}
