use super::{fops_entry_impl, fops_member_entry_impl, fops_members_impl};
use crate::db::{collection::BorrowedIterator, InnerRef};
use ftdb_sys::ftdb::{ftdb_fops_entry, ftdb_fops_member_entry};

/// Fops entry represents one struct type object with members
/// initialized/assigned with functions.
///
/// There are three possible kinds:
/// - "global"   object is a global variable
/// - "local"    object is a local variable
/// - "function" object couldn't be determined; holds info about
///              function where assignment took place
///
#[derive(Debug)]
pub struct FopsEntry<'a>(&'a ftdb_fops_entry);

fops_entry_impl!(FopsEntry<'a>);

impl<'a> From<&'a ftdb_fops_entry> for FopsEntry<'a> {
    fn from(value: &'a ftdb_fops_entry) -> Self {
        Self(value)
    }
}

impl<'s, 'r> InnerRef<'s, 'r, ftdb_fops_entry> for FopsEntry<'r> {
    fn inner_ref(&'s self) -> &'r ftdb_fops_entry {
        self.0
    }
}

impl<'a> FopsEntry<'a> {
    /// A map of function ids assigned to struct fields
    ///
    pub fn members(&self) -> FopsMembers<'a> {
        FopsMembers(self.0)
    }
}

pub struct FopsMembers<'a>(&'a ftdb_fops_entry);

fops_members_impl!(FopsMembers<'a>, FopsMemberEntry<'a>);

impl<'s, 'r> InnerRef<'s, 'r, ftdb_fops_entry> for FopsMembers<'r> {
    fn inner_ref(&'s self) -> &'r ftdb_fops_entry {
        self.0
    }
}

pub struct FopsMemberEntry<'a>(&'a ftdb_fops_member_entry);

fops_member_entry_impl!(FopsMemberEntry<'a>);

impl<'a> From<&'a ftdb_fops_member_entry> for FopsMemberEntry<'a> {
    fn from(value: &'a ftdb_fops_member_entry) -> Self {
        Self(value)
    }
}

impl<'s, 'r> InnerRef<'s, 'r, ftdb_fops_member_entry> for FopsMemberEntry<'r> {
    fn inner_ref(&'s self) -> &'r ftdb_fops_member_entry {
        self.0
    }
}
