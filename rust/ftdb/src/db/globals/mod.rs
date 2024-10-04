mod borrowed;
mod owned;

use super::collection::{BorrowedIterator, FtdbCollection, IntoOwnedIterator};
use super::{FtdbHandle, GlobalId, Handle, InnerRef, Owned};
use crate::db::GlobalsQuery;
use crate::macros::impl_inner_handle;
use ftdb_sys::ftdb::{ftdb, ftdb_global_entry};
use std::fmt::Display;

pub use self::borrowed::GlobalEntry;

/// The 'globals' section of FTDB contains information regarding global variables in
/// a source code.
///
/// There are several ways to navigate through global entries:
///
/// 1. Use [`Ftdb`] instance
///     * Iterate over [`crate::Ftdb::globals_iter`] iterator
///     * Filter by global's id [`crate::Ftdb::global_by_id`]
///     * Filter by global's hash [`crate::Ftdb::global_by_hash`]
///     * Filter by global's name [`crate::Ftdb::globals_by_name`]
/// 2. Use [`Globals`] instance
///     * Iterate over [`Globals::iter`] iterator
///     * Filter by global's id [`Globals::entry_by_id`]
///     * Filter by global's hash [`Globals::entry_by_hash`]
///
/// `Types` instance shares ownership of FTDB database which means that it increases
/// internal reference counters so as long as the `Types` instance exists the
/// underlying FTDB pointers won't be free'd.
///
/// Because the use of reference counting structures (such as `std::sync::Arc`) it is
/// safe to use this type in concurency scenarios. It is possible to process each type
/// in a separate worker, just make sure to use "owned" variant of structures.
///
/// # Examples
///
/// Converting "borrowed" global entries to "owned" entries:
///
/// ```
/// use ftdb::Handle;
///
/// # fn run(db: &ftdb::Ftdb, g: ftdb::GlobalEntry<'_>) {
/// let handle = db.handle();
/// let owned = g.into_owned(handle);
/// # }
/// ```
///
/// For an example on how to process globals in chunks in asynchronous environment,
/// please refer to [`Types`] documentation as it contains similar example.
///
/// [`Ftdb`]: crate::Ftdb
/// [`Types`]: crate::Types
///
#[derive(Debug, Clone)]
pub struct Globals(Owned<ftdb>);

impl Display for Globals {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<Globals: {} globals>", self.len())
    }
}

impl FtdbCollection<ftdb_global_entry> for Globals {
    #[inline]
    unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_global_entry {
        self.as_inner_ref().get_ptr(index)
    }

    #[inline]
    fn len(&self) -> usize {
        <ftdb as FtdbCollection<ftdb_global_entry>>::len(self.as_inner_ref())
    }
}

impl_inner_handle!(Globals);

impl<'s> InnerRef<'s, 's, ftdb> for Globals {
    #[inline]
    fn as_inner_ref(&'s self) -> &'s ftdb {
        self.0.as_inner_ref()
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
        BorrowedIterator::new(self.as_inner_ref())
    }
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
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
        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {

            /// List of positions in refs - indicates nested definitions
            ///
            #[inline]
            pub fn decls(&self) -> &$ret_life [u64] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().decls,
                    self.as_inner_ref().decls_count
                )
            }

            /// Global definition
            ///
            /// Consists of definition level and string
            ///
            #[inline]
            pub fn def(&self) -> $crate::GlobalDef {
                let defstring = self.defstring();
                match self.deftype() {
                    0 => $crate::GlobalDef::Declaration(defstring),
                    1 => $crate::GlobalDef::TentativeDefinition(defstring),
                    2 => $crate::GlobalDef::Definition(defstring),
                    x => $crate::GlobalDef::Unknown(defstring, x),
                }
            }

            /// Global definition string
            ///
            #[inline]
            pub fn defstring(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().def)
            }

            /// Definition level
            ///
            /// * `0` - a declaration
            /// * `1` - a tentative definition
            /// * `2` - a definition
            ///
            #[inline]
            fn deftype(&self) -> u32 {
                self.as_inner_ref().deftype
            }

            /// A global's hash value
            ///
            #[inline]
            pub fn hash(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().hash)
            }

            /// Id of a file containing the global's definition
            ///
            #[inline]
            pub fn fid(&self) -> $crate::FileId {
                self.as_inner_ref().fid.into()
            }

            /// List of referenced functions
            ///
            #[inline]
            pub fn funrefs(&self) -> &$ret_life [$crate::FunctionId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().funrefs as *const $crate::FunctionId,
                    self.as_inner_ref().funrefs_count
                )
            }

            /// List of referenced globals
            ///
            #[inline]
            pub fn globalrefs(&self) -> &$ret_life [$crate::GlobalId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().globalrefs as *const $crate::GlobalId,
                    self.as_inner_ref().globalrefs_count,
                )
            }

            /// Unique identifier of a global
            ///
            #[inline]
            pub fn id(&self) -> $crate::GlobalId {
                self.as_inner_ref().id.into()
            }

            /// Source code of the init section of a global (is applicable)
            ///
            #[inline]
            pub fn init(&self) -> Option<&$ret_life str> {
                match self.as_inner_ref().hasinit {
                    0 => None,
                    _ => Some($crate::utils::ptr_to_str(self.as_inner_ref().init)),
                }
            }

            /// Linkage of a global variable
            ///
            #[inline]
            pub fn linkage(&self) -> $crate::Linkage {
                self.as_inner_ref().linkage.into()
            }

            /// Literals used in a global's init, group by their types
            ///
            #[inline]
            pub fn literals(&self) -> $crate::Literals<$ret_life> {
                self.as_inner_ref().into()
            }

            /// Location of the definition
            ///
            #[inline]
            pub fn location(&self) -> $crate::Location<$ret_life> {
                $crate::utils::ptr_to_str(self.0.location).into()
            }

            /// List of modules with global definition
            ///
            #[inline]
            pub fn mids(&self) -> &$ret_life [$crate::ModuleId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().mids as *const $crate::ModuleId,
                    self.as_inner_ref().mids_count
                )
            }

            /// Name of a global variable
            ///
            #[inline]
            pub fn name(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().name)
            }

            /// List of referenced types
            ///
            #[inline]
            pub fn refs(&self) -> &$ret_life [$crate::TypeId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().refs as *const $crate::TypeId,
                    self.as_inner_ref().refs_count
                )
            }

            /// Id of a type of this global variable
            ///
            #[inline]
            pub fn type_id(&self) -> $crate::TypeId {
                self.as_inner_ref().type_.into()
            }
        }
    }
}

pub(crate) use global_entry_impl;
