use ftdb_sys::ftdb::{
    ftdb, ftdb_funcdecl_entry, functionLinkage, stringRef_entryMap_search,
    ulong_entryMap_search,
};
use std::{ffi::CString, fmt::Display, ops::Deref};

use crate::utils::{ptr_to_slice, ptr_to_str, try_ptr_to_str, try_ptr_to_type};

use super::Linkage;

pub struct FuncDecls<'a>(&'a ftdb);

impl<'a> FuncDecls<'a> {
    /// Construct a vector from inner slice to ftdb_funcdecl_entry
    ///
    pub fn as_vec(&self) -> Vec<FuncDeclEntry<'a>> {
        ptr_to_slice(self.0.funcdecls, self.0.funcdecls_count)
            .iter()
            .map(|x| x.into())
            .collect()
    }

    /// Find funcdecl entry by its id
    ///
    pub fn entry_by_id(&self, id: u64) -> Option<FuncDeclEntry<'a>> {
        let node = unsafe { ulong_entryMap_search(&self.0.fdrefmap, id) };
        if node.is_null() {
            return None;
        }
        let entry = unsafe {
            let entry = (*node).entry;
            &*(entry as *const ftdb_funcdecl_entry)
        };
        Some(entry.into())
    }

    /// Find funcdecl entry by its hash
    ///
    pub fn entry_by_declhash(&self, hash: &str) -> Option<FuncDeclEntry<'a>> {
        // Rust strings are not null terminated but FFI expects NULL byte
        let hash = CString::new(hash).expect("NULL byte in the middle");
        let node = unsafe {
            stringRef_entryMap_search(
                &self.0.fdhrefmap,
                hash.as_ptr() as *const i8,
            )
        };
        if node.is_null() {
            return None;
        }
        let entry = unsafe {
            let entry = (*node).entry;
            &*(entry as *const ftdb_funcdecl_entry)
        };
        Some(entry.into())
    }

    /// Iterator over funcdecl entries
    ///
    pub fn iter(&self) -> FuncDeclsIter<'a> {
        FuncDeclsIter { db: self.0, cur: 0 }
    }
}

impl<'a> From<&'a ftdb> for FuncDecls<'a> {
    fn from(inner: &'a ftdb) -> Self {
        Self(inner)
    }
}

impl<'a> Display for FuncDecls<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<FuncDecls: {} funcdecls>", self.0.funcdecls_count)
    }
}

impl<'a> IntoIterator for FuncDecls<'a> {
    type Item = FuncDeclEntry<'a>;
    type IntoIter = FuncDeclsIter<'a>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

pub struct FuncDeclsIter<'a> {
    db: &'a ftdb,
    cur: usize,
}

impl<'a> Iterator for FuncDeclsIter<'a> {
    type Item = FuncDeclEntry<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur < (self.db.funcdecls_count as usize) {
            let entry = unsafe {
                self.db
                    .funcdecls
                    .add(self.cur)
                    .as_ref()
                    .expect("pointer should never be null")
            };
            self.cur += 1;
            // let entries =
            //     ptr_to_slice(self.db.funcdecls, self.db.funcdecls_count);
            // let entry = &entries[self.cur];

            Some(entry.into())
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let size = self.db.funcdecls_count as usize;
        (size, Some(size))
    }
}

/// libftdb ftdb funcdecl entry object"
///
pub struct FuncDeclEntry<'a>(&'a ftdb_funcdecl_entry);

impl<'a> FuncDeclEntry<'a> {
    /// Returns True if funcdecl entry is contained within a class
    ///
    pub fn has_class(&self) -> bool {
        let cls = try_ptr_to_type(self.0.__class);
        let cls_id = try_ptr_to_type(self.0.classid);
        cls.zip(cls_id).is_some()
    }

    /// ftdb funcdecl entry class value
    ///
    pub fn class(&self) -> Option<&'a str> {
        try_ptr_to_str(self.0.__class)
    }

    /// ftdb funcdecl entry classid value
    ///
    pub fn classid(&self) -> Option<u64> {
        try_ptr_to_type(self.0.classid)
    }

    /// ftdb funcdecl entry decl value
    ///
    pub fn decl(&self) -> &'a str {
        ptr_to_str(self.0.decl)
    }

    /// ftdb funcdecl entry declhash value
    ///
    pub fn declhash(&self) -> &'a str {
        ptr_to_str(self.0.declhash)
    }

    /// ftdb funcdecl entry fid value
    ///
    pub fn fid(&self) -> u64 {
        self.0.fid
    }

    /// ftdb funcdecl entry id value
    ///
    pub fn id(&self) -> u64 {
        self.0.id
    }

    /// ftdb funcdecl entry is member value
    ///
    pub fn is_member(&self) -> bool {
        match try_ptr_to_type(self.0.ismember) {
            Some(x) => x != 0,
            None => false,
        }
    }

    /// ftdb funcdecl entry template_parameters value
    ///
    pub fn is_template(&self) -> bool {
        try_ptr_to_type(self.0.istemplate)
            .map(|x| x != 0)
            .unwrap_or(false)
    }

    /// ftdb funcdecl entry is variadic value
    ///
    pub fn is_variadic(&self) -> bool {
        self.0.isvariadic != 0
    }

    /// ftdb funcdecl entry linkage value
    ///
    pub fn linkage(&self) -> Linkage {
        self.0.linkage.into()
    }

    /// ftdb funcdecl entry linkage value
    ///
    pub fn linkage_string(&self) -> &'a str {
        match self.0.linkage {
            functionLinkage::LINKAGE_NONE => "none",
            functionLinkage::LINKAGE_INTERNAL => "internal",
            functionLinkage::LINKAGE_EXTERNAL => "external",
            _ => "other",
        }
    }

    /// tdb funcdecl entry location value
    ///
    pub fn location(&self) -> &'a str {
        ptr_to_str(self.0.location)
    }

    /// ftdb funcdecl entry name value
    ///
    pub fn name(&self) -> &'a str {
        ptr_to_str(self.0.name)
    }

    /// ftdb funcdecl entry namespace value
    ///
    pub fn namespace(&self) -> Option<&'a str> {
        try_ptr_to_str(self.0.__namespace)
    }

    /// ftdb funcdecl entry nargs value
    ///
    pub fn nargs(&self) -> u64 {
        self.0.nargs
    }

    /// ftdb funcdecl entry refcount value
    ///
    pub fn refcount(&self) -> u64 {
        self.0.refcount
    }

    /// ftdb funcdecl entry signature value
    ///
    pub fn signature(&self) -> &'a str {
        ptr_to_str(self.0.signature)
    }

    /// ftdb funcdecl entry template_parameters value"
    ///
    pub fn template_parameters(&self) -> Option<&'a str> {
        try_ptr_to_str(self.0.template_parameters)
    }

    /// ftdb funcdecl entry types list
    ///
    pub fn types(&self) -> &'a [u64] {
        ptr_to_slice(self.0.types, self.0.types_count)
    }
}

impl<'a> Deref for FuncDeclEntry<'a> {
    type Target = ftdb_funcdecl_entry;

    fn deref(&self) -> &Self::Target {
        self.0
    }
}

impl<'a> From<&'a ftdb_funcdecl_entry> for FuncDeclEntry<'a> {
    fn from(entry: &'a ftdb_funcdecl_entry) -> Self {
        Self(entry)
    }
}
