use std::sync::Arc;

use super::unresolved_funcs_entry_impl;
use crate::db::{FtdbHandle, InnerRef, Owned};
use ftdb_sys::ftdb::ftdb_unresolvedfunc_entry;

#[derive(Debug, Clone)]
pub struct UnresolvedFuncEntry<'a>(&'a ftdb_unresolvedfunc_entry);

unresolved_funcs_entry_impl!(UnresolvedFuncEntry<'a>);

impl<'a> std::fmt::Display for UnresolvedFuncEntry<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "({}, {})", self.id(), self.name())
    }
}

impl<'a> From<&'a ftdb_unresolvedfunc_entry> for UnresolvedFuncEntry<'a> {
    fn from(entry: &'a ftdb_unresolvedfunc_entry) -> Self {
        Self(entry)
    }
}

impl<'s, 'r> InnerRef<'s, 'r, ftdb_unresolvedfunc_entry> for UnresolvedFuncEntry<'r> {
    fn as_inner_ref(&'s self) -> &'r ftdb_unresolvedfunc_entry {
        self.0
    }
}

impl<'a> UnresolvedFuncEntry<'a> {
    pub fn to_owned(&self, handle: Arc<FtdbHandle>) -> super::OwnedUnresolvedFuncEntry {
        let inner_ptr = self.0 as *const ftdb_unresolvedfunc_entry;
        super::OwnedUnresolvedFuncEntry::from(Owned {
            // Safety: Pointer is guaranteed to be non null
            // Converting reference to a pointer and ignoring its lifetime is
            // also safe as memory is valid as long as handle exists
            db: unsafe { std::ptr::NonNull::new_unchecked(inner_ptr.cast_mut()) },
            handle,
        })
    }

    pub fn into_owned(self, handle: Arc<FtdbHandle>) -> super::OwnedUnresolvedFuncEntry {
        self.to_owned(handle)
    }
}
