use crate::utils::{ptr_to_slice, ptr_to_str};
use ftdb_sys::ftdb::{ftdb, ftdb_unresolvedfunc_entry};
use std::fmt::Display;
use std::ops::Deref;

pub struct UnresolvedFuncs<'a>(&'a ftdb);

impl<'a> UnresolvedFuncs<'a> {
    pub fn iter(&self) -> UnresolvedFuncsIter<'a> {
        UnresolvedFuncsIter { db: self.0, cur: 0 }
    }
}

impl<'a> IntoIterator for UnresolvedFuncs<'a> {
    type Item = UnresolvedFuncEntry<'a>;
    type IntoIter = UnresolvedFuncsIter<'a>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

impl<'a> From<&'a ftdb> for UnresolvedFuncs<'a> {
    fn from(db: &'a ftdb) -> Self {
        Self(db)
    }
}

impl<'a> Display for UnresolvedFuncs<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "<UnresolvedFuncs: {} items>",
            self.0.unresolvedfuncs_count
        )
    }
}

pub struct UnresolvedFuncsIter<'a> {
    db: &'a ftdb,
    cur: usize,
}

impl<'a> Iterator for UnresolvedFuncsIter<'a> {
    type Item = UnresolvedFuncEntry<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur < (self.db.unresolvedfuncs_count - 1) as usize {
            self.cur += 1;
            let entries = ptr_to_slice(
                self.db.unresolvedfuncs,
                self.db.unresolvedfuncs_count,
            );
            let entry = &entries[self.cur];
            Some(entry.into())
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let size = self.db.unresolvedfuncs_count as usize;
        (size, Some(size))
    }
}

pub struct UnresolvedFuncEntry<'a>(&'a ftdb_unresolvedfunc_entry);

impl<'a> UnresolvedFuncEntry<'a> {
    pub fn name(&self) -> &'a str {
        ptr_to_str(self.0.name)
    }

    pub fn id(&self) -> u64 {
        self.0.id
    }
}

impl<'a> From<&'a ftdb_unresolvedfunc_entry> for UnresolvedFuncEntry<'a> {
    fn from(entry: &'a ftdb_unresolvedfunc_entry) -> Self {
        Self(entry)
    }
}

impl<'a> Deref for UnresolvedFuncEntry<'a> {
    type Target = ftdb_unresolvedfunc_entry;

    fn deref(&self) -> &Self::Target {
        self.0
    }
}

impl<'a> Display for UnresolvedFuncEntry<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "({}, {})", self.id(), self.name())
    }
}
