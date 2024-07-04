use super::unresolved_funcs_entry_impl;
use crate::db::{InnerRef, Owned, ToBorrowed};
use ftdb_sys::ftdb::ftdb_unresolvedfunc_entry;

pub struct UnresolvedFuncEntry(Owned<ftdb_unresolvedfunc_entry>);

unresolved_funcs_entry_impl!(UnresolvedFuncEntry);

impl std::fmt::Display for UnresolvedFuncEntry {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "({}, {})", self.id(), self.name())
    }
}

impl From<Owned<ftdb_unresolvedfunc_entry>> for UnresolvedFuncEntry {
    fn from(value: Owned<ftdb_unresolvedfunc_entry>) -> Self {
        Self(value)
    }
}

impl<'s> InnerRef<'s, 's, ftdb_unresolvedfunc_entry> for UnresolvedFuncEntry {
    fn inner_ref(&'s self) -> &'s ftdb_unresolvedfunc_entry {
        self.0.inner_ref()
    }
}

impl<'a> ToBorrowed<'a> for UnresolvedFuncEntry {
    type Type = super::UnresolvedFuncEntry<'a>;

    fn to_borrowed(&'a self) -> Self::Type {
        self.inner_ref().into()
    }
}
