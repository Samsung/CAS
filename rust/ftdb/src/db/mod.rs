mod collection;
mod fops;
mod ftdb;
mod funcdecls;
mod funcs;
mod globals;
mod identifiers;
mod sync;
mod types;
mod unresolved;
pub(crate) mod utils;
mod wrappers;

pub use self::collection::*;
pub use self::fops::*;
pub use self::ftdb::*;
pub use self::funcdecls::*;
pub use self::funcs::*;
pub use self::globals::*;
pub use self::identifiers::*;
pub(crate) use self::sync::*;
pub use self::types::*;
pub use self::unresolved::*;
pub use self::wrappers::*;

// Re-import traits
pub use ftdb_sys::ftdb::query::*;

/// Access to inner FFI structure
///
pub trait InnerRef<'s, 'r, T> {
    fn as_inner_ref(&'s self) -> &'r T;
}

/// Property of being a holder of a reference to a pointer to FTDB data
///
pub trait Handle {
    fn handle(&self) -> std::sync::Arc<FtdbHandle>;
}

/// Property of being able to create a borrowed instance of itself (different but
/// related type).
///
/// # Examples
///
/// ```
/// use ftdb::{OwnedFunctionEntry, FunctionEntry, ToBorrowed};
///
/// # fn get_function_entry() -> OwnedFunctionEntry {
/// #    unimplemented!()
/// # }
/// #
/// # fn example() {
/// let owned : OwnedFunctionEntry = get_function_entry();
/// let borrowed : FunctionEntry<'_> = owned.to_borrowed();
/// # }
/// ```
///
pub trait ToBorrowed<'a> {
    type Type;

    fn to_borrowed(&'a self) -> Self::Type;
}

/// Structure holding a pointer to some inner FTDB data as well
/// as shared reference to FTDB database handle (so it is safe to)
/// use it in multithreading environment.
///
#[derive(Debug, Clone)]
pub struct Owned<T> {
    /// Pointer to inner data of FTDB database
    ///
    /// This should be a pointer returned by a call to `libftdb_c_ftdb_object()`
    ///
    pub(crate) db: std::ptr::NonNull<T>,

    /// A shared reference guarding inner data from being released
    ///
    /// This should be a handle storing pointer returned by a call to
    /// `libftdb_c_ftdb_load()`
    ///
    pub(crate) handle: std::sync::Arc<FtdbHandle>,
}

impl<T> std::ops::Deref for Owned<T> {
    type Target = T;

    fn deref(&self) -> &Self::Target {
        // Safety: Alignment and pointer being dereferencable guarantees are provided
        // by native library. All pointer are properly initialized during database load.
        // Aliasing guarantee is also met because of immutability of FTDB database.
        //
        unsafe { self.db.as_ref() }
    }
}

impl<T> Handle for Owned<T> {
    #[inline]
    fn handle(&self) -> std::sync::Arc<FtdbHandle> {
        self.handle.clone()
    }
}

// Safety: Shared reference to FTDB database handle protects inner pointers
// from being invalid
//
unsafe impl<T> Send for Owned<T> {}

// Safety: Shared reference to FTDB database handle protects inner pointers
// from being invalid
//
unsafe impl<T> Sync for Owned<T> {}

impl<'s, T> InnerRef<'s, 's, T> for Owned<T> {
    #[inline]
    fn as_inner_ref(&'s self) -> &'s T {
        unsafe { self.db.as_ref() }
    }
}
