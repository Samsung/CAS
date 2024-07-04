use crate::{
    utils::{ptr_to_slice, ptr_to_str},
    GlobalId,
};
use ftdb_sys::ftdb::{globalref_data, globalref_info};

pub struct GlobalRefs<'a>(&'a [GlobalId], &'a [globalref_data]);

impl<'a> GlobalRefs<'a> {
    pub fn new(refs: &'a [GlobalId], data: &'a [globalref_data]) -> Self {
        Self(refs, data)
    }

    pub fn iter(&self) -> impl Iterator<Item = GlobalRef> {
        std::iter::zip(self.0.iter(), self.1.iter()).map(|(id, info)| GlobalRef::new(*id, info))
    }
}

pub struct GlobalRef<'a> {
    /// Id of a global
    pub id: GlobalId,
    info: &'a [globalref_info],
}

impl<'a> GlobalRef<'a> {
    pub fn new(id: GlobalId, data: &'a globalref_data) -> Self {
        let info = ptr_to_slice(data.refs, data.refs_count);
        Self { id, info }
    }

    /// Iterator to global ref occurence
    ///
    pub fn info_iter(&self) -> impl ExactSizeIterator<Item = GlobalRefInfo> {
        self.info.iter().map(|x| x.into())
    }
}

pub struct GlobalRefInfo<'a>(&'a globalref_info);

impl<'a> From<&'a globalref_info> for GlobalRefInfo<'a> {
    fn from(inner: &'a globalref_info) -> Self {
        Self(inner)
    }
}

impl<'a> GlobalRefInfo<'a> {
    /// global ref start offset
    ///
    pub fn start(&self) -> &'a str {
        ptr_to_str(self.0.start)
    }

    /// global ref end offset
    ///
    pub fn end(&self) -> &'a str {
        ptr_to_str(self.0.end)
    }
}
