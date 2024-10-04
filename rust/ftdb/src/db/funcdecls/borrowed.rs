use super::func_decl_entry_impl;
use crate::db::InnerRef;
use ftdb_sys::ftdb::ftdb_funcdecl_entry;

/// Represents an undefined function declaration
///
#[derive(Debug, Clone)]
pub struct FuncDeclEntry<'a>(pub(crate) &'a ftdb_funcdecl_entry);

func_decl_entry_impl!(FuncDeclEntry<'a>);

impl<'s, 'r> InnerRef<'s, 'r, ftdb_funcdecl_entry> for FuncDeclEntry<'r> {
    fn as_inner_ref(&'s self) -> &'r ftdb_funcdecl_entry {
        self.0
    }
}

impl<'a> From<&'a ftdb_funcdecl_entry> for FuncDeclEntry<'a> {
    fn from(value: &'a ftdb_funcdecl_entry) -> Self {
        Self(value)
    }
}
