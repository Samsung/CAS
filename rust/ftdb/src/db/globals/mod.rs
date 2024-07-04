mod borrowed;
mod owned;

use super::collection::{BorrowedIterator, FtdbCollection, IntoOwnedIterator};
use super::{FtdbHandle, GlobalId, Handle, InnerRef, Owned};
use crate::db::GlobalsQuery;
use crate::macros::impl_inner_handle;
use ftdb_sys::ftdb::{ftdb, ftdb_global_entry};
use std::fmt::Display;

pub use self::borrowed::GlobalEntry;

/// Structure providing access to global entries
///
pub struct Globals(Owned<ftdb>);

impl Display for Globals {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<Globals: {} globals>", self.len())
    }
}

impl FtdbCollection<ftdb_global_entry> for Globals {
    #[inline]
    unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_global_entry {
        self.inner_ref().get_ptr(index)
    }

    #[inline]
    fn len(&self) -> usize {
        <ftdb as FtdbCollection<ftdb_global_entry>>::len(self.inner_ref())
    }
}

impl_inner_handle!(Globals);

impl<'s> InnerRef<'s, 's, ftdb> for Globals {
    #[inline]
    fn inner_ref(&'s self) -> &'s ftdb {
        self.0.inner_ref()
    }
}

impl IntoIterator for Globals {
    type Item = owned::GlobalEntry;
    type IntoIter = IntoOwnedIterator<Globals, Self::Item, ftdb_global_entry>;

    fn into_iter(self) -> Self::IntoIter {
        IntoOwnedIterator::new(self)
    }
}

impl Globals {
    pub(crate) fn new(db: std::ptr::NonNull<ftdb>, handle: std::sync::Arc<FtdbHandle>) -> Self {
        Self(Owned { db, handle })
    }

    /// Get global by its unique identifier
    ///
    #[inline]
    pub fn entry_by_id(&self, id: GlobalId) -> Option<GlobalEntry<'_>> {
        self.0.global_by_id(id.0).map(borrowed::GlobalEntry::from)
    }

    /// Get global by its hash
    ///
    #[inline]
    pub fn entry_by_hash(&self, hash: &str) -> Option<GlobalEntry<'_>> {
        self.0.global_by_hash(hash).map(borrowed::GlobalEntry::from)
    }

    /// Find globals matching given name
    ///
    #[inline]
    pub fn entries_by_name<'p>(
        &'p self,
        name: &'p str,
    ) -> impl Iterator<Item = GlobalEntry<'_>> + 'p {
        self.0
            .globals_by_name(name)
            .map(borrowed::GlobalEntry::from)
    }

    /// Iterator through global entries
    ///
    #[inline]
    pub fn iter(&self) -> BorrowedIterator<'_, ftdb, GlobalEntry<'_>, ftdb_global_entry> {
        BorrowedIterator::new(self.inner_ref())
    }
}

#[derive(Debug, PartialEq, Eq)]
pub enum GlobalDef<'a> {
    Declaration(&'a str),
    TentativeDefinition(&'a str),
    Definition(&'a str),
    Unknown(&'a str, u32),
}

impl<'a> Display for GlobalDef<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let (deftype, defstring) = match *self {
            GlobalDef::Declaration(x) => (0, x),
            GlobalDef::TentativeDefinition(x) => (1, x),
            GlobalDef::Definition(x) => (2, x),
            GlobalDef::Unknown(x, t) => (t, x),
        };
        write!(f, "({}|{})", deftype, defstring)
    }
}

macro_rules! global_entry_impl {
    ($struct_name:ident) => {
        global_entry_impl!($struct_name, '_);
    };

    ($struct_name:ident <$struct_life:lifetime>) => {
        global_entry_impl!($struct_name<$struct_life>, $struct_life);
    };

    ($struct_name:ident $(<$struct_life:lifetime>)?, $ret_life:lifetime) => {
        use $crate::utils::{ptr_to_slice, ptr_to_str};
        use $crate::db::{FunctionId, GlobalDef, GlobalId, Linkage, TypeId};

        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {

            //// ftdb global entry decls values
            ///
            #[inline]
            pub fn decls(&self) -> &$ret_life [u64] {
                ptr_to_slice(self.inner_ref().decls, self.inner_ref().decls_count)
            }

            #[inline]
            pub fn def(&self) -> GlobalDef {
                let defstring = self.defstring();
                match self.deftype() {
                    0 => GlobalDef::Declaration(defstring),
                    1 => GlobalDef::TentativeDefinition(defstring),
                    2 => GlobalDef::Definition(defstring),
                    x => GlobalDef::Unknown(defstring, x),
                }
            }

            #[inline]
            pub fn defstring(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().def)
            }

            #[inline]
            pub fn deftype(&self) -> u32 {
                self.inner_ref().deftype
            }

            /// ftdb global entry hash value
            ///
            #[inline]
            pub fn hash(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().hash)
            }

            /// ftdb global entry fid value
            ///
            #[inline]
            pub fn fid(&self) -> u64 {
                self.inner_ref().fid
            }

            /// ftdb global entry funrefs values
            ///
            #[inline]
            pub fn funrefs(&self) -> &$ret_life [FunctionId] {
                ptr_to_slice(
                    self.inner_ref().funrefs as *const FunctionId,
                    self.inner_ref().funrefs_count
                )
            }

            /// ftdb global entry globalrefs values
            ///
            #[inline]
            pub fn globalrefs(&self) -> &$ret_life [GlobalId] {
                ptr_to_slice(
                    self.inner_ref().globalrefs as *const GlobalId,
                    self.inner_ref().globalrefs_count,
                )
            }

            /// ftdb global entry id value
            ///
            #[inline]
            pub fn id(&self) -> GlobalId {
                self.inner_ref().id.into()
            }

            /// ftdb global entry init value
            ///
            #[inline]
            pub fn init(&self) -> Option<&$ret_life str> {
                match self.inner_ref().hasinit {
                    0 => None,
                    _ => Some(ptr_to_str(self.inner_ref().init)),
                }
            }

            /// ftdb global entry linkage value
            ///
            #[inline]
            pub fn linkage(&self) -> Linkage {
                self.linkage_raw().into()
            }

            #[inline]
            pub fn linkage_raw(&self) -> ::ftdb_sys::ftdb::functionLinkage {
                self.inner_ref().linkage
            }

            /// ftdb global entry literals value
            ///
            #[inline]
            pub fn literals(&self) -> Literals<$ret_life> {
                self.inner_ref().into()
            }

            /// ftdb global entry location value
            ///
            #[inline]
            pub fn location(&self) -> Location<$ret_life> {
                ptr_to_str(self.0.location).into()
            }

            /// ftdb global entry mids values
            ///
            #[inline]
            pub fn mids(&self) -> &$ret_life [u64] {
                ptr_to_slice(self.inner_ref().mids, self.inner_ref().mids_count)
            }

            /// ftdb global entry name value
            ///
            #[inline]
            pub fn name(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().name)
            }

            /// ftdb global entry refs values
            ///
            #[inline]
            pub fn refs(&self) -> &$ret_life [u64] {
                ptr_to_slice(
                    self.inner_ref().refs,
                    self.inner_ref().refs_count
                )
            }

            /// ftdb global entry type value
            ///
            #[inline]
            pub fn type_(&self) -> TypeId {
                self.inner_ref().type_.into()
            }
        }
    }
}

pub(crate) use global_entry_impl;
