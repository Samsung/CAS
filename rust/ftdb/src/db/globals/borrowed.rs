use super::global_entry_impl;
use crate::{db::InnerRef, FtdbHandle, Owned};
use ftdb_sys::ftdb::ftdb_global_entry;
use std::{fmt::Display, sync::Arc};

#[derive(Debug, Clone)]
pub struct GlobalEntry<'a>(pub(crate) &'a ftdb_global_entry);

global_entry_impl!(GlobalEntry<'a>);

impl Display for GlobalEntry<'_> {
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
    fn as_inner_ref(&'s self) -> &'r ftdb_global_entry {
        self.0
    }
}

impl GlobalEntry<'_> {
    pub fn into_owned(self, handle: Arc<FtdbHandle>) -> super::owned::GlobalEntry {
        let inner_ptr = self.0 as *const ftdb_global_entry;
        super::owned::GlobalEntry::from(Owned {
            // Safety: Pointer is guaranteed to be non null
            // Converting reference to a pointer and ignoring its lifetime is
            // also safe as memory is valid as long as handle exists
            db: unsafe { std::ptr::NonNull::new_unchecked(inner_ptr.cast_mut()) },
            handle,
        })
    }
}
