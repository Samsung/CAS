mod borrowed;
mod owned;

use crate::macros::impl_inner_handle;

use super::{BorrowedIterator, FtdbCollection, FtdbHandle, Handle, InnerRef, Owned, TypeId};
use ftdb_sys::ftdb::{ftdb, ftdb_type_entry, query::TypesQuery};
use std::fmt::Display;

pub use self::borrowed::TypeEntry;
pub use self::owned::TypeEntry as OwnedTypeEntry;

/// The 'types' section of FTDB contains information about types in a source code.
///
/// There are several ways to browse type information:
///
/// 1. Use [`Ftdb`] instance
///     * Iterate over [`crate::Ftdb::types_iter`] iterator
///     * Filter by type's id [`crate::Ftdb::type_by_id`]
///     * Filter by type's hash [`crate::Ftdb::type_by_hash`]
///     * Filter by type's name [`crate::Ftdb::types_by_name`] (linear search!)
/// 2. Use [`Types`] instance
///     * Iterate over [`Types::iter`] iterator
///     * Filter by type's id [`Types::entry_by_id`]
///     * Filter by type's hash [`Types::entry_by_hash`]
///
/// `Types` instance shares ownership of FTDB database which means that it increases
/// internal reference counters so as long as the `Types` instance exists the
/// underlying FTDB pointers won't be free'd.
///
/// Because the use of reference counting structures (such as `std::sync::Arc`) it is
/// safe to use this type in concurency scenarios. It is possible to process each type
/// in a separate worker, just make sure to use "owned" variant of structures.
///
///
/// # Examples
///
/// Converting "borrowed" type entries to "owned" entries:
///
/// ```
/// use ftdb::Handle;
///
/// # fn run(db: &ftdb::Ftdb, t: ftdb::TypeEntry<'_>) {
/// let handle = db.handle();
/// let owned = t.into_owned(handle);
/// # }
/// ```
///
/// Asynchronously processing 1000 types per worker and presenting group results:
///
/// ```
/// use futures::{stream::FuturesUnordered, StreamExt};
/// use itertools::Itertools;
/// use ftdb::Handle;
///
/// struct ProcessingResult(String);
///
/// fn process_type(t: ftdb::OwnedTypeEntry) -> ProcessingResult {
///     // [...]
///     ProcessingResult(String::from("failed"))
/// }
///
/// fn display_results(results: Vec<ProcessingResult>) {
///     // [...]
/// }
///
/// async fn run(db: &ftdb::Ftdb) {
///     const TYPES_PER_WORKER: usize = 1000;
///     let handle = db.handle();
///     let tasks: FuturesUnordered<_> = db.types_iter()
///         .map(|t| t.into_owned(handle.clone()))
///         .chunks(TYPES_PER_WORKER)
///         .into_iter()
///         .map(|chunk| {
///             let chunk_vec = chunk.collect_vec();
///             async move {
///                 let task = tokio::spawn(async move {
///                     chunk_vec.into_iter()
///                         .map(process_type)
///                         .collect::<Vec<_>>()
///                 });
///                 task.await.expect("Task is finished")
///             }
///         })
///         .collect();
///
///     let results: Vec<_> = tasks
///        .collect::<Vec<_>>()
///        .await
///        .into_iter()
///        .flatten()
///        .collect();
///
///     display_results(results);
/// }
/// ```
///
/// [`Ftdb`]: crate::Ftdb
///
#[derive(Debug, Clone)]
pub struct Types(Owned<ftdb>);

impl Display for Types {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<Types: {} types>", self.len())
    }
}

impl From<Owned<ftdb>> for Types {
    fn from(value: Owned<ftdb>) -> Self {
        Self(value)
    }
}

impl FtdbCollection<ftdb_type_entry> for Types {
    #[inline(always)]
    unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_type_entry {
        self.0.get_ptr(index)
    }

    #[inline(always)]
    fn len(&self) -> usize {
        <ftdb as FtdbCollection<ftdb_type_entry>>::len(self.as_inner_ref())
    }
}

impl<'s> InnerRef<'s, 's, ftdb> for Types {
    #[inline(always)]
    fn as_inner_ref(&'s self) -> &'s ftdb {
        self.0.as_inner_ref()
    }
}

impl_inner_handle!(Types);

impl Types {
    /// Construct `Types` instance from the raw parts
    ///
    /// # Arguments
    ///
    /// * `db` - Pointer to the underlying structure, retrieved from `libftdb_c_ftdb_object()`
    /// * `handle` - A shared reference to the pointer retrieved from `libftdb_c_ftdb_load()`
    ///
    pub(crate) fn new(db: std::ptr::NonNull<ftdb>, handle: std::sync::Arc<FtdbHandle>) -> Self {
        Self(Owned { db, handle })
    }

    /// Get type object by its unique id
    ///
    /// # Arguments
    ///
    /// * `id` - id of a type to look for
    ///
    /// # Examples
    ///
    /// ```
    /// # use ftdb::TypeId;
    /// # fn run(types: &ftdb::Types) {
    /// let id = TypeId::from(3466u64);
    /// if let Some(t) = types.entry_by_id(id) {
    ///     if let Some(location) = t.location() {
    ///         println!("// Type {id} from {}", location);
    ///     }
    ///     if let Some(type_code) = t.def() {
    ///         println!("{}", type_code);
    ///     }
    /// } else {
    ///     eprintln!("Type {id} not found");
    /// }
    /// # }
    /// ```
    #[inline]
    pub fn entry_by_id(&self, id: TypeId) -> Option<borrowed::TypeEntry<'_>> {
        self.0.type_by_id(id.0).map(borrowed::TypeEntry::from)
    }

    /// Get type by its hash
    ///
    /// # Arguments
    ///
    /// * `hash` - hash of a type to look for
    ///
    /// # Examples
    ///
    /// ```
    /// # fn run(types: &ftdb::Types) {
    /// let hash = "...";
    /// if let Some(t) = types.entry_by_hash(hash) {
    ///     if let Some(location) = t.location() {
    ///         assert_eq!(hash, t.hash());
    ///         println!("// type {} from {}", t.hash(), location);
    ///     }
    ///     if let Some(type_code) = t.def() {
    ///         println!("{}", type_code);
    ///     }
    /// } else {
    ///     eprintln!("Hash {hash} not found");
    /// }
    /// # }
    /// ```
    ///
    #[inline]
    pub fn entry_by_hash(&self, hash: &str) -> Option<borrowed::TypeEntry<'_>> {
        self.0.type_by_hash(hash).map(borrowed::TypeEntry::from)
    }

    /// Iterate through type entries
    ///
    /// # Examples
    ///
    /// Calculate number of types for each type classes:
    ///
    /// ```
    /// use std::collections::HashMap;
    ///
    /// # fn run(types: &ftdb::Types) {
    /// let type_classes : HashMap<ftdb::TypeClass, u64> = types.iter()
    ///     .map(|t| t.classid())
    ///     .fold(HashMap::new(), |mut map, classid| {
    ///         let mut entry = map.entry(classid).or_default();
    ///         *entry += 1;
    ///         map
    ///     });
    ///  println!("{:?}", type_classes);
    ///
    /// # }
    /// ```
    ///
    #[inline]
    pub fn iter(&self) -> BorrowedIterator<'_, ftdb, borrowed::TypeEntry, ftdb_type_entry> {
        BorrowedIterator::new(&self.0)
    }
}

/// Describes a size of a bitfield
///
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct Bitfield {
    /// Index of a member in a struct
    pub index: u64,

    /// Number of bit width
    pub width: u64,
}

impl Bitfield {
    /// Construct a Bitfield instance
    ///
    /// # Arguments:
    ///
    /// * `index` - Index of a field in a struct
    /// * `width` - Number of bit width
    ///
    pub fn new(index: u64, width: u64) -> Self {
        Bitfield { index, width }
    }
}

impl From<&::ftdb_sys::ftdb::bitfield> for Bitfield {
    fn from(src: &::ftdb_sys::ftdb::bitfield) -> Self {
        Self {
            index: src.index,
            width: src.bitwidth,
        }
    }
}

impl Display for Bitfield {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "({}: {})", self.index, self.width)
    }
}

#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct OuterFn<'a> {
    pub name: &'a str,
    pub id: u64,
}

impl<'a> OuterFn<'a> {
    pub fn new(id: u64, name: &'a str) -> Self {
        OuterFn { name, id }
    }
}

/// Describes an enum value
///
#[derive(Debug, Clone)]
pub struct EnumEntry<'a> {
    /// Name of an enum variant
    pub identifier: &'a str,

    /// Value of the variant
    pub value: i64,
}

macro_rules! type_entry_impl {
    ($struct_name:ident) => {
        type_entry_impl!($struct_name, '_);
    };

    ($struct_name:ident<$struct_life:lifetime>) => {
        type_entry_impl!($struct_name<$struct_life>, $struct_life);
    };

    ($struct_name:ident $(<$struct_life:lifetime>)?, $ret_life:lifetime) => {

        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {

            /// List of types referenced in type attributes
            ///
            /// Example:
            ///
            /// ```c
            /// struct __attribute__((aligned(1 << WORK_STRUCT_FLAG_BITS))) pool_workqueue {
            ///     // [...]
            /// };
            /// ```
            ///
            /// In this example, `pool_workqueue` struct refers to anonymous enum which
            /// contains a value `WORK_STRUCT_FLAG_BITS`. When `attrrefs` is called on
            /// `pool_workqueue` type it will contain id of an enum type:
            ///
            /// ```c
            /// enum {
            ///     WORK_STRUCT_PENDING_BIT = 0,
            ///     WORK_STRUCT_INACTIVE_BIT = 1,
            ///     WORK_STRUCT_PWQ_BIT = 2,
            ///     WORK_STRUCT_LINKED_BIT = 3,
            ///     WORK_STRUCT_COLOR_SHIFT = 4,
            ///     WORK_STRUCT_COLOR_BITS = 4,
            ///     // [...]
            ///     WORK_STRUCT_FLAG_BITS = WORK_STRUCT_COLOR_SHIFT + WORK_STRUCT_COLOR_BITS,
            ///     // [...]
            /// }
            /// ```
            ///
            pub fn attrrefs(&self) -> &$ret_life [u64] {
                $crate::utils::ptr_to_slice(self.as_inner_ref().attrrefs, self.as_inner_ref().attrrefs_count)
            }

            /// A vector of bitfield fields with sizes in bits
            ///
            /// See also `bitfields_iter` function

            pub fn bitfields(&self) -> Vec<$crate::db::Bitfield> {
                self.bitfields_iter().collect()
            }

            /// Iterate through bitfield fields with sizes in bits
            ///
            /// Example:
            ///
            /// ```c
            /// struct vm_region {
            ///    struct rb_node vm_rb;
            ///    vm_flags_t vm_flags;
            ///    unsigned long vm_start;
            ///    unsigned long vm_end;
            ///    unsigned long vm_top;
            ///    unsigned long vm_pgoff;
            ///    struct file *vm_file;
            ///    int vm_usage;
            ///    bool vm_icache_flushed : 1;
            /// };
            /// ```
            ///
            /// For a type from the example above following is true:
            ///
            /// ```
            /// # fn run(t: ftdb::TypeEntry<'_>) {
            /// let bf = t.bitfields_iter().next().unwrap();
            /// assert_eq!(bf.index, 8);
            /// assert_eq!(bf.width, 1);
            /// # }
            /// ```
            ///
            pub fn bitfields_iter(&self) -> impl ExactSizeIterator<Item = $crate::db::Bitfield> + $ret_life {
                $crate::utils::ptr_to_slice(self.as_inner_ref().bitfields, self.as_inner_ref().bitfields_count)
                    .iter()
                    .map(|x| x.into())
            }

            /// Class of a type in a string form
            ///
            pub fn classname(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().class_name)
            }

            /// Class of a type in an enum form
            ///
            pub fn classid(&self) -> $crate::db::TypeClass {
                self.as_inner_ref().__class.into()
            }

            /// Class of a type in a numeric form
            ///
            pub fn classnum(&self) -> u32 {
                self.as_inner_ref().__class as u32
            }

            /// Type definition code (if applicable)
            ///
            /// It is applicable for type classes such as: Function, Typedef, Record,
            /// RecordForward, Enum, EnumForward.
            ///
            pub fn def(&self) -> Option<&$ret_life str> {
                $crate::utils::try_ptr_to_str(self.as_inner_ref().def)
            }

            pub fn defhead(&self) -> Option<&$ret_life str> {
                $crate::utils::try_ptr_to_str(self.as_inner_ref().defhead)
            }

            /// Iterate over enum entries (if applicable)
            ///
            /// For types of class enum this allows to iterate over enum identifiers
            /// and values assigned to them.
            ///
            /// # Examples
            ///
            /// ```
            /// # fn run(t: ftdb::TypeEntry<'_>) {
            /// println!("Enum {}", t.str_());
            /// for entry in t.enums_iter() {
            ///     println!("  {} = {}", entry.identifier, entry.value);
            /// }
            /// # }
            /// ```
            ///
            pub fn enums_iter(&self) -> impl Iterator<Item = $crate::db::EnumEntry<$ret_life>> + $ret_life {
                debug_assert!(
                    self.as_inner_ref().identifiers_count == self.as_inner_ref().enumvalues_count,
                    "identifiers_count != enumvalues_count"
                );
                let ids = $crate::utils::ptr_to_slice(
                    self.as_inner_ref().identifiers,
                    self.as_inner_ref().identifiers_count,
                );
                let values = $crate::utils::ptr_to_slice(
                    self.as_inner_ref().enumvalues,
                    self.as_inner_ref().enumvalues_count,
                );
                std::iter::zip(ids, values)
                    .map(|(id, value)| $crate::db::EnumEntry {
                        identifier: $crate::utils::ptr_to_str(*id),
                        value: *value
                    })
            }

            /// Id of a first encountered file with this type
            ///
            /// # Examples:
            ///
            /// ```
            /// # fn run<'a>(db: &'a ftdb::Ftdb, t: ftdb::TypeEntry<'a>) {
            /// let fid = t.fid();
            /// if let Some(source) = db.source_by_id(fid) {
            ///     println!("Type {} first encountered in file {source}", t.id());
            /// }
            /// # }
            /// ```
            ///
            pub fn fid(&self) -> $crate::db::FileId {
                self.as_inner_ref().fid.into()
            }

            /// List of functions referenced by this type
            ///
            /// Example:
            ///
            /// ```c
            /// struct __attribute__((packed)) {
            ///     typeof (*((__le16 *)skb_put(skb, 2))) x;
            /// }
            /// ```
            ///
            /// For a type from the example above following is true:
            ///
            /// ```
            /// # fn run<'a>(db: &'a ftdb::Ftdb, t: ftdb::TypeEntry<'a>) {
            /// let func_id = t.funrefs()[0];
            /// if let Some(func) = db.func_by_id(func_id) {
            ///     assert_eq!(func.name(), "skb_put");
            /// }
            /// # }
            /// ```
            ///
            pub fn funrefs(&self) -> &[$crate::db::FunctionId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().funrefs as *const $crate::db::FunctionId,
                    self.as_inner_ref().funrefs_count
                )
            }

            /// List of referenced global variables
            ///
            ///
            pub fn globalrefs(&self) -> &$ret_life [$crate::db::GlobalId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().globalrefs as *const $crate::db::GlobalId,
                    self.as_inner_ref().globalrefs_count,
                )
            }

            /// Hash of a type
            ///
            /// Note: Hash type is guaranteed to be unique.
            ///
            pub fn hash(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().hash)
            }

            /// Unique identifier of a type
            ///
            /// Note that ID is unique for this database file. For two databases taken
            /// from various points of development time, the same type in both
            /// databases might have a different ID.
            ///
            /// The ID doesn't represent type across various different databases,
            /// but this one.
            ///
            pub fn id(&self) -> $crate::db::TypeId {
                self.as_inner_ref().id.into()
            }

            /// Returns true if type depends on template parameter (applicable for C++
            /// code bases).
            ///
            pub fn is_dependent(&self) -> bool {
                unsafe { self.as_inner_ref().isdependent.as_ref() }
                    .map(|x| *x != 0)
                    .unwrap_or(false)
            }

            /// Returns true if a typedef is defined implicitly by a compiler
            ///
            /// Applicable for typedef types
            ///
            pub fn is_implicit(&self) -> bool {
                unsafe { self.as_inner_ref().isimplicit.as_ref() }
                    .map(|x| *x != 0)
                    .unwrap_or(false)
            }

            /// Returns true if type is an union
            ///
            pub fn is_union(&self) -> bool {
                unsafe { self.as_inner_ref().isunion.as_ref() }
                    .map(|x| *x != 0)
                    .unwrap_or(false)
            }

            /// Returns true if type describes variadic function
            ///
            /// Example:
            ///
            /// ```c
            /// type_t (other_type_t *, ...)
            /// ```
            ///
            /// For the entry of a type above, the following is true:
            ///
            /// ```
            /// # fn run(t: ftdb::TypeEntry<'_>) {
            /// assert_eq!(t.is_variadic(), true);
            /// # }
            /// ```
            ///
            pub fn is_variadic(&self) -> bool {
                unsafe { self.as_inner_ref().isvariadic.as_ref() }
                    .map(|x| *x != 0)
                    .unwrap_or(false)
            }

            /// Definition location in a source code
            ///
            pub fn location(&self) -> Option<$crate::db::Location<$ret_life>> {
                $crate::utils::try_ptr_to_str(self.as_inner_ref().location).map($crate::db::Location::from)
            }

            /// Returns consecutive offsets of fields in bits
            ///
            /// Example:
            ///
            /// ```c
            /// struct i2c_dev_boardinfo {
            ///     struct list_head node;
            ///     struct i2c_board_info base;
            ///     u8 lvr;
            /// }
            /// ```
            ///
            /// For the type defined above, knowing that size of `struct list_head`
            /// is 128 and size of `struct i2c_board_info` is 640, the following is
            /// true:
            ///
            /// ```
            /// # fn run(db: &ftdb::Ftdb, t: &ftdb::TypeEntry<'_>) {
            /// assert_eq!(t.memberoffsets().len(), 3);
            /// assert_eq!(t.memberoffsets()[0], 0);
            /// assert_eq!(t.memberoffsets()[1], 128);
            /// assert_eq!(t.memberoffsets()[2], 768);
            ///
            /// let size = t.memberoffsets()[2] as usize + db.type_by_id(t.refs()[2]).unwrap().size() ;
            /// println!("Additional structure padding is: {}", t.size() - size);
            /// # }
            /// ```
            ///
            pub fn memberoffsets(&self) -> &$ret_life [u64] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().memberoffsets,
                    self.as_inner_ref().memberoffsets_count,
                )
            }

            /// Name of a typedef
            ///
            pub fn name(&self) -> Option<&$ret_life str> {
                $crate::utils::try_ptr_to_str(self.as_inner_ref().name)
            }

            /// Return ID of a function inside which this type is defined
            ///
            pub fn outerfn(&self) -> Option<$crate::db::FunctionId> {
                $crate::utils::try_ptr_to_type(self.as_inner_ref().outerfnid)
                    .map($crate::db::FunctionId::from)
            }

            /// Returns string representing qualifiers of a type.
            ///
            /// Each character of the string represents some qualifier:
            /// * `c` - represents 'const'
            /// * `v` - represents ... ?
            /// * `r` - represents ... ?
            ///
            pub fn qualifiers(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().qualifiers)
            }

            /// Returns number of files containing type
            ///
            pub fn refcount(&self) -> usize {
                self.as_inner_ref().refcount as usize
            }

            /// List of name of fields within a structure
            ///
            pub fn refnames(&self) -> Vec<&$ret_life str> {
                self.refnames_iter().collect()
            }

            /// Iterate over name of fields within a structure
            ///
            /// Applicable for record and enum type classes.
            ///
            /// # Example
            ///
            /// ```c
            /// union {
            ///     void *arg;
            ///     const struct kparam_string *str;
            ///     const struct kparam_array *arr;
            /// }
            /// ```
            ///
            /// For the type defined above, the following is true:
            ///
            /// ```
            /// # fn run(t: &ftdb::TypeEntry<'_>) {
            /// let mut refnames = t.refnames_iter();
            /// assert_eq!(refnames.next(), Some("arg"));
            /// assert_eq!(refnames.next(), Some("str"));
            /// assert_eq!(refnames.next(), Some("arr"));
            /// # }
            /// ```
            ///
            pub fn refnames_iter(&self) -> impl ExactSizeIterator<Item = &$ret_life str> {
                $crate::utils::ptr_to_slice(self.as_inner_ref().refnames, self.as_inner_ref().refnames_count)
                    .iter()
                    .map(|x| $crate::utils::ptr_to_str(*x))
            }

            /// Returns list of referenced types
            ///
            /// # Example
            ///
            /// ```c
            /// struct hid_report_enum {
            ///     unsigned int numbered;
            ///     struct list_head report_list;
            ///     struct hid_report *report_id_hash[256];
            /// }
            /// ```
            ///
            /// For the type defined above, following is true:
            ///
            /// ```
            /// # fn run<'a>(db: &'a ftdb::Ftdb, t: &ftdb::TypeEntry<'a>) {
            /// let mut types = t.refs().iter()
            ///     .map(|id| db.type_by_id(*id).unwrap());
            /// assert_eq!(types.next().map(|t| t.str_()), Some("unsigned int"));
            /// assert_eq!(types.next().map(|t| t.str_()), Some("list_head"));
            /// assert_eq!(types.next().map(|t| t.str_()), Some("[N]"));
            /// # }
            /// ```
            ///
            /// There are few interesting cases
            ///
            /// 1. For arrays of known sizes and incomplete arrays, the length of this
            ///    list is always 1. The only element on the list refers to a type of
            ///    elements of an array
            /// 2. For pointers the length of this list is always 1 and the only
            ///    element refers to a type under the pointer.
            /// 3. For function pointers the refs list refers to actual types; first
            ///    element refers to returned type and other to types of parameters
            ///
            pub fn refs(&self) -> &$ret_life [$crate::db::TypeId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().refs as *const $crate::db::TypeId,
                    self.as_inner_ref().refs_count
                )
            }

            /// Returns size of a type in bits
            ///
            pub fn size(&self) -> usize {
                self.as_inner_ref().size as usize
            }

            /// Returns a string definining a type
            ///
            /// * `name` - name of a type, like `list_head`
            /// * `*` - for a pointer type
            /// * `()` - for function type
            /// * `[N]` - for arrays of known size
            /// * `[]` - for incomplete arrays
            ///
            pub fn str_(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().str_)
            }

            /// Returns list of referenced types as options. A referenced type is not
            /// if corresponding field is never used in a source code.
            ///
            /// # Examples
            ///
            /// ```c
            /// struct color {
            ///     uint8_t r;
            ///     uint8_t g;
            ///     uint8_t b;
            /// }
            ///
            /// struct color c;
            /// c.r = 5;
            /// c.b = 6;
            /// ```
            ///
            /// In the example above `g` field is never used, so:
            ///
            /// ```
            /// # fn run(t: ftdb::TypeEntry<'_>) {
            /// let mut usedrefs = t.usedrefs();
            /// assert_eq!(usedrefs.next(), Some(Some(t.refs()[0])));
            /// assert_eq!(usedrefs.next(), Some(Some(t.refs()[1])));
            /// assert_eq!(usedrefs.next(), None);
            /// # }
            /// ```
            ///
            pub fn usedrefs(&self) -> impl Iterator<Item = Option<$crate::db::TypeId>> + $ret_life {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().usedrefs,
                    self.as_inner_ref().usedrefs_count
                )
                .into_iter()
                .map(|value| {
                    if *value >= 0 {
                        Some($crate::db::TypeId(*value as u64))
                    } else {
                        None
                    }
                })
            }
        }
    }
}

pub(crate) use type_entry_impl;
