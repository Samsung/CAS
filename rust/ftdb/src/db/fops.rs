use crate::utils::{ptr_to_slice, ptr_to_str};
use ftdb_sys::ftdb::{fopsKind, ftdb, ftdb_fops_entry, ftdb_fops_member_entry};
use std::fmt::Display;

#[derive(Debug)]
pub struct Fops<'a>(&'a ftdb);

impl<'a> From<&'a ftdb> for Fops<'a> {
    fn from(db: &'a ftdb) -> Self {
        Self(db)
    }
}

impl<'a> Fops<'a> {
    /// Iterator throug fops entries
    ///
    pub fn iter(&self) -> FopsIter<'a> {
        FopsIter { db: self.0, cur: 0 }
    }

    /// Checks whether fops array is empty or not
    ///
    pub fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Return number of entries
    ///
    pub fn len(&self) -> usize {
        return self.0.fops_count as usize;
    }
}

impl<'a> Display for Fops<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<Fops: {} entries>", self.0.fops_count)
    }
}

impl<'a> IntoIterator for Fops<'a> {
    type Item = FopsEntry<'a>;
    type IntoIter = FopsIter<'a>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

/// Fops entry represents one struct type object with members
/// initialized/assigned with functions.
///
/// There are three possible kinds:
/// - "global"   object is a global variable
/// - "local"    object is a local variable
/// - "function" object couldn't be determined; holds info about
///              function where assignment took place
///
#[derive(Debug)]
pub struct FopsEntry<'a>(&'a ftdb_fops_entry);

impl<'a> FopsEntry<'a> {
    ///
    ///
    pub fn is_global(&self) -> bool {
        self.0.kind == fopsKind::FOPSKIND_GLOBAL
    }

    /// Entry kind
    ///
    /// It might be a global, local or function. Check out docs of FopsEntry
    /// type for more details.
    ///
    /// It should not be used directly as both var_id and func_id
    /// validate fopsKind value.
    ///
    pub fn kind(&self) -> fopsKind {
        self.0.kind
    }

    /// ID of a struct type with function pointer members
    ///
    pub fn type_id(&self) -> u64 {
        self.0.type_id
    }

    /// ID of a variable (if applicable)
    ///
    /// ID might refer to either global or local variable, depending
    /// on where assignment took place.
    ///
    pub fn var_id(&self) -> VarId {
        match self.kind() {
            fopsKind::FOPSKIND_GLOBAL => VarId::Global(self.0.var_id),
            fopsKind::FOPSKIND_LOCAL => VarId::Local(self.0.var_id),
            fopsKind::FOPSKIND_FUNCTION => VarId::Unknown,
            _ => panic!("Unsupported case!"),
        }
    }

    /// ID of a function associated with the object
    ///
    /// This value is valid for kinds other than "global"
    ///
    pub fn func_id(&self) -> Option<u64> {
        match self.kind() {
            fopsKind::FOPSKIND_GLOBAL => None,
            fopsKind::FOPSKIND_LOCAL => Some(self.0.func_id),
            fopsKind::FOPSKIND_FUNCTION => Some(self.0.func_id),
            _ => panic!("Unsupported case!"),
        }
    }

    /// Location associated with the object
    ///
    pub fn location(&self) -> &'a str {
        ptr_to_str(self.0.location)
    }

    /// A map of function ids assigned to struct fields
    ///
    pub fn members(&self) -> FopsMembers<'a> {
        self.0.into()
    }
}

impl<'a> From<&'a ftdb_fops_entry> for FopsMembers<'a> {
    fn from(inner: &'a ftdb_fops_entry) -> Self {
        FopsMembers(inner)
    }
}

impl<'a> From<&'a ftdb_fops_entry> for FopsEntry<'a> {
    fn from(inner: &'a ftdb_fops_entry) -> Self {
        FopsEntry(inner)
    }
}

impl<'a> Display for FopsEntry<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<ftdb:FopsEntry ...>")
    }
}

pub struct FopsMembers<'a>(&'a ftdb_fops_entry);

impl<'a> FopsMembers<'a> {
    pub fn entry_by_member_idx(&self, member_idx: usize) -> Option<FopsMemberEntry<'a>> {
        let entry = self
            .iter()
            .filter(|entry| entry.member_id() == member_idx)
            .next();
        entry
    }

    /// Iterate through FopsMember objects
    ///
    ///
    pub fn iter(&self) -> FopsMemberEntryIter<'a> {
        FopsMemberEntryIter { db: self.0, cur: 0 }
    }
}

impl<'a> IntoIterator for FopsMembers<'a> {
    type Item = FopsMemberEntry<'a>;
    type IntoIter = FopsMemberEntryIter<'a>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

pub struct FopsMemberEntry<'a>(&'a ftdb_fops_member_entry);

impl<'a> FopsMemberEntry<'a> {
    /// Index of a field in a structure
    ///
    pub fn member_id(&self) -> usize {
        self.0.member_id as usize
    }

    /// Functions assigned to a given field.
    ///
    /// In most cases the returned list contains a single entry only.
    /// However in a scenario where conditional statements (if/else, etc)
    /// are used to determine an assignment, this list might contain
    /// more entries.
    ///
    pub fn func_ids(&self) -> &'a [u64] {
        ptr_to_slice(self.0.func_ids, self.0.func_ids_count)
    }
}

impl<'a> From<&'a ftdb_fops_member_entry> for FopsMemberEntry<'a> {
    fn from(inner: &'a ftdb_fops_member_entry) -> Self {
        FopsMemberEntry(inner)
    }
}

pub struct FopsMemberEntryIter<'a> {
    db: &'a ftdb_fops_entry,
    cur: usize,
}

impl<'a> Iterator for FopsMemberEntryIter<'a> {
    type Item = FopsMemberEntry<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur < self.db.members_count as usize {
            let entry = unsafe { self.db.members.add(self.cur).as_ref() }
                .expect("Pointer must not be null");
            self.cur += 1;
            Some(entry.into())
        } else {
            None
        }
    }
}

pub struct FopsIter<'a> {
    db: &'a ftdb,
    cur: usize,
}

impl<'a> Iterator for FopsIter<'a> {
    type Item = FopsEntry<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur < self.db.fops_count as usize {
            let entry =
                unsafe { self.db.fops.add(self.cur).as_ref() }.expect("Pointer must not be null");
            self.cur += 1;
            Some(entry.into())
        } else {
            None
        }
    }
}

pub enum VarId {
    Global(u64),
    Local(u64),
    Unknown,
}
