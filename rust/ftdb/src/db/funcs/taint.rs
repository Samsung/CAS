use crate::utils::ptr_to_slice;
use ftdb_sys::ftdb::taint_data;
use std::fmt::Display;

pub struct Taint<'a> {
    pub index: usize,
    inner: &'a taint_data,
}

impl<'a> Taint<'a> {
    /// Create new Taint
    ///
    pub fn new(index: usize, data: &'a taint_data) -> Self {
        Self { index, inner: data }
    }

    /// List of taint elements for this taint
    ///
    pub fn data(&self) -> Vec<TaintElement> {
        self.data_iter().collect()
    }

    pub fn data_iter(&self) -> impl ExactSizeIterator<Item = TaintElement> {
        ptr_to_slice(self.inner.taint_list, self.inner.taint_list_count)
            .iter()
            .map(|x| TaintElement {
                level: x.taint_level,
                varid: x.varid,
            })
    }
}

pub struct TaintElement {
    pub level: u64,
    pub varid: u64,
}

impl Display for TaintElement {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "({}: {})", self.level, self.varid)
    }
}
