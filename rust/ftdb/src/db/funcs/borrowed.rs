use super::{asms::Asms, calls::Calls, entry::func_decl_entry_impl};
use crate::db::{FtdbHandle, InnerRef, Owned};
use ftdb_sys::ftdb::ftdb_func_entry;
use std::{fmt::Display, sync::Arc};

#[derive(Debug, Clone)]
pub struct FunctionEntry<'a>(pub(crate) &'a ftdb_func_entry);

func_decl_entry_impl!(FunctionEntry<'a>, 'a);

impl<'a> Display for FunctionEntry<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<Function: {} | {}>", self.id(), self.name())
    }
}

impl<'a> From<&'a ftdb_func_entry> for FunctionEntry<'a> {
    fn from(value: &'a ftdb_func_entry) -> Self {
        Self(value)
    }
}

impl<'s, 'r> InnerRef<'s, 'r, ftdb_func_entry> for FunctionEntry<'r> {
    #[inline]
    fn inner_ref(&'s self) -> &'r ftdb_func_entry {
        self.0
    }
}

impl<'a> FunctionEntry<'a> {
    pub fn into_owned(self, handle: Arc<FtdbHandle>) -> super::owned::FunctionEntry {
        let inner_ptr = self.0 as *const ftdb_func_entry;
        super::owned::FunctionEntry::from(Owned {
            // Safety: Pointer is guaranteed to be non null
            // Converting reference to a pointer and ignoring its lifetime is
            // also safe as memory is valid as long as handle exists
            db: unsafe { std::ptr::NonNull::new_unchecked(inner_ptr.cast_mut()) },
            handle,
        })
    }
}
