use crate::utils::{ptr_to_slice, ptr_to_str};
use ftdb_sys::ftdb::{asm_info, ftdb_func_entry};
use std::fmt::Display;

pub struct Asms<'a>(pub(crate) &'a ftdb_func_entry);

impl<'a> From<&'a ftdb_func_entry> for Asms<'a> {
    fn from(inner: &'a ftdb_func_entry) -> Self {
        Self(inner)
    }
}

impl<'a> Asms<'a> {
    /// Return iterator over asm entries
    ///
    pub fn iter(&self) -> impl ExactSizeIterator<Item = AsmEntry<'a>> {
        ptr_to_slice(self.0.asms, self.0.asms_count)
            .iter()
            .map(|x| x.into())
    }

    /// Return vector containing asm entries
    ///
    pub fn to_vec(&self) -> Vec<AsmEntry<'a>> {
        self.iter().collect()
    }
}

impl<'a> Display for Asms<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        for line in self.iter() {
            line.fmt(f)?;
            writeln!(f)?;
        }
        Ok(())
    }
}

pub struct AsmEntry<'a>(&'a asm_info);

impl<'a> From<&'a asm_info> for AsmEntry<'a> {
    fn from(inner: &'a asm_info) -> Self {
        Self(inner)
    }
}

impl<'a> AsmEntry<'a> {
    /// ???
    ///
    pub fn csid(&self) -> i64 {
        self.0.csid
    }

    /// Line of code in asm
    ///
    pub fn str_(&self) -> &'a str {
        ptr_to_str(self.0.str_)
    }
}

impl<'a> Display for AsmEntry<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.str_())
    }
}
