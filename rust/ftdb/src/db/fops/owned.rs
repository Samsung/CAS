use super::{fops_entry_impl, fops_member_entry_impl};
use crate::db::{collection::BorrowedIterator, FtdbHandle, Handle, InnerRef, Owned, ToBorrowed};
use crate::macros::impl_inner_handle;
use ftdb_sys::ftdb::{ftdb_fops_entry, ftdb_fops_member_entry};

/// Structure represents a single fop entry. It stores a shared reference to FTDB handle
/// so it is safe to use it in multithreaded environment
///
#[derive(Debug, Clone)]
pub struct FopsEntry(Owned<ftdb_fops_entry>);

fops_entry_impl!(FopsEntry);

impl FopsEntry {
    pub fn members(&self) -> Vec<FopsMember> {
        ptr_to_slice(
            self.as_inner_ref().members,
            self.as_inner_ref().members_count,
        )
        .iter()
        .map(|db| {
            let inner_ptr = db as *const ftdb_fops_member_entry;
            FopsMember(Owned {
                // Safety: Pointer is guaranteed to be non null
                // Converting reference to a pointer and ignoring its lifetime is
                // also safe as memory is valid as long as handle exists
                db: unsafe { std::ptr::NonNull::new_unchecked(inner_ptr.cast_mut()) },
                handle: self.handle(),
            })
        })
        .collect()
    }
}

impl From<Owned<ftdb_fops_entry>> for FopsEntry {
    fn from(value: Owned<ftdb_fops_entry>) -> Self {
        Self(value)
    }
}

impl_inner_handle!(FopsEntry);

impl<'s> InnerRef<'s, 's, ftdb_fops_entry> for FopsEntry {
    fn as_inner_ref(&'s self) -> &'s ftdb_fops_entry {
        self.0.as_inner_ref()
    }
}

impl<'a> ToBorrowed<'a> for FopsEntry {
    type Type = super::FopsEntry<'a>;

    fn to_borrowed(&'a self) -> Self::Type {
        self.as_inner_ref().into()
    }
}

#[derive(Debug, Clone)]
pub struct FopsMember(Owned<ftdb_fops_member_entry>);

fops_member_entry_impl!(FopsMember);

impl From<Owned<ftdb_fops_member_entry>> for FopsMember {
    fn from(value: Owned<ftdb_fops_member_entry>) -> Self {
        Self(value)
    }
}

impl_inner_handle!(FopsMember);

impl<'s> InnerRef<'s, 's, ftdb_fops_member_entry> for FopsMember {
    fn as_inner_ref(&'s self) -> &'s ftdb_fops_member_entry {
        self.0.as_inner_ref()
    }
}
