use crate::{
    db::{ExprType, FunctionId},
    utils::{ptr_to_slice, ptr_to_str, try_ptr_to_str, try_ptr_to_type},
};
use ftdb_sys::ftdb::{call_info, callref_data, callref_info, ftdb_func_entry};
use std::fmt::Display;

/// Details about function calls within a function
///
/// See also: [CallEntry](CallEntry)
///
pub struct Calls<'a>(&'a ftdb_func_entry);

impl<'a> Display for Calls<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<Calls: {} entries>", self.0.calls_count)
    }
}
impl<'a> From<&'a ftdb_func_entry> for Calls<'a> {
    fn from(inner: &'a ftdb_func_entry) -> Self {
        Self(inner)
    }
}

impl<'a> Calls<'a> {
    pub fn iter(&self) -> CallEntryIterator<'a> {
        CallEntryIterator { db: self.0, cur: 0 }
    }

    pub fn to_vec(&self) -> Vec<CallEntry<'a>> {
        self.iter().collect()
    }
}

impl<'a> IntoIterator for Calls<'a> {
    type Item = CallEntry<'a>;
    type IntoIter = CallEntryIterator<'a>;

    fn into_iter(self) -> Self::IntoIter {
        self.iter()
    }
}

pub struct CallEntryIterator<'a> {
    db: &'a ftdb_func_entry,
    cur: usize,
}

impl<'a> Iterator for CallEntryIterator<'a> {
    type Item = CallEntry<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur < self.db.calls_count as usize {
            unsafe {
                let id = self.db.calls.add(self.cur);
                let ci = self.db.call_info.add(self.cur);
                let cri = self.db.callrefs.add(self.cur);
                self.cur += 1;
                Some(CallEntry {
                    id: FunctionId::from(*id),
                    ci: &*ci,
                    cr: &*cri,
                })
            }
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let remaining = self.db.calls_count as usize - self.cur;
        (remaining, Some(remaining))
    }
}

impl<'a> ExactSizeIterator for CallEntryIterator<'a> {}

/// Function call details
///
/// Groups together common fields related to calls:
///   - `calls`     (accessed through [id](CallEntry::id) field)
///   - `callrefs`  (accessed through [refs](CallEntry::refs) function)
///   - `call_info` (accessed through [info](CallEntry::info) function)
///
pub struct CallEntry<'a> {
    /// Id of a called function
    pub id: FunctionId,

    /// Inner data structure, representing `call_info`
    ci: &'a call_info,

    /// Inner data structure representing one of `callrefs` entries
    cr: &'a callref_info,
}

impl<'a> CallEntry<'a> {
    pub fn info(&self) -> CallEntryInfo<'a> {
        self.ci.into()
    }

    pub fn refs(&self) -> Vec<CallEntryRefInfo<'a>> {
        self.refs_iter().collect()
    }

    pub fn refs_iter(&self) -> impl ExactSizeIterator<Item = CallEntryRefInfo<'a>> {
        ptr_to_slice(self.cr.callarg, self.cr.callarg_count)
            .iter()
            .map(|x| x.into())
    }
}

/// Represents a single `call_info` entry
pub struct CallEntryInfo<'a>(&'a call_info);

impl<'a> From<&'a call_info> for CallEntryInfo<'a> {
    fn from(inner: &'a call_info) -> Self {
        Self(inner)
    }
}

impl<'a> CallEntryInfo<'a> {
    pub fn args(&self) -> &'a [u64] {
        ptr_to_slice(self.0.args, self.0.args_count)
    }

    pub fn end(&self) -> &'a str {
        ptr_to_str(self.0.end)
    }

    pub fn expr(&self) -> &'a str {
        ptr_to_str(self.0.expr)
    }

    pub fn loc(&self) -> &'a str {
        ptr_to_str(self.0.loc)
    }

    pub fn ord(&self) -> u64 {
        self.0.ord
    }

    pub fn start(&self) -> &'a str {
        ptr_to_str(self.0.start)
    }
}

/// Represents a single `callrefs` entry
pub struct CallEntryRefInfo<'a>(&'a callref_data);

impl<'a> From<&'a callref_data> for CallEntryRefInfo<'a> {
    fn from(inner: &'a callref_data) -> Self {
        Self(inner)
    }
}

#[derive(Debug, PartialEq)]
pub enum CallRefDataId<'a> {
    Id(u64),
    IntegerLiteral(i64),
    CharacterLiteral(i32),
    FloatingLiteral(f64),
    StringLiteral(&'a str),
    None,
}

impl<'a> CallEntryRefInfo<'a> {
    pub fn id(&self) -> CallRefDataId {
        try_ptr_to_type(self.0.id)
            .map(CallRefDataId::Id)
            .or_else(|| try_ptr_to_type(self.0.integer_literal).map(CallRefDataId::IntegerLiteral))
            .or_else(|| {
                try_ptr_to_type(self.0.character_literal).map(CallRefDataId::CharacterLiteral)
            })
            .or_else(|| {
                try_ptr_to_type(self.0.floating_literal).map(CallRefDataId::FloatingLiteral)
            })
            .or_else(|| try_ptr_to_str(self.0.string_literal).map(CallRefDataId::StringLiteral))
            .unwrap_or(CallRefDataId::None)
    }

    pub fn pos(&self) -> u64 {
        self.0.pos
    }

    pub fn type_(&self) -> ExprType {
        self.0.type_.into()
    }

    /// Try to return data behind CallRefDataId::Id
    ///
    pub fn try_plain_id(&self) -> Option<u64> {
        match self.id() {
            CallRefDataId::Id(id) => Some(id),
            _ => None,
        }
    }

    /// Return data behind CallRefDataId::Id or panic
    ///
    /// This call expects CallRefDataId::Id variant
    pub fn plain_id(&self) -> u64 {
        self.try_plain_id().expect("Unexpected ref ID type!")
    }
}
