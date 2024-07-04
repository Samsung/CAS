mod borrowed;
mod owned;

use super::{
    collection::{BorrowedIterator, FtdbCollection, IntoOwnedIterator},
    FtdbHandle, Handle, InnerRef, Owned,
};
use crate::{db::FuncDeclsQuery, macros::impl_inner_handle};
use ftdb_sys::ftdb::{ftdb, ftdb_funcdecl_entry};
use std::{fmt::Display, ptr::NonNull, sync::Arc};

pub use self::borrowed::FuncDeclEntry;

/// Structure providing access to function declarations data
///
pub struct FuncDecls(Owned<ftdb>);

impl Display for FuncDecls {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<FuncDecls: {} funcdecls>", self.len())
    }
}

impl FtdbCollection<ftdb_funcdecl_entry> for FuncDecls {
    #[inline]
    unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_funcdecl_entry {
        self.inner_ref().get_ptr(index)
    }

    #[inline]
    fn len(&self) -> usize {
        <ftdb as FtdbCollection<ftdb_funcdecl_entry>>::len(self.inner_ref())
    }
}

impl_inner_handle!(FuncDecls);

impl<'s> InnerRef<'s, 's, ftdb> for FuncDecls {
    fn inner_ref(&'s self) -> &'s ftdb {
        self.0.inner_ref()
    }
}

impl IntoIterator for FuncDecls {
    type Item = owned::FuncDeclEntry;
    type IntoIter = IntoOwnedIterator<FuncDecls, Self::Item, ftdb_funcdecl_entry>;

    fn into_iter(self) -> Self::IntoIter {
        IntoOwnedIterator::new(self)
    }
}

impl FuncDecls {
    pub(crate) fn new(db: NonNull<ftdb>, handle: Arc<FtdbHandle>) -> Self {
        Self(Owned { db, handle })
    }

    /// Construct a vector from inner slice to ftdb_funcdecl_entry
    ///
    pub fn as_vec(&self) -> Vec<borrowed::FuncDeclEntry<'_>> {
        self.iter().collect()
    }

    /// Find funcdecl entry by its id
    ///
    pub fn entry_by_id(&self, id: u64) -> Option<borrowed::FuncDeclEntry<'_>> {
        self.0.funcdecl_by_id(id).map(FuncDeclEntry::from)
    }

    /// Find funcdecl entry by its hash
    ///
    pub fn entry_by_declhash(&self, hash: &str) -> Option<borrowed::FuncDeclEntry<'_>> {
        self.0.funcdecl_by_declhash(hash).map(FuncDeclEntry::from)
    }

    /// Iterator over funcdecl entries
    ///
    pub fn iter(&self) -> BorrowedIterator<'_, ftdb, borrowed::FuncDeclEntry, ftdb_funcdecl_entry> {
        BorrowedIterator::new(self.inner_ref())
    }
}

macro_rules! func_decl_entry_impl {
    ($struct_name:ident) => {
        func_decl_entry_impl!($struct_name, '_);
    };

    ($struct_name:ident <$struct_life:lifetime>) => {
        func_decl_entry_impl!($struct_name<$struct_life>, $struct_life);
    };

    ($struct_name:ident $(<$struct_life:lifetime>)?, $ret_life:lifetime) => {
        use $crate::utils::{try_ptr_to_type, try_ptr_to_str, ptr_to_str, ptr_to_slice};
        use $crate::db::{FunctionId, Linkage, Location, TypeId};
        use ftdb_sys::ftdb::functionLinkage;

        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {

            /// Returns True if funcdecl entry is contained within a class
            ///
            pub fn has_class(&self) -> bool {
                let cls = try_ptr_to_type(self.inner_ref().__class);
                let cls_id = try_ptr_to_type(self.inner_ref().classid);
                cls.zip(cls_id).is_some()
            }

            /// ftdb funcdecl entry class value
            ///
            pub fn class(&self) -> Option<&$ret_life str> {
                try_ptr_to_str(self.inner_ref().__class)
            }

            /// ftdb funcdecl entry classid value
            ///
            pub fn classid(&self) -> Option<u64> {
                try_ptr_to_type(self.inner_ref().classid)
            }

            /// ftdb funcdecl entry decl value
            ///
            pub fn decl(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().decl)
            }

            /// ftdb funcdecl entry declhash value
            ///
            pub fn declhash(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().declhash)
            }

            /// ftdb funcdecl entry fid value
            ///
            pub fn fid(&self) -> u64 {
                self.inner_ref().fid
            }

            /// ftdb funcdecl entry id value
            ///
            pub fn id(&self) -> FunctionId {
                self.inner_ref().id.into()
            }

            /// ftdb funcdecl entry is member value
            ///
            pub fn is_member(&self) -> bool {
                match try_ptr_to_type(self.inner_ref().ismember) {
                    Some(x) => x != 0,
                    None => false,
                }
            }

            /// ftdb funcdecl entry template_parameters value
            ///
            pub fn is_template(&self) -> bool {
                try_ptr_to_type(self.inner_ref().istemplate)
                    .map(|x| x != 0)
                    .unwrap_or(false)
            }

            /// ftdb funcdecl entry is variadic value
            ///
            pub fn is_variadic(&self) -> bool {
                self.inner_ref().isvariadic != 0
            }

            /// ftdb funcdecl entry linkage value
            ///
            pub fn linkage(&self) -> Linkage {
                self.inner_ref().linkage.into()
            }

            /// ftdb funcdecl entry linkage value
            ///
            pub fn linkage_string(&self) -> &$ret_life str {
                match self.inner_ref().linkage {
                    functionLinkage::LINKAGE_NONE => "none",
                    functionLinkage::LINKAGE_INTERNAL => "internal",
                    functionLinkage::LINKAGE_EXTERNAL => "external",
                    _ => "other",
                }
            }

            /// tdb funcdecl entry location value
            ///
            pub fn location(&self) -> Location<$ret_life> {
                ptr_to_str(self.inner_ref().location).into()
            }

            /// ftdb funcdecl entry name value
            ///
            pub fn name(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().name)
            }

            /// ftdb funcdecl entry namespace value
            ///
            pub fn namespace(&self) -> Option<&$ret_life str> {
                try_ptr_to_str(self.inner_ref().__namespace)
            }

            /// ftdb funcdecl entry nargs value
            ///
            pub fn nargs(&self) -> usize {
                self.inner_ref().nargs as usize
            }

            /// ftdb funcdecl entry refcount value
            ///
            pub fn refcount(&self) -> usize {
                self.inner_ref().refcount as usize
            }

            /// ftdb funcdecl entry signature value
            ///
            pub fn signature(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().signature)
            }

            /// ftdb funcdecl entry template_parameters value"
            ///
            pub fn template_parameters(&self) -> Option<&$ret_life str> {
                try_ptr_to_str(self.inner_ref().template_parameters)
            }

            /// ftdb funcdecl entry types list
            ///
            pub fn types(&self) -> &$ret_life [TypeId] {
                ptr_to_slice(
                    self.inner_ref().types as *const TypeId,
                    self.inner_ref().types_count
                )
            }
        }
    };
}

pub(crate) use func_decl_entry_impl;
