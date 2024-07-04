use super::{
    collection::{BorrowedIterator, FtdbCollection, IntoOwnedIterator},
    FtdbHandle, Handle, InnerRef, Owned,
};
use crate::{macros::impl_inner_handle, FunctionId};
use ftdb_sys::ftdb::{ftdb, ftdb_func_entry, query::FuncEntryQuery};
use std::{ptr::NonNull, sync::Arc};

mod asms;
mod borrowed;
mod calls;
mod deref;
mod entry;
mod extensions;
mod globalref;
mod ifs;
mod location;
mod owned;
mod refcall;
mod switches;
mod taint;

pub use self::asms::*;
pub use self::calls::*;
pub use self::deref::*;
pub use self::entry::*;
pub use self::extensions::DfsFunctionExt;
pub use self::globalref::*;
pub use self::ifs::*;
pub use self::location::*;
pub use self::refcall::*;
pub use self::switches::*;
pub use self::taint::*;

pub use self::borrowed::FunctionEntry;
pub use self::owned::FunctionEntry as OwnedFunctionEntry;

///
///
pub struct Functions(Owned<ftdb>);

impl FtdbCollection<ftdb_func_entry> for Functions {
    #[inline]
    unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_func_entry {
        self.inner_ref().get_ptr(index)
    }

    #[inline]
    fn len(&self) -> usize {
        <ftdb as FtdbCollection<ftdb_func_entry>>::len(self.inner_ref())
    }
}

impl_inner_handle!(Functions);

impl<'s> InnerRef<'s, 's, ftdb> for Functions {
    #[inline]
    fn inner_ref(&'s self) -> &'s ftdb {
        self.0.inner_ref()
    }
}

impl IntoIterator for Functions {
    type Item = owned::FunctionEntry;
    type IntoIter = IntoOwnedIterator<Self, Self::Item, ftdb_func_entry>;

    fn into_iter(self) -> Self::IntoIter {
        IntoOwnedIterator::new(self)
    }
}

impl Functions {
    pub(super) fn new(db: NonNull<ftdb>, handle: Arc<FtdbHandle>) -> Self {
        Self(Owned { db, handle })
    }

    /// Get function by its unique identifier
    ///
    #[inline]
    pub fn get_by_id(&self, id: FunctionId) -> Option<FunctionEntry<'_>> {
        self.0.func_by_id(id.0).map(FunctionEntry::from)
    }

    /// Get function by its hash
    ///
    #[inline]
    pub fn get_by_hash(&self, hash: &str) -> Option<FunctionEntry> {
        self.0.func_by_hash(hash).map(FunctionEntry::from)
    }

    /// Get functions by a name
    ///
    ///
    #[inline]
    pub fn get_by_name<'p>(
        &'p self,
        name: &'p str,
    ) -> impl Iterator<Item = FunctionEntry<'_>> + 'p {
        self.0.funcs_by_name(name).map(FunctionEntry::from)
    }

    /// Iterator through function entries
    ///
    #[inline]
    pub fn iter(&self) -> BorrowedIterator<'_, Functions, FunctionEntry<'_>, ftdb_func_entry> {
        BorrowedIterator::new(self)
    }
}
