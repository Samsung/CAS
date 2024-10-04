mod borrowed;
mod owned;

use crate::macros::impl_inner_handle;
use crate::GlobalId;

pub use self::borrowed::{FopsEntry, FopsMember};
pub use self::owned::{FopsEntry as OwnedFopsEntry, FopsMember as OwnedFopsMemberEntry};
use super::{
    collection::{BorrowedIterator, FtdbCollection, IntoOwnedIterator},
    FtdbHandle, InnerRef, Owned,
};
use super::{FunctionId, Handle, LocalId};
use ftdb_sys::ftdb::{ftdb, ftdb_fops_entry};
use std::{fmt::Display, sync::Arc};

/// Structure providing access to FTDB data related to fops.
///
/// A fop entry is defined as function that is used in initialization of a field in
/// some structure (either global or local).
///
/// A 'fop' name comes from 'file_operations' structure of a linux kernel in which
/// functions handling various syscalls (read, write, ioctl, etc) are assigned to
/// variables of type `file_operations` (and several others) which then are used to
/// register these syscall handlers.
///
/// There are three types of fop entries, based on type of object with functions
/// initialized/assigned to its fields:
///
/// * `global` - an object is a global variable
/// * `local` - an object is a local variable
/// * `function` - an object could not be determined
///
///
#[derive(Debug, Clone)]
pub struct Fops(Owned<ftdb>);

impl Fops {
    /// Create new instance of Fops that owns the (shared) database handle.
    ///
    /// Returned instance will extend lifetime of FTDB inner pointers.
    ///
    /// # Arguments
    ///
    /// * `db` - Pointer to the underlying structure, retrieved from `libftdb_c_ftdb_object()`
    /// * `handle` - A shared reference to the pointer retrieved from `libftdb_c_ftdb_load()`
    ///
    pub(crate) fn new(db: std::ptr::NonNull<ftdb>, handle: Arc<FtdbHandle>) -> Self {
        Self(Owned { db, handle })
    }

    /// Iterate through fops section entries
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb, fops: &ftdb::Fops)  {
    /// for fop in fops.iter() {
    ///     println!("\nType {} assigns:", fop.type_id());
    ///     for members in fop.members_iter() {
    ///         for func_id in members.func_ids() {
    ///             if let Some(f) = db.func_by_id(*func_id) {
    ///                 println!("  {:5}: {}", f.id(), f.name());
    ///             }
    ///         }
    ///     }
    /// }
    /// # }
    /// ```
    ///
    pub fn iter(&self) -> BorrowedIterator<'_, ftdb, borrowed::FopsEntry<'_>, ftdb_fops_entry> {
        BorrowedIterator::new(self.as_inner_ref())
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
        self.as_inner_ref().get_ptr(index)
    }

    #[inline]
    fn len(&self) -> usize {
        <ftdb_sys::ftdb::ftdb as FtdbCollection<ftdb_fops_entry>>::len(self.as_inner_ref())
    }
}

impl_inner_handle!(Fops);
impl<'s> InnerRef<'s, 's, ftdb> for Fops {
    fn as_inner_ref(&'s self) -> &'s ftdb {
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

        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {
            /// Returns true if function is assigned to a global structure
            ///
            #[inline]
            pub fn is_global(&self) -> bool {
                self.kind() == ftdb_sys::ftdb::fopsKind::FOPSKIND_GLOBAL
            }

            /// Entry kind
            ///
            /// It might be a global, local or function. Check out docs of
            /// [`FopsEntry``] type for more details.
            ///
            /// It should not be used directly as both var_id and func_id
            /// validate fopsKind value.
            ///
            #[inline]
            fn kind(&self) -> ftdb_sys::ftdb::fopsKind {
                self.as_inner_ref().kind
            }

            /// ID of a struct type with function pointer members
            ///
            #[inline]
            pub fn type_id(&self) -> $crate::db::TypeId {
                self.as_inner_ref().type_id.into()
            }

            /// ID of a variable (if applicable)
            ///
            /// ID might refer to either global or local variable, depending
            /// on where assignment took place.
            ///
            #[inline]
            pub fn var_id(&self) -> $crate::db::VarId {
                match self.kind() {
                    ftdb_sys::ftdb::fopsKind::FOPSKIND_GLOBAL => $crate::db::VarId::Global(
                        self.as_inner_ref().var_id.into()
                    ),
                    ftdb_sys::ftdb::fopsKind::FOPSKIND_LOCAL => $crate::db::VarId::Local(
                        self.as_inner_ref().func_id.into(),
                        self.as_inner_ref().var_id.into()
                    ),
                    ftdb_sys::ftdb::fopsKind::FOPSKIND_FUNCTION => $crate::db::VarId::Unknown,
                    _ => panic!("Unsupported case!"),
                }
            }

            /// ID of a function associated with the object
            ///
            /// This value is valid for kinds other than "global"
            ///
            #[inline]
            pub fn func_id(&self) -> Option<$crate::db::FunctionId> {
                match self.kind() {
                    ftdb_sys::ftdb::fopsKind::FOPSKIND_GLOBAL => None,
                    ftdb_sys::ftdb::fopsKind::FOPSKIND_LOCAL => Some(self.as_inner_ref().func_id.into()),
                    ftdb_sys::ftdb::fopsKind::FOPSKIND_FUNCTION => Some(self.as_inner_ref().func_id.into()),
                    _ => panic!("Unsupported case!"),
                }
            }

            /// Location associated with the struct instance
            ///
            /// TODO: ???
            ///
            #[inline]
            pub fn location(&self) -> $crate::db::Location<$ret_life> {
                $crate::utils::ptr_to_str(self.as_inner_ref().location).into()
            }

            /// Iterate over fields of a structure and function(s) assigned to them.
            ///
            /// # Examples
            ///
            /// ```
            /// # fn run(db: &ftdb::Ftdb, fop: ftdb::FopsEntry<'_>) {
            /// for entry in fop.members_iter() {
            ///     println!("Field #{}:", entry.member_id());
            ///     for func_id in entry.func_ids() {
            ///         if let Some(f) = db.func_by_id(*func_id) {
            ///             println!(" - Function #{}: {}", f.id(), f.name());
            ///         }
            ///     }
            /// }
            /// # }
            /// ```
            ///
            pub fn members_iter(
                &self,
            ) -> BorrowedIterator<$ret_life, ftdb_fops_entry, $crate::FopsMember$(<$inner_life>)?, ftdb_fops_member_entry> {
                BorrowedIterator::new(self.as_inner_ref())
            }
        }
    }
}

macro_rules! fops_member_entry_impl {
    ($struct_name:ident $(<$struct_life:lifetime>)?) => {
        use $crate::utils::ptr_to_slice;

        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {
            /// Index of a field in a structure
            ///
            #[inline]
            pub fn member_id(&self) -> usize {
                self.as_inner_ref().member_id as usize
            }

            /// Functions assigned to a given field of a structure
            ///
            /// In most cases the returned list contains a single entry only.
            /// However in a scenario where conditional statements (if/else, etc)
            /// are used to determine an assignment, this list might contain
            /// more entries.
            ///
            #[inline]
            pub fn func_ids(&self) -> &[$crate::db::FunctionId] {
                let inner = self.as_inner_ref();
                ptr_to_slice(
                    inner.func_ids as *const $crate::db::FunctionId,
                    inner.func_ids_count
                )
            }
        }

    };
}

pub(crate) use fops_entry_impl;
pub(crate) use fops_member_entry_impl;

/// Id of an variable that might come from various contexts
///
#[derive(Debug, Clone, Copy, Hash, PartialEq, Eq, PartialOrd, Ord)]
pub enum VarId {
    /// Variable is a global of a given Id
    Global(GlobalId),

    /// Variable is a local of a function of specified id and offset into
    Local(FunctionId, LocalId),

    /// Source of variable is unknown
    Unknown,
}
