use std::sync::Arc;

use super::{fops_entry_impl, fops_member_entry_impl};
use crate::{
    db::{collection::BorrowedIterator, InnerRef},
    FtdbHandle, Owned,
};
use ftdb_sys::ftdb::{ftdb_fops_entry, ftdb_fops_member_entry};

/// Fops entry represents  struct type object with members
/// initialized/assigned with functions.
///
/// Represents a struct instance that has its members initialized or assigned to
/// some functions.
///
/// There are three possible kinds of entries, depending on were this struct
/// instance came from:
///
/// * `global` - instance is a global variable
/// * `local` - instance is a local variable
/// * `function` - instance couldn't be determined; holds info about function where
///                assignment took place
///
#[derive(Debug, Clone)]
pub struct FopsEntry<'a>(&'a ftdb_fops_entry);

fops_entry_impl!(FopsEntry<'a>);

impl<'a> From<&'a ftdb_fops_entry> for FopsEntry<'a> {
    fn from(value: &'a ftdb_fops_entry) -> Self {
        Self(value)
    }
}

impl<'s, 'r> InnerRef<'s, 'r, ftdb_fops_entry> for FopsEntry<'r> {
    fn as_inner_ref(&'s self) -> &'r ftdb_fops_entry {
        self.0
    }
}

impl<'a> FopsEntry<'a> {
    pub fn into_owned(self, handle: Arc<FtdbHandle>) -> super::owned::FopsEntry {
        let inner_ptr = self.0 as *const ftdb_fops_entry;
        super::owned::FopsEntry::from(Owned {
            // Safety: Pointer is guaranteed to be non null
            // Converting reference to a pointer and ignoring its lifetime is
            // also safe as memory is valid as long as handle exists
            db: unsafe { std::ptr::NonNull::new_unchecked(inner_ptr.cast_mut()) },
            handle,
        })
    }
}

#[derive(Debug, Clone)]
pub struct FopsMember<'a>(&'a ftdb_fops_member_entry);

fops_member_entry_impl!(FopsMember<'a>);

impl<'a> From<&'a ftdb_fops_member_entry> for FopsMember<'a> {
    fn from(value: &'a ftdb_fops_member_entry) -> Self {
        Self(value)
    }
}

impl<'s, 'r> InnerRef<'s, 'r, ftdb_fops_member_entry> for FopsMember<'r> {
    fn as_inner_ref(&'s self) -> &'r ftdb_fops_member_entry {
        self.0
    }
}

impl<'a> FopsMember<'a> {
    pub fn into_owned(self, handle: Arc<FtdbHandle>) -> super::owned::FopsMember {
        let inner_ptr = self.0 as *const ftdb_fops_member_entry;
        super::owned::FopsMember::from(Owned {
            // Safety: Pointer is guaranteed to be non null
            // Converting reference to a pointer and ignoring its lifetime is
            // also safe as memory is valid as long as handle exists
            db: unsafe { std::ptr::NonNull::new_unchecked(inner_ptr.cast_mut()) },
            handle,
        })
    }
}
