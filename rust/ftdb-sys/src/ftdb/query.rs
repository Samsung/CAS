use super::{ftdb_func_entry, ftdb_type_entry};
use crate::ftdb::{
    ftdb, ftdb_funcdecl_entry, ftdb_global_entry, stringRef_entryListMap_search,
    stringRef_entryMap_search, ulong_entryMap_search,
};
use std::ffi::CString;

/// Converts pointer to a slice
///
/// If data is NULL, 0-length slice is returned
///
fn ptr_to_slice<'a, T>(data: *const T, len: u64) -> &'a [T] {
    let data = match len {
        0 => std::ptr::NonNull::dangling().as_ptr(),
        _ => data,
    };
    unsafe { std::slice::from_raw_parts(data, len as usize) }
}

pub trait FuncDeclsQuery {
    fn funcdecl_by_id(&self, id: u64) -> Option<&ftdb_funcdecl_entry>;
    fn funcdecl_by_declhash<T: AsRef<str>>(&self, hash: T) -> Option<&ftdb_funcdecl_entry>;
}

pub trait FuncEntryQuery {
    fn func_by_id(&self, id: u64) -> Option<&ftdb_func_entry>;
    fn func_by_hash<T: AsRef<str>>(&self, hash: T) -> Option<&ftdb_func_entry>;
    fn funcs_by_name<T: AsRef<str>>(&self, name: T) -> impl Iterator<Item = &ftdb_func_entry>;
}

pub trait GlobalsQuery {
    fn global_by_id(&self, id: u64) -> Option<&ftdb_global_entry>;
    fn global_by_hash<T: AsRef<str>>(&self, hash: T) -> Option<&ftdb_global_entry>;
    fn globals_by_name<T: AsRef<str>>(&self, name: T) -> impl Iterator<Item = &ftdb_global_entry>;
}

pub trait TypesQuery {
    fn type_by_id(&self, id: u64) -> Option<&ftdb_type_entry>;
    fn type_by_hash<T: AsRef<str>>(&self, hash: T) -> Option<&ftdb_type_entry>;
}

impl FuncDeclsQuery for ftdb {
    fn funcdecl_by_id(&self, id: u64) -> Option<&ftdb_funcdecl_entry> {
        unsafe {
            let node = ulong_entryMap_search(&self.fdhrefmap, id);
            if node.is_null() {
                return None;
            }
            (*node).entry.cast::<ftdb_funcdecl_entry>().as_ref()
        }
    }

    fn funcdecl_by_declhash<T: AsRef<str>>(&self, hash: T) -> Option<&ftdb_funcdecl_entry> {
        // Rust strings are not null terminated but FFI expects NULL byte
        let hash = hash.as_ref();
        let hash = CString::new(hash).expect("NULL byte in the middle");
        unsafe {
            let node = stringRef_entryMap_search(&self.fdhrefmap, hash.as_ptr());
            if node.is_null() {
                return None;
            }
            (*node).entry.cast::<ftdb_funcdecl_entry>().as_ref()
        }
    }
}

impl FuncEntryQuery for ftdb {
    fn func_by_id(&self, id: u64) -> Option<&ftdb_func_entry> {
        unsafe {
            let node = ulong_entryMap_search(&self.frefmap, id);
            if node.is_null() {
                return None;
            }
            (*node).entry.cast::<ftdb_func_entry>().as_ref()
        }
    }

    fn func_by_hash<T: AsRef<str>>(&self, hash: T) -> Option<&ftdb_func_entry> {
        // Rust strings are not null terminated but FFI expects NULL byte
        let hash = hash.as_ref();
        let hash = CString::new(hash).expect("Null byte in the middle");
        unsafe {
            let node = stringRef_entryMap_search(&self.fhrefmap, hash.as_ptr().cast::<i8>());
            if node.is_null() {
                return None;
            }
            (*node).entry.cast::<ftdb_func_entry>().as_ref()
        }
    }

    fn funcs_by_name<T: AsRef<str>>(&self, name: T) -> impl Iterator<Item = &ftdb_func_entry> {
        let name = CString::new(name.as_ref()).expect("Null byte in the middle");
        let slice = unsafe {
            let nodes = stringRef_entryListMap_search(&self.fnrefmap, name.as_ptr());
            if nodes.is_null() {
                &[]
            } else {
                let nodes = *nodes;
                let count = nodes.entry_count;
                let entries = nodes
                    .entry_list
                    .cast::<*const ftdb_func_entry>()
                    .cast_const();
                ptr_to_slice(entries, count)
            }
        };
        slice
            .iter()
            .map(|x| unsafe { x.as_ref() }.expect("Null entry"))
    }
}

impl GlobalsQuery for ftdb {
    fn global_by_id(&self, id: u64) -> Option<&ftdb_global_entry> {
        unsafe {
            let node = ulong_entryMap_search(&self.grefmap, id);
            if node.is_null() {
                return None;
            }
            (*node).entry.cast::<ftdb_global_entry>().as_ref()
        }
    }

    fn global_by_hash<T>(&self, hash: T) -> Option<&ftdb_global_entry>
    where
        T: AsRef<str>,
    {
        // Rust strings are not null terminated but FFI expects NULL byte
        let hash = hash.as_ref();
        let hash = CString::new(hash).expect("NULL byte in the middle");
        unsafe {
            let node = stringRef_entryMap_search(&self.ghrefmap, hash.as_ptr());
            if node.is_null() {
                return None;
            }
            (*node).entry.cast::<ftdb_global_entry>().as_ref()
        }
    }

    fn globals_by_name<T>(&self, name: T) -> impl Iterator<Item = &ftdb_global_entry>
    where
        T: AsRef<str>,
    {
        // Rust strings are not null terminated but FFI expects NULL byte
        let name = name.as_ref();
        let name = CString::new(name).expect("Null byte in the middle");
        let slice = unsafe {
            let nodes = stringRef_entryListMap_search(&self.gnrefmap, name.as_ptr());
            if nodes.is_null() {
                &[]
            } else {
                let nodes = *nodes;
                let count = nodes.entry_count;
                let entries = nodes
                    .entry_list
                    .cast::<*const ftdb_global_entry>()
                    .cast_const();
                ptr_to_slice(entries, count)
            }
        };
        slice
            .iter()
            .map(|x| unsafe { x.as_ref() }.expect("Null entry"))
    }
}

impl TypesQuery for ftdb {
    fn type_by_id(&self, id: u64) -> Option<&ftdb_type_entry> {
        unsafe {
            let node = ulong_entryMap_search(&self.refmap, id);
            if node.is_null() {
                return None;
            }
            (*node).entry.cast::<ftdb_type_entry>().as_ref()
        }
    }

    fn type_by_hash<T: AsRef<str>>(&self, hash: T) -> Option<&ftdb_type_entry> {
        let hash = CString::new(hash.as_ref()).expect("NULL byte in the middle");
        unsafe {
            let node = stringRef_entryMap_search(&self.hrefmap, hash.as_ptr().cast::<i8>());
            if node.is_null() {
                return None;
            }
            (*node).entry.cast::<ftdb_type_entry>().as_ref()
        }
    }
}
