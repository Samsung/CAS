mod borrowed;
mod owned;

use crate::macros::impl_inner_handle;

use super::{
    BorrowedIterator, FtdbCollection, FtdbHandle, Handle, InnerRef, IntoOwnedIterator, Owned,
};
use ftdb_sys::ftdb::{ftdb, ftdb_unresolvedfunc_entry};
use std::fmt::Display;
use std::ptr::NonNull;
use std::sync::Arc;

pub use self::borrowed::UnresolvedFuncEntry;
pub use self::owned::UnresolvedFuncEntry as OwnedUnresolvedFuncEntry;

/// Structure providing access to collection of unresolved functions
///
pub struct UnresolvedFuncs(Owned<ftdb>);

impl Display for UnresolvedFuncs {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<UnresolvedFuncs: {} items>", self.len())
    }
}

impl FtdbCollection<ftdb_unresolvedfunc_entry> for UnresolvedFuncs {
    unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_unresolvedfunc_entry {
        self.inner_ref().get_ptr(index)
    }

    fn len(&self) -> usize {
        <ftdb as FtdbCollection<ftdb_unresolvedfunc_entry>>::len(self.inner_ref())
    }
}

impl_inner_handle!(UnresolvedFuncs);

impl<'s> InnerRef<'s, 's, ftdb> for UnresolvedFuncs {
    fn inner_ref(&'s self) -> &'s ftdb {
        self.0.inner_ref()
    }
}

impl IntoIterator for UnresolvedFuncs {
    type Item = OwnedUnresolvedFuncEntry;
    type IntoIter = IntoOwnedIterator<UnresolvedFuncs, Self::Item, ftdb_unresolvedfunc_entry>;

    fn into_iter(self) -> Self::IntoIter {
        IntoOwnedIterator::new(self)
    }
}

impl UnresolvedFuncs {
    pub(crate) fn new(db: NonNull<ftdb>, handle: Arc<FtdbHandle>) -> Self {
        Self(Owned { db, handle })
    }

    /// Iterate through unresolved function entries
    ///
    pub fn iter(&self) -> impl ExactSizeIterator<Item = UnresolvedFuncEntry<'_>> {
        BorrowedIterator::<'_, ftdb, UnresolvedFuncEntry<'_>, ftdb_unresolvedfunc_entry>::new(
            self.inner_ref(),
        )
    }
}

macro_rules! unresolved_funcs_entry_impl {
    ($struct_name:ident $(<$struct_life:lifetime>)?) => {
        use $crate::utils::ptr_to_str;

        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {
            fn id(&self) -> $crate::db::FunctionId {
                self.inner_ref().id.into()
            }

            fn name(&self) -> &$($struct_life)? str {
                ptr_to_str(self.inner_ref().name)
            }
        }

    };
}

pub(crate) use unresolved_funcs_entry_impl;
