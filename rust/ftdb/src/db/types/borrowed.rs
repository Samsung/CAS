use super::type_entry_impl;
use crate::db::{FtdbHandle, InnerRef, Owned};
use ftdb_sys::ftdb::ftdb_type_entry;

/// Represents a type definition in the source code
///
#[derive(Debug, Clone)]
pub struct TypeEntry<'a>(&'a ftdb_type_entry);

type_entry_impl!(TypeEntry<'a>);

impl std::fmt::Display for TypeEntry<'_> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "id:{}|class:{}", self.id(), self.classname())
    }
}

impl<'a> From<&'a ftdb_type_entry> for TypeEntry<'a> {
    fn from(t: &'a ftdb_type_entry) -> Self {
        Self(t)
    }
}

impl<'s, 'r> InnerRef<'s, 'r, ftdb_type_entry> for TypeEntry<'r> {
    fn as_inner_ref(&'s self) -> &'r ftdb_type_entry {
        self.0
    }
}

impl TypeEntry<'_> {
    /// Upgrade instance into owned one by providing a shared handle to the
    /// FTDB database
    ///
    pub fn into_owned(self, handle: std::sync::Arc<FtdbHandle>) -> super::owned::TypeEntry {
        let inner_ptr = self.0 as *const ftdb_type_entry;
        super::owned::TypeEntry::from(Owned {
            // Safety: Pointer is guaranteed to be non null
            // Converting reference to a pointer and ignoring its lifetime is
            // also safe as memory is valid as long as handle exists
            db: unsafe { std::ptr::NonNull::new_unchecked(inner_ptr.cast_mut()) },
            handle,
        })
    }
}
