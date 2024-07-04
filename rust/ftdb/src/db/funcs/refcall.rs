#![allow(unused_variables)]
#![allow(dead_code)]

use ftdb_sys::ftdb::{call_info, callref_info, ftdb_func_entry};

pub enum RefCallType {
    MemberCall(u64, u64, u64), // fid, cid, field_index
    Call(u64),                 // fid
}

pub struct RefCalls<'a>(&'a ftdb_func_entry);

impl<'a> RefCalls<'a> {
    pub fn iter(&self) -> impl Iterator<Item = RefCallEntry<'a>> {
        (0..self.0.refcalls_count).map(|idx| {
            todo!();
        })
    }
}

pub struct RefCall<'a> {
    pub kind: RefCallType,
    rci: &'a call_info,
    rcr: &'a callref_info,
}

pub struct RefCallEntry<'a> {
    x: &'a str, // TODO: Fix
}
