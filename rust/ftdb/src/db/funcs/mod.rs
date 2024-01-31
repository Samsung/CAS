pub mod asms;
pub mod deref;
pub mod function;
pub mod globalref;
pub mod ifs;
pub mod refcall;
pub mod taint;
use std::ffi::CString;

pub use self::function::FunctionEntry;
use crate::utils::ptr_to_slice;
use ftdb_sys::ftdb::{
    ftdb, ftdb_func_entry, stringRef_entryListMap_search, stringRef_entryMap_search,
    ulong_entryMap_search,
};

pub struct Functions<'a>(&'a ftdb);

impl<'a> From<&'a ftdb> for Functions<'a> {
    fn from(db: &'a ftdb) -> Self {
        Functions(db)
    }
}

impl<'a> IntoIterator for Functions<'a> {
    type Item = FunctionEntry<'a>;
    type IntoIter = FunctionsIter<'a>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

impl<'a> Functions<'a> {
    /// Get function by its unique identifier
    ///
    pub fn get_by_id(&self, id: u64) -> Option<FunctionEntry<'a>> {
        let node = unsafe { ulong_entryMap_search(&self.0.frefmap, id) };
        if node.is_null() {
            return None;
        }
        let entry = unsafe {
            let entry = (*node).entry;
            &*(entry as *const ftdb_func_entry)
        };
        Some(entry.into())
    }

    /// Get function by its hash
    ///
    pub fn get_by_hash(&self, hash: &str) -> Option<FunctionEntry<'a>> {
        // Rust strings are not null terminated but FFI expects NULL byte
        let name = CString::new(hash).expect("Null byte in the middle");
        let node =
            unsafe { stringRef_entryMap_search(&self.0.fhrefmap, hash.as_ptr() as *const i8) };
        if node.is_null() {
            return None;
        }
        let entry = unsafe {
            let entry = (*node).entry;
            &*(entry as *const ftdb_func_entry)
        };
        Some(entry.into())
    }

    /// Get functions by a name
    ///
    ///
    pub fn get_by_name(&self, name: &str) -> Vec<FunctionEntry<'a>> {
        // Rust strings are not null terminated but FFI expects NULL byte
        let name = CString::new(name).expect("Null byte in the middle");
        let nodes =
            unsafe { stringRef_entryListMap_search(&self.0.fnrefmap, name.as_ptr() as *const i8) };
        if nodes.is_null() {
            return Vec::new();
        }
        unsafe {
            let nodes = *nodes;
            let entries = nodes.entry_list;
            let entries = &*(entries as *const *const ftdb_func_entry);
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

    /// Iterator through function entries
    ///
    pub fn iter(&self) -> FunctionsIter<'a> {
        FunctionsIter { db: self.0, cur: 0 }
    }
}

pub struct FunctionsIter<'a> {
    db: &'a ftdb,
    cur: usize,
}

impl<'a> Iterator for FunctionsIter<'a> {
    type Item = FunctionEntry<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur < self.db.funcs_count as usize {
            let entry =
                unsafe { self.db.funcs.add(self.cur).as_ref() }.expect("Pointer must not be null");
            self.cur += 1;
            Some(entry.into())
        } else {
            None
        }
    }
}
