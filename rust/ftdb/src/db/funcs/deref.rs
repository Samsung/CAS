use crate::{
    db::{ExprType, InnerRef, TypeId},
    utils::{ptr_to_slice, ptr_to_str, try_ptr_to_slice, try_ptr_to_type},
};
use ftdb_sys::ftdb::{deref_info, offsetref_info};

pub struct DerefInfo<'a>(&'a deref_info);

impl<'a> From<&'a deref_info> for DerefInfo<'a> {
    fn from(inner: &'a deref_info) -> Self {
        Self(inner)
    }
}

impl<'s, 'r> InnerRef<'s, 'r, deref_info> for DerefInfo<'r> {
    fn inner_ref(&'s self) -> &'r deref_info {
        self.0
    }
}

impl<'a> DerefInfo<'a> {
    pub fn access(&self) -> Option<&[u64]> {
        try_ptr_to_slice(self.0.access, self.0.access_count)
    }

    /// ftdb func derefinfo entry basecnt value
    ///
    pub fn basecnt(&self) -> Option<u64> {
        try_ptr_to_type(self.0.basecnt)
    }

    ///
    ///
    pub fn csid(&self) -> i64 {
        self.0.csid
    }

    /// ftdb func derefinfo entry expr value
    ///
    pub fn expr(&self) -> &str {
        ptr_to_str(self.0.expr)
    }

    /// ftdb func derefinfo entry kind value
    ///
    pub fn kind(&self) -> ExprType {
        self.0.kind.into()
    }

    ///
    ///
    pub fn mcall(&self) -> Option<&[i64]> {
        try_ptr_to_slice(self.0.mcall, self.0.mcall_count)
    }

    ///
    ///
    pub fn member(&self) -> Option<&[i64]> {
        try_ptr_to_slice(self.0.member, self.0.member_count)
    }

    /// ftdb func derefinfo entry offset value
    ///
    pub fn offset(&self) -> Option<i64> {
        try_ptr_to_type(self.0.offset)
    }

    ///
    ///
    pub fn ords(&self) -> &[u64] {
        ptr_to_slice(self.0.ord, self.0.ord_count)
    }

    /// ftdb func derefinfo entry offsetrefs values
    ///
    pub fn offsetrefs(&self) -> Vec<OffsetRef> {
        self.offsetrefs_iter().collect()
    }

    pub fn offsetrefs_iter(&self) -> impl ExactSizeIterator<Item = OffsetRef> {
        ptr_to_slice(self.0.offsetrefs, self.0.offsetrefs_count)
            .iter()
            .map(|x| x.into())
    }

    ///
    ///
    pub fn shift(&self) -> Option<&[i64]> {
        try_ptr_to_slice(self.0.shift, self.0.shift_count)
    }

    ///
    ///
    pub fn type_(&self) -> Option<&[TypeId]> {
        try_ptr_to_slice(self.0.type_ as *const TypeId, self.0.type_count)
    }
}

pub struct OffsetRef<'a>(&'a offsetref_info);

impl<'a> From<&'a offsetref_info> for OffsetRef<'a> {
    fn from(inner: &'a offsetref_info) -> Self {
        Self(inner)
    }
}

impl<'a> OffsetRef<'a> {
    ///
    ///
    pub fn cast(&self) -> Option<u64> {
        try_ptr_to_type(self.0.cast)
    }

    ///
    ///
    pub fn di(&self) -> Option<u64> {
        try_ptr_to_type(self.0.di)
    }

    ///
    ///
    pub fn id(&self) -> OffsetRefId {
        match self.kind() {
            ExprType::String => OffsetRefId::String(ptr_to_str(self.0.id_string)),
            ExprType::Floating => OffsetRefId::Float(self.0.id_float),
            _ => OffsetRefId::Integer(self.0.id_integer),
        }
    }

    ///
    ///
    pub fn kind(&self) -> ExprType {
        self.0.kind.into()
    }

    ///
    ///
    pub fn mi(&self) -> Option<u64> {
        try_ptr_to_type(self.0.mi)
    }
}

pub enum OffsetRefId<'a> {
    String(&'a str),
    Float(f64),
    Integer(i64),
}
