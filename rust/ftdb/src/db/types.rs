use super::TypeClass;
use crate::utils::{
    ptr_to_slice, ptr_to_str, ptr_to_vec_str, try_ptr_to_slice, try_ptr_to_str, try_ptr_to_type,
};
use ftdb_sys::ftdb::{ftdb, ftdb_type_entry, stringRef_entryMap_search, ulong_entryMap_search};
use std::{ffi::CString, fmt::Display};

pub struct Types<'a>(&'a ftdb);

impl<'a> Display for Types<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<Types: {} types>", self.len())
    }
}

impl<'a> Types<'a> {
    /// Find type by its unique identifier
    ///
    pub fn entry_by_id(&self, id: u64) -> Option<TypeEntry<'a>> {
        let node = unsafe { ulong_entryMap_search(&self.0.refmap, id) };
        if node.is_null() {
            return None;
        }
        unsafe {
            let entry = (*node).entry;
            let entry = &*(entry as *const ftdb_type_entry);
            Some(entry.into())
        }
    }

    /// Find type by its hash
    pub fn entry_by_hash(&self, hash: &str) -> Option<TypeEntry<'a>> {
        let name = CString::new(hash).expect("NULL byte in the middle");
        let node =
            unsafe { stringRef_entryMap_search(&self.0.hrefmap, hash.as_ptr() as *const i8) };
        if node.is_null() {
            return None;
        }
        unsafe {
            let entry = (*node).entry;
            let entry = &*(entry as *const ftdb_type_entry);
            Some(entry.into())
        }
    }

    /// Iterator through type entries
    ///
    pub fn iter(&self) -> TypesIter<'a> {
        TypesIter { db: self.0, cur: 0 }
    }

    /// Checks whether type array is empty or not
    ///
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Return number of entries
    ///
    pub fn len(&self) -> usize {
        self.0.types_count as usize
    }
}

impl<'a> From<&'a ftdb> for Types<'a> {
    fn from(db: &'a ftdb) -> Self {
        Types(db)
    }
}

impl<'a> IntoIterator for Types<'a> {
    type Item = TypeEntry<'a>;
    type IntoIter = TypesIter<'a>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

pub struct TypesIter<'a> {
    db: &'a ftdb,
    cur: usize,
}

impl<'a> Iterator for TypesIter<'a> {
    type Item = TypeEntry<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur < self.db.types_count as usize {
            let entry =
                unsafe { self.db.types.add(self.cur).as_ref() }.expect("Pointer must not be null");
            self.cur += 1;
            Some(entry.into())
        } else {
            None
        }
    }
}

#[derive(Debug)]
pub struct TypeEntry<'a>(&'a ftdb_sys::ftdb::ftdb_type_entry);

impl<'a> From<&'a ftdb_sys::ftdb::ftdb_type_entry> for TypeEntry<'a> {
    fn from(t: &'a ftdb_sys::ftdb::ftdb_type_entry) -> Self {
        Self(t)
    }
}

impl<'a> Display for TypeEntry<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "id:{}|class:{}", self.id(), self.classname())
    }
}

impl<'a> TypeEntry<'a> {
    pub fn attrnum(&self) -> usize {
        unsafe { self.0.attrnum.as_ref().map(|x| *x as usize) }.unwrap_or(0)
    }

    pub fn attrrefs(&self) -> &'a [u64] {
        ptr_to_slice(self.0.attrrefs, self.0.attrrefs_count)
    }

    pub fn bitfields(&self) -> Vec<Bitfield> {
        ptr_to_slice(self.0.bitfields, self.0.bitfields_count)
            .iter()
            .map(|x| x.into())
            .collect()
    }

    pub fn classname(&self) -> &'a str {
        ptr_to_str(self.0.class_name)
    }

    pub fn classid(&self) -> TypeClass {
        self.0.__class.into()
    }

    pub fn classnum(&self) -> u32 {
        self.0.__class as u32
    }

    pub fn def(&self) -> Option<&'a str> {
        try_ptr_to_str(self.0.def)
    }

    pub fn defhead(&self) -> Option<&'a str> {
        try_ptr_to_str(self.0.defhead)
    }

    pub fn enumvalues(&self) -> Option<&'a [i64]> {
        try_ptr_to_slice(self.0.enumvalues, self.0.enumvalues_count)
    }

    pub fn fid(&self) -> u64 {
        self.0.fid
    }

    pub fn funrefs(&self) -> &'a [u64] {
        ptr_to_slice(self.0.funrefs, self.0.funrefs_count)
    }

    pub fn globalrefs(&self) -> &'a [u64] {
        ptr_to_slice(self.0.globalrefs, self.0.globalrefs_count)
    }

    pub fn hash(&self) -> &'a str {
        ptr_to_str(self.0.hash)
    }

    pub fn id(&self) -> u64 {
        self.0.id
    }

    pub fn identifiers(&self) -> Vec<&'a str> {
        ptr_to_vec_str(self.0.identifiers, self.0.identifiers_count)
    }

    pub fn is_dependent(&self) -> bool {
        unsafe { self.0.isdependent.as_ref() }
            .map(|x| *x != 0)
            .unwrap_or(false)
    }

    pub fn is_implicit(&self) -> bool {
        unsafe { self.0.isimplicit.as_ref() }
            .map(|x| *x != 0)
            .unwrap_or(false)
    }

    pub fn is_union(&self) -> bool {
        unsafe { self.0.isunion.as_ref() }
            .map(|x| *x != 0)
            .unwrap_or(false)
    }

    pub fn is_variadic(&self) -> bool {
        unsafe { self.0.isvariadic.as_ref() }
            .map(|x| *x != 0)
            .unwrap_or(false)
    }

    pub fn location(&self) -> Option<&'a str> {
        try_ptr_to_str(self.0.location)
    }

    pub fn memberoffsets(&self) -> &'a [u64] {
        ptr_to_slice(self.0.memberoffsets, self.0.memberoffsets_count)
    }

    pub fn name(&self) -> Option<&'a str> {
        try_ptr_to_str(self.0.name)
    }

    pub fn outerfn(&self) -> Option<OuterFn> {
        let name = try_ptr_to_str(self.0.outerfn);
        let id = try_ptr_to_type(self.0.outerfnid);

        name.zip(id).map(|(name, id)| OuterFn { name, id })
    }

    pub fn qualifiers(&self) -> &'a str {
        ptr_to_str(self.0.qualifiers)
    }

    pub fn refcount(&self) -> usize {
        self.0.refcount as usize
    }

    pub fn refnames(&self) -> Vec<&'a str> {
        ptr_to_vec_str(self.0.refnames, self.0.refnames_count)
    }

    pub fn refs(&self) -> &'a [u64] {
        ptr_to_slice(self.0.refs, self.0.refs_count)
    }

    pub fn size(&self) -> usize {
        self.0.size as usize
    }

    pub fn str_(&self) -> &'a str {
        ptr_to_str(self.0.str_)
    }

    pub fn useddef(&self) -> Vec<&'a str> {
        ptr_to_vec_str(self.0.useddef, self.0.useddef_count)
    }

    pub fn usedrefs(&self) -> &'a [i64] {
        ptr_to_slice(self.0.usedrefs, self.0.usedrefs_count)
    }
}

#[derive(Debug, PartialEq, Eq)]
pub struct Bitfield {
    pub index: u64,
    pub width: u64,
}

impl Bitfield {
    pub fn new(index: u64, width: u64) -> Self {
        Bitfield { index, width }
    }
}

impl From<&::ftdb_sys::ftdb::bitfield> for Bitfield {
    fn from(src: &::ftdb_sys::ftdb::bitfield) -> Self {
        Self {
            index: src.index,
            width: src.bitwidth,
        }
    }
}

impl Display for Bitfield {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "({}: {})", self.index, self.width)
    }
}

#[derive(Debug, PartialEq, Eq)]
pub struct OuterFn<'a> {
    pub name: &'a str,
    pub id: u64,
}

impl<'a> OuterFn<'a> {
    pub fn new(id: u64, name: &'a str) -> Self {
        OuterFn { name, id }
    }
}
