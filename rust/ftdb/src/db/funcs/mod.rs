use super::{
    collection::{BorrowedIterator, FtdbCollection, IntoOwnedIterator},
    FtdbHandle, Handle, InnerRef, Owned,
};
use crate::{macros::impl_inner_handle, FunctionId};
use ftdb_sys::ftdb::{ftdb, ftdb_func_entry, query::FuncEntryQuery};
use std::{ptr::NonNull, sync::Arc};

mod borrowed;
mod calls;
mod deref;
mod entry;
mod expr;
mod extensions;
mod globalref;
mod ifs;
mod location;
mod owned;
mod switches;
mod taint;

pub use self::calls::*;
pub use self::deref::*;
pub use self::entry::*;
pub use self::expr::*;
pub use self::extensions::DfsFunctionExt;
pub use self::globalref::*;
pub use self::ifs::*;
pub use self::location::*;
pub use self::switches::*;
pub use self::taint::*;

pub use self::borrowed::FunctionEntry;
pub use self::owned::FunctionEntry as OwnedFunctionEntry;

/// The 'funcs' section of FTDB contains information about function definitions
/// in a source code.
///
/// Entries of this section share the same [`FunctionId`] pool with entries of
/// `funcdecl` section.
///
/// There are several ways to browse function definitions:
///
/// 1. Use [`Ftdb`] instance
///     * Iterate over [`crate::Ftdb::funcs_iter`] iterator
///     * Filter by function's id [`crate::Ftdb::func_by_id`]
///     * Filter by function's hash [`crate::Ftdb::func_by_hash`]
///     * Filter by function's name [`crate::Ftdb::funcs_by_name`]
/// 2. Use [`Functions`] instance
///     * Iterate over [`Functions::iter`] iterator
///     * Filter by function's id [`Functions::entry_by_id`]
///     * Filter by function's hash [`Functions::entry_by_hash`]
///     * Filter by function's name [`Functions::entry_by_name`]
///
/// `Functions` instance shares ownership of FTDB database which means that it
/// increases internal reference counters so as long as the `Functions` instance
/// exists the underlying pointers won't be free'd.
///
/// Because the use of reference couting structures (such as `std::sync::Arc`)
/// it is safe to use this type in concurrency scenarios. It is possible to
/// process eafch function in a separate worker, just make sure to use "owned"
/// variant of entries.
///
/// # Examples
///
/// Converting "borrowed" function entries to "owned" entries:
///
/// ```
/// use ftdb::Handle;
///
/// # fn run(db: &ftdb::Ftdb, borrowed: ftdb::FunctionEntry<'_>) {
/// let handle = db.handle();
/// let owned = borrowed.into_owned(handle);
/// # }
/// ```
///
/// Getting "borrowed" function entries from "owned":
///
/// ```
/// use ftdb::ToBorrowed;
///
/// # fn run(db: &ftdb::Ftdb, owned: ftdb::OwnedFunctionEntry) {
/// let borrowed = owned.to_borrowed();
/// # }
///
/// ```
///
/// For an example of asynchronous processing of functions consider visting
/// [`Types`] documentation
///
/// Print functions sharing the same global as function `soft_connect_store`
///
/// ```
/// use ftdb::{GlobalId, FunctionId};
/// use std::collections::{HashSet, HashMap};
///
/// # fn run(db: &ftdb::Ftdb) {
/// // Prepare reverse map: global -> functions
/// let mut globals_ref_map : HashMap<GlobalId, HashSet<FunctionId>> = HashMap::new();
/// for func in db.funcs_iter() {
///     for global_id in func.globalrefs() {
///         let mut entry = globals_ref_map.entry(*global_id).or_default();
///         entry.insert(func.id());
///     }
/// }
///
/// for orig_func in db.funcs_by_name("soft_connect_store") {
///     println!("Function: {}: {}", orig_func.id(), orig_func.name());
///     for global_id in orig_func.globalrefs() {
///         let g = db.global_by_id(*global_id).unwrap();
///         println!("  Global: {}: {}", g.id(), g.name());
///         // unwrap because all globals were processed
///         for func_id in globals_ref_map.get(global_id).unwrap().into_iter() {
///             if *func_id != orig_func.id() {
///                 let func = db.func_by_id(*func_id).unwrap();
///                 println!("    {}: {}", func.id(), func.name());
///             }
///         }
///     }
/// }
///
/// # }
/// ```
///
/// [`Ftdb`]: crate::Ftdb
/// [`Types`]: crate::Types
///
#[derive(Debug, Clone)]
pub struct Functions(Owned<ftdb>);

impl FtdbCollection<ftdb_func_entry> for Functions {
    #[inline]
    unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_func_entry {
        self.as_inner_ref().get_ptr(index)
    }

    #[inline]
    fn len(&self) -> usize {
        <ftdb as FtdbCollection<ftdb_func_entry>>::len(self.as_inner_ref())
    }
}

impl_inner_handle!(Functions);

impl<'s> InnerRef<'s, 's, ftdb> for Functions {
    #[inline]
    fn as_inner_ref(&'s self) -> &'s ftdb {
        self.0.as_inner_ref()
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
    pub fn entry_by_id(&self, id: FunctionId) -> Option<FunctionEntry<'_>> {
        self.0.func_by_id(id.0).map(FunctionEntry::from)
    }

    /// Get function by its hash
    ///
    #[inline]
    pub fn entry_by_hash(&self, hash: &str) -> Option<FunctionEntry> {
        self.0.func_by_hash(hash).map(FunctionEntry::from)
    }

    /// Get functions by a name
    ///
    #[inline]
    pub fn entry_by_name<'p>(
        &'p self,
        name: &'p str,
    ) -> impl Iterator<Item = FunctionEntry<'p>> + 'p {
        self.0.funcs_by_name(name).map(FunctionEntry::from)
    }

    /// Iterate through function definition entries
    ///
    #[inline]
    pub fn iter(&self) -> BorrowedIterator<'_, Functions, FunctionEntry<'_>, ftdb_func_entry> {
        BorrowedIterator::new(self)
    }
}
