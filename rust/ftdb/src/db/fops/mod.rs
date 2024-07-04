mod borrowed;
mod owned;

use crate::macros::impl_inner_handle;
use crate::GlobalId;

pub use self::borrowed::FopsEntry;
pub use self::owned::FopsEntry as OwnedFopsEntry;
use super::Handle;
use super::{
    collection::{BorrowedIterator, FtdbCollection, IntoOwnedIterator},
    FtdbHandle, InnerRef, Owned,
};
use ftdb_sys::ftdb::{ftdb, ftdb_fops_entry};
use std::{fmt::Display, sync::Arc};

/// Structure providing access to FTDB data related to fops.
///
/// A fop entry is defined as function that is used in initialization of a field in
/// some structure (either global or local)
///
pub struct Fops(Owned<ftdb>);

impl Fops {
    /// Create new instance of Fops
    ///
    pub(crate) fn new(db: std::ptr::NonNull<ftdb>, handle: Arc<FtdbHandle>) -> Self {
        Self(Owned { db, handle })
    }

    /// Iterator through fops entries
    ///
    pub fn iter(&self) -> BorrowedIterator<'_, ftdb, borrowed::FopsEntry<'_>, ftdb_fops_entry> {
        BorrowedIterator::new(self.inner_ref())
    }
}

impl Display for Fops {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<Fops: {} entries>", self.len())
    }
}

impl FtdbCollection<ftdb_fops_entry> for Fops {
    #[inline]
    unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_fops_entry {
        self.inner_ref().get_ptr(index)
    }

    #[inline]
    fn len(&self) -> usize {
        <ftdb_sys::ftdb::ftdb as FtdbCollection<ftdb_fops_entry>>::len(self.inner_ref())
    }
}

impl_inner_handle!(Fops);
impl<'s> InnerRef<'s, 's, ftdb> for Fops {
    fn inner_ref(&'s self) -> &'s ftdb {
        &self.0
    }
}

impl IntoIterator for Fops {
    type Item = owned::FopsEntry;
    type IntoIter = IntoOwnedIterator<Fops, Self::Item, ftdb_fops_entry>;

    fn into_iter(self) -> Self::IntoIter {
        IntoOwnedIterator::new(self)
    }
}

macro_rules! fops_entry_impl {
    ($struct_name:ident<$struct_life:lifetime>) => {
        fops_entry_impl!($struct_name<$struct_life>, $struct_life, $struct_life);
    };

    ($struct_name:ident) => {
        fops_entry_impl!($struct_name, '_);
    };


    ($struct_name:ident $(<$struct_life:lifetime>)?, $ret_life:lifetime $(, $inner_life:lifetime)?) => {
        use ftdb_sys::ftdb::fopsKind;
        use $crate::db::{FunctionId, Location, TypeId, VarId};
        use $crate::utils::ptr_to_str;

        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {
            ///
            ///
            pub fn is_global(&self) -> bool {
                self.kind() == fopsKind::FOPSKIND_GLOBAL
            }

            /// Entry kind
            ///
            /// It might be a global, local or function. Check out docs of FopsEntry
            /// type for more details.
            ///
            /// It should not be used directly as both var_id and func_id
            /// validate fopsKind value.
            ///
            /// FIXME: Not possible to use this value for a now as function
            /// returns value of a type from internal api
            pub fn kind(&self) -> fopsKind {
                self.inner_ref().kind
            }

            /// ID of a struct type with function pointer members
            ///
            pub fn type_id(&self) -> TypeId {
                self.inner_ref().type_id.into()
            }

            /// ID of a variable (if applicable)
            ///
            /// ID might refer to either global or local variable, depending
            /// on where assignment took place.
            ///
            pub fn var_id(&self) -> VarId {
                match self.kind() {
                    fopsKind::FOPSKIND_GLOBAL => VarId::Global(self.inner_ref().var_id.into()),
                    fopsKind::FOPSKIND_LOCAL => VarId::Local(self.inner_ref().var_id),
                    fopsKind::FOPSKIND_FUNCTION => VarId::Unknown,
                    _ => panic!("Unsupported case!"),
                }
            }

            /// ID of a function associated with the object
            ///
            /// This value is valid for kinds other than "global"
            ///
            pub fn func_id(&self) -> Option<FunctionId> {
                match self.kind() {
                    fopsKind::FOPSKIND_GLOBAL => None,
                    fopsKind::FOPSKIND_LOCAL => Some(self.inner_ref().func_id.into()),
                    fopsKind::FOPSKIND_FUNCTION => Some(self.inner_ref().func_id.into()),
                    _ => panic!("Unsupported case!"),
                }
            }

            /// Location associated with the object
            ///
            pub fn location(&self) -> Location<$ret_life> {
                ptr_to_str(self.inner_ref().location).into()
            }

            /// Returns iterator over FopsMemberEntry items
            ///
            pub fn members_iter(
                &self,
            ) -> BorrowedIterator<$ret_life, ftdb_fops_entry, FopsMemberEntry$(<$inner_life>)?, ftdb_fops_member_entry> {
                BorrowedIterator::new(self.inner_ref())
            }
        }
    }
}

macro_rules! fops_members_impl {
    (   $struct_name:ident $(<$struct_life:lifetime>)? ,
        $return_type:ident $(<$return_life:lifetime>)?
    ) => {
        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {
            ///
            ///
            pub fn entry_by_member_id(
                &$($struct_life)?self,
                member_index: usize
            ) -> Option<$return_type $(<$return_life>)?> {
                self.iter().find(|entry| entry.member_id() == member_index)
            }

            /// Iterate through FopsMember objects
            ///
            pub fn iter(
                &self,
            ) -> BorrowedIterator<
                    '_,
                    ftdb_fops_entry,
                    $return_type $(<$return_life>)?,
                    ftdb_fops_member_entry
                >
            {
                BorrowedIterator::new(self.inner_ref())
            }
        }

    };
}

macro_rules! fops_member_entry_impl {
    ($struct_name:ident $(<$struct_life:lifetime>)?) => {
        use $crate::utils::ptr_to_slice;

        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {
            /// Index of a field in a structure
            ///
            #[inline]
            pub fn member_id(&self) -> usize {
                self.inner_ref().member_id as usize
            }

            /// Functions assigned to a given field.
            ///
            /// In most cases the returned list contains a single entry only.
            /// However in a scenario where conditional statements (if/else, etc)
            /// are used to determine an assignment, this list might contain
            /// more entries.
            ///
            #[inline]
            pub fn func_ids(&self) -> &[FunctionId] {
                let inner = self.inner_ref();
                ptr_to_slice(
                    inner.func_ids as *const FunctionId,
                    inner.func_ids_count
                )
            }
        }

    };
}

pub(crate) use fops_entry_impl;
pub(crate) use fops_member_entry_impl;
pub(crate) use fops_members_impl;

pub enum VarId {
    Global(GlobalId),
    Local(u64),
    Unknown,
}
