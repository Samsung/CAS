use super::{Linkage, Literals};
use crate::utils::{ptr_to_slice, ptr_to_str};
use ftdb_sys::ftdb::{
    ftdb, ftdb_global_entry, stringRef_entryListMap_search, stringRef_entryMap_search,
    ulong_entryMap_search,
};
use std::{ffi::CString, fmt::Display};

pub struct Globals<'a>(&'a ftdb);

impl<'a> Globals<'a> {
    /// Get global by its unique identifier
    ///
    pub fn entry_by_id(&self, id: u64) -> Option<GlobalEntry<'a>> {
        let node = unsafe { ulong_entryMap_search(&self.0.grefmap, id) };
        if node.is_null() {
            return None;
        }
        unsafe {
            let entry = (*node).entry;
            let entry = &*(entry as *const ftdb_global_entry);
            Some(entry.into())
        }
    }

    /// Get global by its hash
    ///
    pub fn entry_by_hash(&self, hash: &str) -> Option<GlobalEntry<'a>> {
        // Rust strings are not null terminated but FFI expects NULL byte
        let name = CString::new(hash).expect("NULL byte in the middle");
        let node =
            unsafe { stringRef_entryMap_search(&self.0.ghrefmap, hash.as_ptr() as *const i8) };
        if node.is_null() {
            return None;
        }
        unsafe {
            let entry = (*node).entry;
            let entry = &*(entry as *const ftdb_global_entry);
            Some(entry.into())
        }
    }

    pub fn entry_by_name(&self, name: &str) -> Vec<GlobalEntry<'a>> {
        // Rust strings are not null terminated but FFI expects NULL byte
        let name = CString::new(name).expect("Null byte in the middle");
        let nodes =
            unsafe { stringRef_entryListMap_search(&self.0.gnrefmap, name.as_ptr() as *const i8) };
        if nodes.is_null() {
            return Vec::new();
        }
        unsafe {
            let nodes = *nodes;
            let entries = nodes.entry_list;
            let entries = &*(entries as *const *const ftdb_global_entry);
            let count = nodes.entry_count;
            ptr_to_slice(entries, count)
                .iter()
                .map(|x| {
                    let node = x.as_ref().unwrap();
                    node.into()
                })
                .collect()
        }
    }

    /// Iterator through global entries
    ///
    pub fn iter(&self) -> GlobalsIter<'a> {
        GlobalsIter { db: self.0, cur: 0 }
    }

    /// Checks whether globals array is empty or not
    ///
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Return number of entries
    ///
    pub fn len(&self) -> usize {
        self.0.globals_count as usize
    }
}

impl<'a> Display for Globals<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<Globals: {} globals>", self.len())
    }
}

impl<'a> From<&'a ftdb> for Globals<'a> {
    fn from(db: &'a ftdb) -> Self {
        Globals(db)
    }
}

impl<'a> IntoIterator for Globals<'a> {
    type Item = GlobalEntry<'a>;
    type IntoIter = GlobalsIter<'a>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

// impl<'a> From<Vec<GlobalEntry<'a>>> for Globals<'a> {
//     fn from(entries: Vec<GlobalEntry<'a>>) -> Self {
//         Self(entries)
//     }
// }

#[derive(Debug)]
pub struct GlobalEntry<'a>(&'a ftdb_global_entry);

impl<'a> GlobalEntry<'a> {
    //// ftdb global entry decls values
    ///
    pub fn decls(&self) -> &'a [u64] {
        ptr_to_slice(self.0.decls, self.0.decls_count)
    }

    pub fn defstring(&self) -> &'a str {
        ptr_to_str(self.0.def)
    }

    pub fn def(&self) -> GlobalDef {
        let defstring = self.defstring();
        match self.0.deftype {
            0 => GlobalDef::Declaration(defstring),
            1 => GlobalDef::TentativeDefinition(defstring),
            2 => GlobalDef::Definition(defstring),
            x => GlobalDef::Unknown(defstring, x),
        }
    }

    /// ftdb global entry hash value
    ///
    pub fn hash(&self) -> &'a str {
        ptr_to_str(self.0.hash)
    }

    /// ftdb global entry fid value
    ///
    pub fn fid(&self) -> u64 {
        self.0.fid
    }

    /// ftdb global entry funrefs values
    ///
    pub fn funrefs(&self) -> &'a [u64] {
        ptr_to_slice(self.0.funrefs, self.0.funrefs_count)
    }

    /// ftdb global entry globalrefs values
    ///
    pub fn globalrefs(&self) -> &'a [u64] {
        ptr_to_slice(self.0.globalrefs, self.0.globalrefs_count)
    }

    /// ftdb global entry id value
    ///
    pub fn id(&self) -> u64 {
        self.0.id
    }

    /// ftdb global entry init value
    ///
    pub fn init(&self) -> Option<&'a str> {
        match self.0.hasinit {
            0 => None,
            _ => Some(ptr_to_str(self.0.init)),
        }
    }

    /// ftdb global entry linkage value
    ///
    pub fn linkage(&self) -> Linkage {
        self.0.linkage.into()
    }

    /// ftdb global entry literals value
    ///
    pub fn literals(&self) -> Literals<'a> {
        self.0.into()
    }

    /// ftdb global entry location value
    ///
    pub fn location(&self) -> &'a str {
        ptr_to_str(self.0.location)
    }

    /// ftdb global entry mids values
    ///
    pub fn mids(&self) -> &'a [u64] {
        ptr_to_slice(self.0.mids, self.0.mids_count)
    }

    /// ftdb global entry name value
    ///
    pub fn name(&self) -> &'a str {
        ptr_to_str(self.0.name)
    }

    /// ftdb global entry refs values
    ///
    pub fn refs(&self) -> &'a [u64] {
        ptr_to_slice(self.0.refs, self.0.refs_count)
    }

    /// ftdb global entry type value
    ///
    pub fn type_(&self) -> u64 {
        self.0.type_
    }
}

impl<'a> From<&'a ftdb_global_entry> for GlobalEntry<'a> {
    fn from(inner: &'a ftdb_global_entry) -> Self {
        GlobalEntry(inner)
    }
}

impl<'a> Display for GlobalEntry<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "<ftdb:GlobalEntry id:{}|name:{}>",
            self.id(),
            self.name()
        )
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

pub struct GlobalsIter<'a> {
    db: &'a ftdb,
    cur: usize,
}

impl<'a> Iterator for GlobalsIter<'a> {
    type Item = GlobalEntry<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur < self.db.globals_count as usize {
            let entry = unsafe { self.db.globals.add(self.cur).as_ref() }
                .expect("Pointer must not be null");
            self.cur += 1;
            Some(entry.into())
        } else {
            None
        }
    }
}
