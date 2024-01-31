use super::ExprType;
use crate::utils::{ptr_to_slice, ptr_to_str, try_ptr_to_str, try_ptr_to_type};
use ftdb_sys::ftdb::{call_info, callref_data, callref_info, ftdb_func_entry};
use std::fmt::Display;

/// Details about function calls within a function
///
/// See also: [CallEntry](CallEntry)
///
pub struct Calls<'a>(&'a ftdb_func_entry);

impl<'a> From<&'a ftdb_func_entry> for Calls<'a> {
    fn from(inner: &'a ftdb_func_entry) -> Self {
        Self(inner)
    }
}

impl<'a> Display for Calls<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<Calls: {} entries>", self.0.calls_count)
    }
}

impl<'a> Calls<'a> {
    pub fn iter(&self) -> impl Iterator<Item = CallEntry<'a>> {
        (0..self.0.calls_count).into_iter().map(|idx| unsafe {
            let id = self.0.calls.add(idx as usize);
            let ci = self.0.call_info.add(idx as usize);
            let cri = self.0.callrefs.add(idx as usize);
            CallEntry {
                id: *id,
                ci: &*ci,
                cr: &*cri,
            }
        })
    }

    pub fn to_vec(&self) -> Vec<CallEntry<'a>> {
        self.iter().collect()
    }
}

/// Function call details
///
/// Groups together common fields related to calls:
///   - `calls`     (accessed through [id](CallEntry::id) field)
///   - `callrefs`  (accessed through [refs](CallEntry::refs) function)
///   - `call_info` (accessed through [info](CallEntry::info) function)
///
pub struct CallEntry<'a> {
    /// Id of a called function
    pub id: u64,

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
        ptr_to_slice(self.cr.callarg, self.cr.callarg_count)
            .iter()
            .map(|x| x.into())
            .collect()
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
}
