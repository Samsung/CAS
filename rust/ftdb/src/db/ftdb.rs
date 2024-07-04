use super::{
    BorrowedIterator, Fops, FtdbHandle, FuncDecls, FunctionId, Functions, GlobalId, Globals,
    Handle, InnerRef, Owned, TypeId, Types, UnresolvedFuncs,
};
use crate::macros::impl_inner_handle;
use crate::utils::{ptr_to_slice, ptr_to_str};
use ftdb_sys::ftdb::query::{FuncEntryQuery, GlobalsQuery, TypesQuery};
use std::ffi::CStr;
use std::fmt::Display;
use std::ptr::NonNull;
use std::sync::Arc;

/// Provides access to flattened FTDB data.
///
///
#[derive(Clone)]
pub struct Ftdb(Owned<ftdb_sys::ftdb::ftdb>);

impl Display for Ftdb {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let inner = self.inner_ref();
        write!(
            f,
            "<ftdb: funcs:{}|types:{}|globals:{}|sources:{}>",
            inner.funcs_count,
            inner.types_count,
            inner.globals_count,
            inner.sourceindex_table_count
        )
    }
}

impl<'s> InnerRef<'s, 's, ::ftdb_sys::ftdb::ftdb> for Ftdb {
    #[inline(always)]
    fn inner_ref(&'s self) -> &'s ::ftdb_sys::ftdb::ftdb {
        self.0.inner_ref()
    }
}

impl_inner_handle!(Ftdb);

pub type FopsIterator<'a> = BorrowedIterator<
    'a,
    ftdb_sys::ftdb::ftdb,
    super::FopsEntry<'a>,
    ftdb_sys::ftdb::ftdb_fops_entry,
>;

impl Ftdb {
    /// Directory from which the build has been started
    ///
    #[inline]
    pub fn directory(&self) -> &str {
        let x = unsafe { CStr::from_ptr(self.inner_ref().directory) };
        x.to_str().expect("File format is valid")
    }

    /// ftdb fops object
    ///
    #[inline]
    pub fn fops(&self) -> Fops {
        Fops::new(self.0.db, self.0.handle.clone())
    }

    /// Iterate over fops entries
    ///
    pub fn fops_iter(&self) -> impl ExactSizeIterator<Item = super::FopsEntry> {
        BorrowedIterator::<
            ftdb_sys::ftdb::ftdb,
            super::FopsEntry<'_>,
            ftdb_sys::ftdb::ftdb_fops_entry,
        >::new(self.inner_ref())
    }

    /// Function declarations in a source code
    ///
    #[inline]
    pub fn funcdecls(&self) -> FuncDecls {
        FuncDecls::new(self.0.db, self.0.handle.clone())
    }

    ///
    ///
    #[inline]
    pub fn funcdecls_iter(
        &self,
    ) -> impl ExactSizeIterator<Item = super::funcdecls::FuncDeclEntry<'_>> {
        BorrowedIterator::<
            '_,
            ftdb_sys::ftdb::ftdb,
            super::funcdecls::FuncDeclEntry<'_>,
            ftdb_sys::ftdb::ftdb_funcdecl_entry,
        >::new(self.inner_ref())
    }

    /// Function definitions in a source code
    ///
    #[inline]
    pub fn funcs(&self) -> Functions {
        Functions::new(self.0.db, self.0.handle.clone())
    }

    /// Iterator through function entries
    ///
    #[inline]
    pub fn funcs_iter(&self) -> impl ExactSizeIterator<Item = super::funcs::FunctionEntry<'_>> {
        BorrowedIterator::<
            '_,
            ftdb_sys::ftdb::ftdb,
            super::funcs::FunctionEntry<'_>,
            ftdb_sys::ftdb::ftdb_func_entry,
        >::new(self.inner_ref())
    }

    /// Find function by its unique id
    ///
    #[inline]
    pub fn func_by_id(&self, id: FunctionId) -> Option<super::funcs::FunctionEntry<'_>> {
        self.inner_ref()
            .func_by_id(id.0)
            .map(super::funcs::FunctionEntry)
    }

    /// Find function by its hash
    ///
    #[inline]
    pub fn func_by_hash<T: AsRef<str>>(&self, hash: T) -> Option<super::funcs::FunctionEntry<'_>> {
        self.inner_ref()
            .func_by_hash(hash)
            .map(super::funcs::FunctionEntry)
    }

    /// Find functions by specific name
    ///
    #[inline]
    pub fn funcs_by_name<T: AsRef<str>>(
        &self,
        name: T,
    ) -> impl Iterator<Item = super::funcs::FunctionEntry<'_>> {
        self.inner_ref()
            .funcs_by_name(name)
            .map(super::funcs::FunctionEntry)
    }

    /// ftdb globals object
    ///
    #[inline]
    pub fn globals(&self) -> Globals {
        Globals::new(self.0.db, self.0.handle.clone())
    }

    /// Iterate through global entries
    ///
    #[inline]
    pub fn globals_iter(&self) -> impl ExactSizeIterator<Item = super::globals::GlobalEntry<'_>> {
        BorrowedIterator::<
            '_,
            ftdb_sys::ftdb::ftdb,
            super::globals::GlobalEntry<'_>,
            ftdb_sys::ftdb::ftdb_global_entry,
        >::new(self.inner_ref())
    }

    /// Find global by its unique id
    ///
    #[inline]
    pub fn global_by_id(&self, id: GlobalId) -> Option<super::globals::GlobalEntry<'_>> {
        self.inner_ref()
            .global_by_id(id.0)
            .map(super::globals::GlobalEntry)
    }

    /// Find global by its hash
    ///
    #[inline]
    pub fn global_by_hash<T: AsRef<str>>(
        &self,
        hash: T,
    ) -> Option<super::globals::GlobalEntry<'_>> {
        self.inner_ref()
            .global_by_hash(hash)
            .map(super::globals::GlobalEntry)
    }

    /// Filter globals by a global name
    ///
    #[inline]
    pub fn globals_by_name<T: AsRef<str>>(
        &self,
        name: T,
    ) -> impl Iterator<Item = super::globals::GlobalEntry<'_>> {
        self.inner_ref()
            .globals_by_name(name)
            .map(super::globals::GlobalEntry)
    }

    /// ftdb object module
    ///
    #[inline]
    pub fn module(&self) -> &str {
        let x = unsafe { CStr::from_ptr(self.inner_ref().module) };
        x.to_str().expect("File format is valid")
    }

    /// List of module names
    ///
    #[inline]
    pub fn modules(&self) -> Vec<&str> {
        self.modules_iter().collect()
    }

    /// Iterate through module names
    ///
    #[inline]
    pub fn modules_iter(&self) -> impl Iterator<Item = &str> {
        ptr_to_slice(
            self.inner_ref().moduleindex_table,
            self.inner_ref().moduleindex_table_count,
        )
        .iter()
        .map(|x| ptr_to_str(*x))
    }

    pub(crate) fn new(db: NonNull<ftdb_sys::ftdb::ftdb>, handle: Arc<FtdbHandle>) -> Self {
        Self(Owned { db, handle })
    }

    /// ftdb sources object
    ///
    #[inline]
    pub fn sources(&self) -> Vec<&str> {
        self.sources_iter().collect()
    }

    /// Iterate through source entries
    ///
    #[inline]
    pub fn sources_iter(&self) -> impl ExactSizeIterator<Item = &str> {
        ptr_to_slice(
            self.inner_ref().sourceindex_table,
            self.inner_ref().sourceindex_table_count,
        )
        .iter()
        .map(|x| ptr_to_str(*x))
    }

    /// ftdb modules object
    ///
    #[inline]
    pub fn release(&self) -> &str {
        let x = unsafe { CStr::from_ptr(self.inner_ref().release) };
        x.to_str().expect("File format is valid")
    }

    /// ftdb types object
    ///
    #[inline]
    pub fn types(&self) -> Types {
        Types::new(self.0.db, self.0.handle.clone())
    }

    /// Iterate through type entries
    ///
    #[inline]
    pub fn types_iter(&self) -> impl ExactSizeIterator<Item = super::types::TypeEntry<'_>> {
        BorrowedIterator::<
            '_,
            ftdb_sys::ftdb::ftdb,
            super::types::TypeEntry<'_>,
            ftdb_sys::ftdb::ftdb_type_entry,
        >::new(self.inner_ref())
    }

    #[inline]
    pub fn type_by_id(&self, id: TypeId) -> Option<super::types::TypeEntry> {
        self.inner_ref()
            .type_by_id(id.0)
            .map(super::types::TypeEntry::from)
    }

    #[inline]
    pub fn type_by_hash<T: AsRef<str>>(&self, hash: T) -> Option<super::types::TypeEntry> {
        self.inner_ref()
            .type_by_hash(hash)
            .map(super::types::TypeEntry::from)
    }

    #[inline]
    pub fn types_by_name<'a, T: AsRef<str> + 'a>(
        &'a self,
        name: T,
    ) -> impl Iterator<Item = super::types::TypeEntry> {
        // Unfortunately there is no supporting type names map in the db
        self.types_iter().filter(move |t| t.str_() == name.as_ref())
    }

    /// ftdb unresolvedfuncs object
    ///
    #[inline]
    pub fn unresolved_funcs(&self) -> UnresolvedFuncs {
        UnresolvedFuncs::new(self.0.db, self.0.handle.clone())
    }

    /// Iterate over unresolved entries
    ///
    #[inline]
    pub fn unresolved_funcs_iter(
        &self,
    ) -> impl ExactSizeIterator<Item = super::unresolved::UnresolvedFuncEntry<'_>> {
        BorrowedIterator::<
            '_,
            ftdb_sys::ftdb::ftdb,
            super::unresolved::UnresolvedFuncEntry<'_>,
            ftdb_sys::ftdb::ftdb_unresolvedfunc_entry,
        >::new(self.inner_ref())
    }

    /// ftdb object version
    ///
    #[inline]
    pub fn version(&self) -> &str {
        let x = unsafe { CStr::from_ptr(self.inner_ref().version) };
        x.to_str().expect("File format is valid")
    }
}
