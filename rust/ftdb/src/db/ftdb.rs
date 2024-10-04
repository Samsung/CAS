use super::{
    BorrowedIterator, Fops, FtdbHandle, FuncDecls, FunctionId, Functions, GlobalId, Globals,
    Handle, InnerRef, Location, ModuleId, Owned, TypeId, Types, UnresolvedFuncs,
};
use crate::macros::impl_inner_handle;
use crate::utils::{ptr_to_slice, ptr_to_str};
use crate::FileId;
use ftdb_sys::ftdb::query::{FuncDeclsQuery, FuncEntryQuery, GlobalsQuery, TypesQuery};
use std::ffi::CStr;
use std::fmt::Display;
use std::ptr::NonNull;
use std::sync::Arc;

/// Provides access to flattened FTDB data.
///
#[derive(Debug, Clone)]
pub struct Ftdb(Owned<ftdb_sys::ftdb::ftdb>);

impl Display for Ftdb {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let inner = self.as_inner_ref();
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
    fn as_inner_ref(&'s self) -> &'s ::ftdb_sys::ftdb::ftdb {
        self.0.as_inner_ref()
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
    /// # Examples
    ///
    /// ```
    /// # fn run(db: ftdb::Ftdb) -> ftdb::FtdbResult<()> {
    /// println!("Built from: {}", db.directory());
    /// #   Ok(())
    /// # }
    /// ```
    ///
    #[inline]
    pub fn directory(&self) -> &str {
        let x = unsafe { CStr::from_ptr(self.as_inner_ref().directory) };
        x.to_str().expect("File format is valid")
    }

    /// Get an object representing "fops" section.
    ///
    /// Refer to [`Fops`] documentation for more details.
    ///
    #[inline]
    pub fn fops(&self) -> Fops {
        Fops::new(self.0.db, self.0.handle.clone())
    }

    /// Iterate over entries from the [`Fops`]` section
    ///
    ///  # Examples
    ///
    /// ```
    /// # fn run(db: ftdb::Ftdb) {
    /// for entry in db.fops_iter() {
    ///     println!("Type: {}\tLocation: {}", entry.type_id(), entry.location());
    /// }
    /// # }
    /// ```
    ///
    pub fn fops_iter(&self) -> impl ExactSizeIterator<Item = super::FopsEntry> {
        BorrowedIterator::<
            ftdb_sys::ftdb::ftdb,
            super::FopsEntry<'_>,
            ftdb_sys::ftdb::ftdb_fops_entry,
        >::new(self.as_inner_ref())
    }

    /// Find function declaration entry by its id
    ///
    /// # Arguments
    ///
    /// * `id` - ID of a function declaration to look for
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// if let Some(entry) = db.funcdecl_by_id(42u64.into()) {
    ///     println!("Signature: {}", entry.signature());
    /// } else {
    ///     eprintln!("Function declaration id 42 not found");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn funcdecl_by_id(&self, id: FunctionId) -> Option<super::funcdecls::FuncDeclEntry<'_>> {
        self.as_inner_ref()
            .funcdecl_by_id(id.0)
            .map(super::funcdecls::FuncDeclEntry)
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
    /// if let Some(entry) = db.funcdecl_by_declhash(hash) {
    ///     println!("Signature: {}", entry.signature());
    /// } else {
    ///     eprintln!("Function declaration id 42 not found");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn funcdecl_by_declhash<T: AsRef<str>>(
        &self,
        hash: T,
    ) -> Option<super::funcdecls::FuncDeclEntry<'_>> {
        self.as_inner_ref()
            .funcdecl_by_declhash(hash.as_ref())
            .map(super::funcdecls::FuncDeclEntry)
    }

    /// Get an object representing "function declarations" section.
    ///
    /// Refer to [`FuncDecls`] documentation for more details.
    ///
    #[inline]
    pub fn funcdecls(&self) -> FuncDecls {
        FuncDecls::new(self.0.db, self.0.handle.clone())
    }

    /// Iterate over function declarations
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// for fdecl in db.funcdecls_iter() {
    ///     println!("{}", fdecl.signature());
    /// }
    /// # }
    /// ```
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
        >::new(self.as_inner_ref())
    }

    /// Get an object representing "function definitions"
    ///
    /// Refer to [`Functions`] documentation for more details.
    ///
    #[inline]
    pub fn funcs(&self) -> Functions {
        Functions::new(self.0.db, self.0.handle.clone())
    }

    /// Iterate over function entries
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// db.funcs_iter()
    ///     .filter(|f| f.name().contains("_ioctl"))
    ///     .for_each(|f| {
    ///         println!("{}", f.unpreprocessed_body());
    ///     });
    /// # }
    /// ```
    ///
    #[inline]
    pub fn funcs_iter(&self) -> impl ExactSizeIterator<Item = super::funcs::FunctionEntry<'_>> {
        BorrowedIterator::<
            '_,
            ftdb_sys::ftdb::ftdb,
            super::funcs::FunctionEntry<'_>,
            ftdb_sys::ftdb::ftdb_func_entry,
        >::new(self.as_inner_ref())
    }

    /// Find function entry by its unique id
    ///
    /// # Arguments
    ///
    /// * `id` - id of a function to look for
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb, func_id: ftdb::FunctionId) {
    /// if let Some(f) = db.func_by_id(func_id) {
    ///     println!("{func_id}: {}", f.declbody());
    /// } else {
    ///     eprintln!("Function ({func_id}) not found");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn func_by_id(&self, id: FunctionId) -> Option<super::funcs::FunctionEntry<'_>> {
        self.as_inner_ref()
            .func_by_id(id.0)
            .map(super::funcs::FunctionEntry)
    }

    /// Find function entry by its hash
    ///
    /// The hash is guaranteed to be unique by the design.
    ///
    /// # Arguments
    ///
    /// * `hash` - hash of a function to look for
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// let hash = "...";
    /// if let Some(f) = db.func_by_hash(hash) {
    ///     println!("{hash}: {}", f.declbody());
    /// } else {
    ///     eprintln!("Hash {hash} not found");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn func_by_hash<T: AsRef<str>>(&self, hash: T) -> Option<super::funcs::FunctionEntry<'_>> {
        self.as_inner_ref()
            .func_by_hash(hash)
            .map(super::funcs::FunctionEntry)
    }

    /// Find functions by specific name
    ///
    /// # Arguments
    ///
    /// * `name` - name of functions to look for
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// if let Some(f) = db.func_by_hash("copy_from_user") {
    ///     println!("// {} from {}", f.name(), f.location());
    ///     println!("{}", f.unpreprocessed_body());
    /// } else {
    ///     eprintln!("// Function copy_from_user not found");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn funcs_by_name<T: AsRef<str>>(
        &self,
        name: T,
    ) -> impl Iterator<Item = super::funcs::FunctionEntry<'_>> {
        self.as_inner_ref()
            .funcs_by_name(name)
            .map(super::funcs::FunctionEntry)
    }

    /// Get an object representing "globals" section
    ///
    /// Refer to [`Globals`] documentation for more details.
    ///
    #[inline]
    pub fn globals(&self) -> Globals {
        Globals::new(self.0.db, self.0.handle.clone())
    }

    /// Iterate over "globals" section entries
    ///
    /// # Example
    ///
    /// ```
    /// # use std::collections::HashSet;
    /// # use ftdb::TypeClass;
    /// # fn run(db: &ftdb::Ftdb) {
    /// let type_ids : HashSet<_> = db.types_iter()
    ///     .filter(|t| t.name() == Some("file_operations") && t.classid() == TypeClass::Record)
    ///     .map(|t| t.id())
    ///     .collect();
    ///
    /// db.globals_iter()
    ///     .filter(|g| type_ids.contains(&g.type_id()))
    ///     .for_each(|g| {
    ///         println!("{}: {}", g.id(), g.defstring());
    ///     });
    /// # }
    /// ```
    ///
    #[inline]
    pub fn globals_iter(&self) -> impl ExactSizeIterator<Item = super::globals::GlobalEntry<'_>> {
        BorrowedIterator::<
            '_,
            ftdb_sys::ftdb::ftdb,
            super::globals::GlobalEntry<'_>,
            ftdb_sys::ftdb::ftdb_global_entry,
        >::new(self.as_inner_ref())
    }

    /// Find global object by its unique id
    ///
    /// # Arguments
    ///
    /// * `id` - id of a global to look for
    ///
    /// # Example
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// let id = 12367u64.into();
    /// if let Some(g) = db.global_by_id(id) {
    ///     println!("// {} from {}", g.name(), g.location());
    ///     println!("{}", g.def());
    /// } else {
    ///     eprintln!("Global {id} not found");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn global_by_id(&self, id: GlobalId) -> Option<super::globals::GlobalEntry<'_>> {
        self.as_inner_ref()
            .global_by_id(id.0)
            .map(super::globals::GlobalEntry)
    }

    /// Find global object by its hash
    ///
    /// The hash is guaranteed to be unique by the design.
    ///
    /// # Arguments
    ///
    /// * `hash` - hash of a global to look for
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// let hash = "...";
    /// if let Some(g) = db.global_by_hash(hash) {
    ///     println!("// {} from {}", g.name(), g.location());
    ///     println!("{}", g.def());
    /// } else {
    ///     eprintln!("Global {hash} not found");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn global_by_hash<T: AsRef<str>>(
        &self,
        hash: T,
    ) -> Option<super::globals::GlobalEntry<'_>> {
        self.as_inner_ref()
            .global_by_hash(hash)
            .map(super::globals::GlobalEntry)
    }

    /// Filter "globals" section entries by a given global name
    ///
    /// # Arguments
    ///
    /// * `name` - name of globals to look for
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// for g in db.globals_by_name("dev_attr_type") {
    ///     println!("// {} from {}", g.id(), g.location());
    ///     println!("{}", g.def());
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn globals_by_name<T: AsRef<str>>(
        &self,
        name: T,
    ) -> impl Iterator<Item = super::globals::GlobalEntry<'_>> {
        self.as_inner_ref()
            .globals_by_name(name)
            .map(super::globals::GlobalEntry)
    }

    /// Name of a target module
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// if db.module() == "vmlinux" {
    ///     println!("This is a linux module");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn module(&self) -> &str {
        let x = unsafe { CStr::from_ptr(self.as_inner_ref().module) };
        x.to_str().expect("File format is valid")
    }

    /// A collection of module entries
    ///
    #[inline]
    pub fn modules(&self) -> Vec<Location> {
        self.modules_iter().collect()
    }

    /// Iterate through module entries
    ///
    /// A module represents an end linking product for a binary or a library.
    /// It might be an executable, .so file or maybe a kernel module (.ko).
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// for module in db.modules_iter().map(|m| m.file()) {
    ///    println!("{}", module);
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn modules_iter(&self) -> impl Iterator<Item = Location> {
        ptr_to_slice(
            self.as_inner_ref().moduleindex_table,
            self.as_inner_ref().moduleindex_table_count,
        )
        .iter()
        .map(|x| Location::from(ptr_to_str(*x)))
    }

    /// Get module entry by its ID
    ///
    /// # Arguments
    ///
    /// * `id` - id of a module
    ///
    /// # Examples
    ///
    /// ```
    /// use ftdb::ModuleId;
    ///
    /// # fn run(db: &ftdb::Ftdb) {
    /// if let Some(m) = db.module_by_id(ModuleId::from(1u64)) {
    ///    println!("Module: {m}");
    /// } else {
    ///     eprintln!("Module 1 not found");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn module_by_id(&self, id: ModuleId) -> Option<Location> {
        ptr_to_slice(
            self.as_inner_ref().moduleindex_table,
            self.as_inner_ref().moduleindex_table_count,
        )
        .get(id.0 as usize)
        .map(|x| Location::from(ptr_to_str(*x)))
    }

    /// Construct Ftdb instance from the raw parts
    ///
    /// # Arguments
    ///
    /// * `db` - Pointer to the underlying structure, retrieved from `libftdb_c_ftdb_object()`
    /// * `handle` - A shared reference to the pointer retrieved from `libftdb_c_ftdb_load()`
    ///
    pub(crate) fn new(db: NonNull<ftdb_sys::ftdb::ftdb>, handle: Arc<FtdbHandle>) -> Self {
        Self(Owned { db, handle })
    }

    /// A collection of "source" section entries
    ///
    #[inline]
    pub fn sources(&self) -> Vec<&str> {
        self.sources_iter().collect()
    }

    /// Iterate through "source" section entries
    ///
    /// Sources are iterated by the source ID order.
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// for (source_id, path) in db.sources_iter().enumerate() {
    ///     println!("{source_id:4}: {path}");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn sources_iter(&self) -> impl ExactSizeIterator<Item = &str> {
        ptr_to_slice(
            self.as_inner_ref().sourceindex_table,
            self.as_inner_ref().sourceindex_table_count,
        )
        .iter()
        .map(|x| ptr_to_str(*x))
    }

    /// Get a path for a specified file identifier
    ///
    /// Note: This query has a low cost as underlying structure provides
    /// random access to the data.
    ///
    /// # Arguments
    ///
    /// * `fid` - Unique source file identifier
    ///
    /// # Examples
    ///
    /// ```
    /// # use ftdb::FileId;
    /// # fn run(db: &ftdb::Ftdb) {
    /// let fid = FileId::from(415u64);
    /// if let Some(path) = db.source_by_id(fid) {
    ///     println!("{fid}: {path}");
    /// } else {
    ///     eprintln!("File {fid} not found");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn source_by_id(&self, fid: FileId) -> Option<&str> {
        ptr_to_slice(
            self.as_inner_ref().sourceindex_table,
            self.as_inner_ref().sourceindex_table_count,
        )
        .get(fid.0 as usize)
        .map(|x| ptr_to_str(*x))
    }

    /// Returns a codename of processed target
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// let s : Vec<&str> = db.release().split(":").collect();
    /// let codename = s[0];
    /// let hash = s[1];
    /// println!("Codename: {codename} [{hash}]");
    /// # }
    /// ```
    ///
    #[inline]
    pub fn release(&self) -> &str {
        let x = unsafe { CStr::from_ptr(self.as_inner_ref().release) };
        x.to_str().expect("File format is valid")
    }

    /// Get an object representing "types" section
    ///
    /// Refer to [`Types`] documentation for more details.
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
        >::new(self.as_inner_ref())
    }

    /// Get type object by its unique id
    ///
    /// # Arguments
    ///
    /// * `id` - id of a type to look for
    ///
    /// # Examples
    ///
    /// ```
    /// # use ftdb::TypeId;
    /// # fn run(db: &ftdb::Ftdb) {
    /// let id = TypeId::from(3466u64);
    /// if let Some(t) = db.type_by_id(id) {
    ///     if let Some(location) = t.location() {
    ///         println!("// Type {id} from {}", location);
    ///     }
    ///     if let Some(type_code) = t.def() {
    ///         println!("{}", type_code);
    ///     }
    /// } else {
    ///     eprintln!("Type {id} not found");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn type_by_id(&self, id: TypeId) -> Option<super::types::TypeEntry> {
        self.as_inner_ref()
            .type_by_id(id.0)
            .map(super::types::TypeEntry::from)
    }

    /// Get type by its hash
    ///
    /// # Arguments
    ///
    /// * `hash` - hash of a type to look for
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// let hash = "...";
    /// if let Some(t) = db.type_by_hash(hash) {
    ///     if let Some(location) = t.location() {
    ///         assert_eq!(hash, t.hash());
    ///         println!("// type {} from {}", t.hash(), location);
    ///     }
    ///     if let Some(type_code) = t.def() {
    ///         println!("{}", type_code);
    ///     }
    /// } else {
    ///     eprintln!("Hash {hash} not found");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn type_by_hash<T: AsRef<str>>(&self, hash: T) -> Option<super::types::TypeEntry> {
        self.as_inner_ref()
            .type_by_hash(hash)
            .map(super::types::TypeEntry::from)
    }

    /// Find types by a name
    ///
    /// Note that this function doesn't use any additional structures to speed up the
    /// query. Complexity is linear as every entry is checked for a name equality.
    ///
    /// # Arguments
    ///
    /// * `name` - name of types to look for
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// for t in db.types_by_name("file_operations") {
    ///     if let Some(location) = t.location() {
    ///         println!("// type {} from {}", t.id(), location);
    ///     }
    ///     if let Some(type_code) = t.def() {
    ///         println!("{}", type_code);
    ///     }
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn types_by_name<T: AsRef<str>>(
        &self,
        name: T,
    ) -> impl Iterator<Item = super::types::TypeEntry> {
        // Unfortunately there is no supporting type names map in the db
        self.types_iter().filter(move |t| t.str_() == name.as_ref())
    }

    /// Get an object representing "unresolvedfuncs" section
    ///
    /// Refer to [`UnresolvedFuncs`] documentation for more details.
    ///
    #[inline]
    pub fn unresolved_funcs(&self) -> UnresolvedFuncs {
        UnresolvedFuncs::new(self.0.db, self.0.handle.clone())
    }

    /// Iterate over entries from "unresolvedfuncs" section
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(db: &ftdb::Ftdb) {
    /// println!("List of undeclared functions:");
    /// for f in db.unresolved_funcs_iter() {
    ///     println!("{:5}: {}", f.id(), f.name());
    /// }
    /// # }
    /// ```
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
        >::new(self.as_inner_ref())
    }

    /// Database version string
    ///
    #[inline]
    pub fn version(&self) -> &str {
        let x = unsafe { CStr::from_ptr(self.as_inner_ref().version) };
        x.to_str().expect("File format is valid")
    }
}
