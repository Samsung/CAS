use super::borrowed::FopsMemberEntry as BorrowedMemberEntry;
use super::{fops_entry_impl, fops_member_entry_impl, fops_members_impl};
use crate::db::{
    collection::{BorrowedIterator, FtdbCollection, IntoOwnedIterator},
    FtdbHandle, Handle, InnerRef, Owned, ToBorrowed,
};
use crate::macros::impl_inner_handle;
use ftdb_sys::ftdb::{ftdb_fops_entry, ftdb_fops_member_entry};

/// Structure represents a single fop entry. It stores a shared reference to FTDB handle
/// so it is safe to use it in multithreaded environment
///
pub struct FopsEntry(Owned<ftdb_fops_entry>);

fops_entry_impl!(FopsEntry);

impl From<Owned<ftdb_fops_entry>> for FopsEntry {
    fn from(value: Owned<ftdb_fops_entry>) -> Self {
        Self(value)
    }
}

impl_inner_handle!(FopsEntry);

impl<'s> InnerRef<'s, 's, ftdb_fops_entry> for FopsEntry {
    fn inner_ref(&'s self) -> &'s ftdb_fops_entry {
        self.0.inner_ref()
    }
}

impl<'a> ToBorrowed<'a> for FopsEntry {
    type Type = super::FopsEntry<'a>;

    fn to_borrowed(&'a self) -> Self::Type {
        self.inner_ref().into()
    }
}

impl FopsEntry {
    /// A map of function ids assigned to struct fields
    ///
    pub fn members(&self) -> FopsMembers {
        FopsMembers(Owned {
            db: self.0.db,
            handle: self.0.handle.clone(),
        })
    }

    /// Moves into iterator over OwnedFopsMemberEntry items
    ///
    pub fn into_members_iter(
        self,
    ) -> IntoOwnedIterator<FopsEntry, FopsMemberEntry, ftdb_fops_member_entry> {
        IntoOwnedIterator::new(self)
    }
}

pub struct FopsMembers(Owned<ftdb_fops_entry>);

fops_members_impl!(FopsMembers, BorrowedMemberEntry<'_>);

impl FtdbCollection<ftdb_fops_member_entry> for FopsMembers {
    #[inline]
    unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_fops_member_entry {
        self.inner_ref().get_ptr(index)
    }

    #[inline]
    fn len(&self) -> usize {
        self.inner_ref().len()
    }
}

impl_inner_handle!(FopsMembers);

impl<'s> InnerRef<'s, 's, ftdb_fops_entry> for FopsMembers {
    fn inner_ref(&'s self) -> &'s ftdb_fops_entry {
        self.0.inner_ref()
    }
}

impl IntoIterator for FopsMembers {
    type Item = FopsMemberEntry;
    type IntoIter = IntoOwnedIterator<Self, Self::Item, ftdb_fops_member_entry>;

    fn into_iter(self) -> Self::IntoIter {
        IntoOwnedIterator::new(self)
    }
}

pub struct FopsMemberEntry(Owned<ftdb_fops_member_entry>);

fops_member_entry_impl!(FopsMemberEntry);

impl From<Owned<ftdb_fops_member_entry>> for FopsMemberEntry {
    fn from(value: Owned<ftdb_fops_member_entry>) -> Self {
        Self(value)
    }
}

impl_inner_handle!(FopsMemberEntry);

impl<'s> InnerRef<'s, 's, ftdb_fops_member_entry> for FopsMemberEntry {
    fn inner_ref(&'s self) -> &'s ftdb_fops_member_entry {
        self.0.inner_ref()
    }
}
