use crate::{db::ExprType, macros::impl_expr_iface, utils::ptr_to_slice, CsId, InnerRef};
use ftdb_sys::ftdb::{if_info, ifref_info};

/// Represents an if statement block
///
#[derive(Debug, Clone)]
pub struct If<'a>(&'a if_info);

impl<'a> From<&'a if_info> for If<'a> {
    fn from(inner: &'a if_info) -> Self {
        Self(inner)
    }
}

impl<'s, 'r> InnerRef<'s, 'r, if_info> for If<'r> {
    fn as_inner_ref(&'s self) -> &'r if_info {
        self.0
    }
}

impl<'a> If<'a> {
    /// Id of a compount statement this if section belongs to
    ///
    pub fn csid(&self) -> CsId {
        self.as_inner_ref()
            .csid
            .try_into()
            .expect("csid expected >= 0")
    }

    /// List of details of referenced objects
    ///
    pub fn refs(&self) -> Vec<IfRef<'a>> {
        self.refs_iter().collect()
    }

    /// Iterate over details of referenced objects
    ///
    pub fn refs_iter(&self) -> impl ExactSizeIterator<Item = IfRef<'a>> {
        ptr_to_slice(self.as_inner_ref().refs, self.as_inner_ref().refs_count)
            .iter()
            .map(|x| x.into())
    }
}

#[derive(Debug, Clone)]
pub struct IfRef<'a>(&'a ifref_info);

impl<'a> From<&'a ifref_info> for IfRef<'a> {
    fn from(inner: &'a ifref_info) -> Self {
        Self(inner)
    }
}

impl<'s, 'r> InnerRef<'s, 'r, ifref_info> for IfRef<'r> {
    fn as_inner_ref(&'s self) -> &'r ifref_info {
        self.0
    }
}

impl_expr_iface!(IfRef);
