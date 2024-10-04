use crate::{utils::ptr_to_slice, LocalId};
use ftdb_sys::ftdb::taint_data;
use std::fmt::Display;

/// A tainting process refers to a possibility that variable's value is changed due to
/// other variable's value.
///
/// ```c
/// int function(char* buf) {
///     char tmp[32] = {0};
///     if (sscanf(buf, "%31s", tmp) != 1) {
///         return -1;
///     }
///     // do stuff with tmp
///     return 0;
/// }
/// ```
///
/// In this example a variable `tmp` is tainted because its value might come from
/// a `buf` variable.
///
/// # Examples
///
/// Let's define following function:
///
/// ```c
/// static ssize_t store_handler (
///     struct file *filp,
///     const char *ubuf,
///     size_t count,
///     loff_t *pos
/// ) {
///     bool enable;
///     int err;
///     err = kstrtobool_from_user(ubuf, count, &enable);
///     if (err) {
///         return err;
///     }
///     some_global = enable;
///     return count;
/// }
/// ```
///
/// It is true that:
///
/// ```
/// # use ftdb::TaintElement;
/// # fn run(db: &ftdb::Ftdb, f: &ftdb::FunctionEntry<'_>) {
/// assert_eq!(f.local_by_id(1u64.into()).unwrap().name(), "ubuf");
/// assert_eq!(f.local_by_id(2u64.into()).unwrap().name(), "count");
/// assert_eq!(f.local_by_id(4u64.into()).unwrap().name(), "enable");
/// assert_eq!(f.local_by_id(5u64.into()).unwrap().name(), "err");
///
/// let mut taints = f.taint_iter().skip(1);
///
/// // ubuf
/// let expected = vec![
///     TaintElement{level: 0, varid: 1u64.into()},
///     TaintElement{level: 1, varid: 5u64.into()},
///     TaintElement{level: 1, varid: 4u64.into()}
/// ];
/// let actual : Vec<_> = taints.next().unwrap().data_iter().collect();
/// assert_eq!(expected, actual);
///
/// // count
/// let expected = vec![
///     TaintElement{level: 0, varid: 2u64.into()},
///     TaintElement{level: 1, varid: 5u64.into()},
/// ];
/// let actual : Vec<_> = taints.next().unwrap().data_iter().collect();
/// assert_eq!(expected, actual);
///
/// # }
/// ```
///
/// Note that in current implementation process of tainting refers only to local
/// variables.
///
#[derive(Debug, Clone)]
pub struct Taint<'a> {
    /// Id of a local variable from which tainting process starts from
    ///
    pub index: LocalId,

    inner: &'a taint_data,
}

impl<'a> Taint<'a> {
    /// Create new Taint
    ///
    pub(crate) fn new(index: LocalId, data: &'a taint_data) -> Self {
        Self { index, inner: data }
    }

    /// List of taint elements for this taint
    ///
    pub fn data(&self) -> Vec<TaintElement> {
        self.data_iter().collect()
    }

    /// Iterate over taint dependencies
    ///
    pub fn data_iter(&self) -> impl ExactSizeIterator<Item = TaintElement> {
        ptr_to_slice(self.inner.taint_list, self.inner.taint_list_count)
            .iter()
            .map(|x| TaintElement {
                level: x.taint_level,
                varid: x.varid.into(),
            })
    }
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct TaintElement {
    /// Depth of a tainting
    pub level: u64,

    /// Variable which is tainted in that depth
    pub varid: LocalId,
}

impl Display for TaintElement {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "({}: {})", self.level, self.varid)
    }
}
