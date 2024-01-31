use crate::{
    db::ExprType,
    utils::{ptr_to_slice, try_ptr_to_str, try_ptr_to_type},
};
use ftdb_sys::ftdb::{if_info, ifref_info};

pub struct If<'a>(&'a if_info);

impl<'a> From<&'a if_info> for If<'a> {
    fn from(inner: &'a if_info) -> Self {
        Self(inner)
    }
}

impl<'a> If<'a> {
    ///
    ///
    pub fn csid(&self) -> i64 {
        self.0.csid
    }

    ///
    ///
    pub fn refs_iter(&self) -> impl Iterator<Item = IfRef> {
        ptr_to_slice(self.0.refs, self.0.refs_count)
            .iter()
            .map(|x| x.into())
    }
}

pub struct IfRef<'a>(&'a ifref_info);

impl<'a> From<&'a ifref_info> for IfRef<'a> {
    fn from(inner: &'a ifref_info) -> Self {
        Self(inner)
    }
}

impl<'a> IfRef<'a> {
    ///
    ///
    pub fn kind(&self) -> ExprType {
        self.0.type_.into()
    }

    ///
    ///
    pub fn id(&self) -> IfRefId {
        if let Some(id) = try_ptr_to_type(self.0.id) {
            return IfRefId::Id(id);
        }
        if let Some(id) = try_ptr_to_type(self.0.integer_literal) {
            return IfRefId::Integer(id);
        }
        if let Some(id) = try_ptr_to_type(self.0.character_literal) {
            return IfRefId::Character(id);
        }
        if let Some(id) = try_ptr_to_type(self.0.floating_literal) {
            return IfRefId::Floating(id);
        }
        if let Some(id) = try_ptr_to_str(self.0.string_literal) {
            return IfRefId::String(id);
        }
        unreachable!()
    }
}

pub enum IfRefId<'a> {
    Id(u64),
    Integer(i64),
    Character(i32),
    Floating(f64),
    String(&'a str),
}
