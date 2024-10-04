mod borrowed;
mod owned;

use super::{
    collection::{BorrowedIterator, FtdbCollection, IntoOwnedIterator},
    FtdbHandle, Handle, InnerRef, Owned,
};
use crate::{db::FuncDeclsQuery, macros::impl_inner_handle, FunctionId};
use ftdb_sys::ftdb::{ftdb, ftdb_funcdecl_entry};
use std::{fmt::Display, ptr::NonNull, sync::Arc};

pub use self::borrowed::FuncDeclEntry;

/// The 'funcdecl' section of FTDB contains information about undefined function
/// declarations.
///
/// These functions share the same [`FunctionId`] pool with entries of `funcs` section.
///
/// There are several ways to browse undefined function declarations entries:
///
/// 1. Use [`Ftdb`] instance
///     * Iterate over [`crate::Ftdb::funcdecls_iter`] iterator
///     * Filter by function's id [`crate::Ftdb::funcdecl_by_id`]
///     * Filter by function's hash [`crate::Ftdb::funcdecl_by_declhash`]
/// 2. Use [`FuncDecls`] instance
///     * Iterate over [`FuncDecls::iter`] iterator
///     * Filter by function's id [`FuncDecls::entry_by_id`]
///     * Filter by function's hash [`FuncDecls::entry_by_declhash`]
///
/// [`Ftdb`]: crate::Ftdb
///
#[derive(Debug, Clone)]
pub struct FuncDecls(Owned<ftdb>);

impl Display for FuncDecls {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<FuncDecls: {} funcdecls>", self.len())
    }
}

impl FtdbCollection<ftdb_funcdecl_entry> for FuncDecls {
    #[inline]
    unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_funcdecl_entry {
        self.as_inner_ref().get_ptr(index)
    }

    #[inline]
    fn len(&self) -> usize {
        <ftdb as FtdbCollection<ftdb_funcdecl_entry>>::len(self.as_inner_ref())
    }
}

impl_inner_handle!(FuncDecls);

impl<'s> InnerRef<'s, 's, ftdb> for FuncDecls {
    fn as_inner_ref(&'s self) -> &'s ftdb {
        self.0.as_inner_ref()
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

    /// Get function declaration by its unique identifier
    ///
    /// # Arguments
    ///
    /// * `id` - ID of a function declaration to look for
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// let decls = db.funcdecls();
    /// if let Some(entry) = decls.entry_by_id(42u64.into()) {
    ///     println!("Signature: {}", entry.signature());
    /// } else {
    ///     eprintln!("Function declaration id 42 not found");
    /// }
    /// # }
    /// ```
    ///
    pub fn entry_by_id(&self, id: FunctionId) -> Option<borrowed::FuncDeclEntry<'_>> {
        self.0.funcdecl_by_id(id.0).map(FuncDeclEntry::from)
    }

    /// Find function declaration entry by its declaration hash
    ///
    /// # Arguments
    ///
    /// * `hash` - hash of a function declaration to look for
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// let hash = "...";
    /// let decls = db.funcdecls();
    /// if let Some(entry) = decls.entry_by_declhash(hash) {
    ///     println!("Signature: {}", entry.signature());
    /// } else {
    ///     eprintln!("Function declaration id 42 not found");
    /// }
    /// # }
    /// ```
    ///
    pub fn entry_by_declhash(&self, hash: &str) -> Option<borrowed::FuncDeclEntry<'_>> {
        self.0.funcdecl_by_declhash(hash).map(FuncDeclEntry::from)
    }

    /// Iterate over all function declarations
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// let decls = db.funcdecls();
    /// for fdecl in decls.iter() {
    ///     println!("{}", fdecl.signature());
    /// }
    /// # }
    /// ```
    ///
    pub fn iter(&self) -> BorrowedIterator<'_, ftdb, borrowed::FuncDeclEntry, ftdb_funcdecl_entry> {
        BorrowedIterator::new(self.as_inner_ref())
    }

    /// List of all function declarations
    ///
    pub fn to_vec(&self) -> Vec<borrowed::FuncDeclEntry<'_>> {
        self.iter().collect()
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

        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {

            /// Returns function declaration a a string
            ///
            pub fn decl(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().decl)
            }

            /// Returns a function declaration hash
            ///
            pub fn declhash(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().declhash)
            }

            /// Id of a file in which this function declaration is found
            ///
            pub fn fid(&self) -> $crate::db::FileId {
                self.as_inner_ref().fid.into()
            }

            /// If of a function declaration
            ///
            /// Note that the pool of `FunctionId` is shared with `funcs` entries
            ///
            pub fn id(&self) -> $crate::db::FunctionId {
                self.as_inner_ref().id.into()
            }

            /// Returns true if function declaration has variadic parameters
            ///
            pub fn is_variadic(&self) -> bool {
                self.as_inner_ref().isvariadic != 0
            }

            /// Returns type of linkage
            ///
            pub fn linkage(&self) -> $crate::db::Linkage {
                self.as_inner_ref().linkage.into()
            }

            /// Absolute path to a file containing function declaration
            ///
            pub fn location(&self) -> $crate::db::Location<$ret_life> {
                $crate::utils::ptr_to_str(self.as_inner_ref().location).into()
            }

            /// Returns function name
            ///
            pub fn name(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().name)
            }

            /// Returns number of function arguments
            ///
            /// In case of variadic functions, additional arguments are not counted.
            ///
            pub fn nargs(&self) -> usize {
                self.as_inner_ref().nargs as usize
            }

            /// Number of files in which this function appears
            ///
            pub fn refcount(&self) -> usize {
                self.as_inner_ref().refcount as usize
            }

            /// Returns function's signature
            ///
            pub fn signature(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().signature)
            }

            /// Id of types used in the function signature
            ///
            /// Number of entries is equal to `nargs() + 1`.
            /// First element represent returned type. Other elements are types of
            /// function parameters.
            ///
            pub fn types(&self) -> &$ret_life [$crate::db::TypeId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().types as *const $crate::db::TypeId,
                    self.as_inner_ref().types_count
                )
            }
        }
    };
}

pub(crate) use func_decl_entry_impl;
