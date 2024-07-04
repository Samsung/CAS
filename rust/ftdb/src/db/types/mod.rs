mod borrowed;
mod owned;

use crate::macros::impl_inner_handle;

use super::{BorrowedIterator, FtdbCollection, FtdbHandle, Handle, InnerRef, Owned, TypeId};
use ftdb_sys::ftdb::{ftdb, ftdb_type_entry, query::TypesQuery};
use std::fmt::Display;

pub use self::borrowed::TypeEntry;

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
        <ftdb as FtdbCollection<ftdb_type_entry>>::len(self.inner_ref())
    }
}

impl<'s> InnerRef<'s, 's, ftdb> for Types {
    #[inline(always)]
    fn inner_ref(&'s self) -> &'s ftdb {
        self.0.inner_ref()
    }
}

impl_inner_handle!(Types);

impl Types {
    pub(crate) fn new(db: std::ptr::NonNull<ftdb>, handle: std::sync::Arc<FtdbHandle>) -> Self {
        Self(Owned { db, handle })
    }

    /// Find type by its unique identifier
    ///
    #[inline]
    pub fn entry_by_id(&self, id: TypeId) -> Option<borrowed::TypeEntry<'_>> {
        self.0.type_by_id(id.0).map(borrowed::TypeEntry::from)
    }

    /// Find type by its hash
    #[inline]
    pub fn entry_by_hash(&self, hash: &str) -> Option<borrowed::TypeEntry<'_>> {
        self.0.type_by_hash(hash).map(borrowed::TypeEntry::from)
    }

    /// Iterator through type entries
    ///
    #[inline]
    pub fn iter(&self) -> BorrowedIterator<'_, ftdb, borrowed::TypeEntry, ftdb_type_entry> {
        BorrowedIterator::new(&self.0)
    }
}

#[derive(Debug, PartialEq, Eq)]
pub struct Bitfield {
    pub index: u64,
    pub width: u64,
}

impl Bitfield {
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

#[derive(Debug, PartialEq, Eq)]
pub struct OuterFn<'a> {
    pub name: &'a str,
    pub id: u64,
}

impl<'a> OuterFn<'a> {
    pub fn new(id: u64, name: &'a str) -> Self {
        OuterFn { name, id }
    }
}

macro_rules! type_entry_impl {
    ($struct_name:ident) => {
        type_entry_impl!($struct_name, '_);
    };

    ($struct_name:ident<$struct_life:lifetime>) => {
        type_entry_impl!($struct_name<$struct_life>, $struct_life);
    };

    ($struct_name:ident $(<$struct_life:lifetime>)?, $ret_life:lifetime) => {
        use $crate::utils::{ptr_to_slice, ptr_to_str, try_ptr_to_str, try_ptr_to_slice, try_ptr_to_type};
        use $crate::db::{Bitfield, FunctionId, GlobalId, Location, OuterFn, TypeClass, TypeId};

        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {
            pub fn attrnum(&self) -> usize {
                unsafe { self.inner_ref().attrnum.as_ref() }
                    .map(|x| *x as usize)
                    .unwrap_or(0)
            }

            pub fn attrrefs(&self) -> &$ret_life [u64] {
                ptr_to_slice(self.inner_ref().attrrefs, self.inner_ref().attrrefs_count)
            }

            pub fn bitfields(&self) -> Vec<Bitfield> {
                self.bitfields_iter().collect()
            }

            pub fn bitfields_iter(&self) -> impl ExactSizeIterator<Item = Bitfield> + $ret_life {
                ptr_to_slice(self.inner_ref().bitfields, self.inner_ref().bitfields_count)
                    .iter()
                    .map(|x| x.into())
            }

            pub fn classname(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().class_name)
            }

            pub fn classid(&self) -> TypeClass {
                self.inner_ref().__class.into()
            }

            pub fn classnum(&self) -> u32 {
                self.inner_ref().__class as u32
            }

            pub fn def(&self) -> Option<&$ret_life str> {
                try_ptr_to_str(self.inner_ref().def)
            }

            pub fn defhead(&self) -> Option<&$ret_life str> {
                try_ptr_to_str(self.inner_ref().defhead)
            }

            pub fn enumvalues(&self) -> Option<&$ret_life [i64]> {
                try_ptr_to_slice(
                    self.inner_ref().enumvalues,
                    self.inner_ref().enumvalues_count,
                )
            }

            pub fn fid(&self) -> u64 {
                self.inner_ref().fid
            }

            pub fn funrefs(&self) -> &[FunctionId] {
                ptr_to_slice(
                    self.inner_ref().funrefs as *const FunctionId,
                    self.inner_ref().funrefs_count
                )
            }

            pub fn globalrefs(&self) -> &$ret_life [GlobalId] {
                ptr_to_slice(
                    self.inner_ref().globalrefs as *const GlobalId,
                    self.inner_ref().globalrefs_count,
                )
            }

            pub fn hash(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().hash)
            }

            pub fn id(&self) -> TypeId {
                self.inner_ref().id.into()
            }

            pub fn identifiers(&self) -> Vec<&$ret_life str> {
                self.identifiers_iter().collect()
            }

            pub fn identifiers_iter(&self) -> impl ExactSizeIterator<Item = &$ret_life str> + $ret_life {
                ptr_to_slice(
                    self.inner_ref().identifiers,
                    self.inner_ref().identifiers_count,
                )
                .iter()
                .map(|x| ptr_to_str(*x))
            }

            pub fn is_dependent(&self) -> bool {
                unsafe { self.inner_ref().isdependent.as_ref() }
                    .map(|x| *x != 0)
                    .unwrap_or(false)
            }

            pub fn is_implicit(&self) -> bool {
                unsafe { self.inner_ref().isimplicit.as_ref() }
                    .map(|x| *x != 0)
                    .unwrap_or(false)
            }

            pub fn is_union(&self) -> bool {
                unsafe { self.inner_ref().isunion.as_ref() }
                    .map(|x| *x != 0)
                    .unwrap_or(false)
            }

            pub fn is_variadic(&self) -> bool {
                unsafe { self.inner_ref().isvariadic.as_ref() }
                    .map(|x| *x != 0)
                    .unwrap_or(false)
            }

            pub fn location(&self) -> Option<Location<$ret_life>> {
                try_ptr_to_str(self.inner_ref().location).map(Location::from)
            }

            pub fn memberoffsets(&self) -> &$ret_life [u64] {
                ptr_to_slice(
                    self.inner_ref().memberoffsets,
                    self.inner_ref().memberoffsets_count,
                )
            }

            pub fn name(&self) -> Option<&$ret_life str> {
                try_ptr_to_str(self.inner_ref().name)
            }

            pub fn outerfn(&self) -> Option<OuterFn> {
                let name = try_ptr_to_str(self.inner_ref().outerfn);
                let id = try_ptr_to_type(self.inner_ref().outerfnid);

                name.zip(id).map(|(name, id)| OuterFn { name, id })
            }

            pub fn qualifiers(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().qualifiers)
            }

            pub fn refcount(&self) -> usize {
                self.inner_ref().refcount as usize
            }

            pub fn refnames(&self) -> Vec<&$ret_life str> {
                self.refnames_iter().collect()
            }

            pub fn refnames_iter(&self) -> impl ExactSizeIterator<Item = &$ret_life str> {
                ptr_to_slice(self.inner_ref().refnames, self.inner_ref().refnames_count)
                    .iter()
                    .map(|x| ptr_to_str(*x))
            }

            pub fn refs(&self) -> &$ret_life [TypeId] {
                ptr_to_slice(
                    self.inner_ref().refs as *const TypeId,
                    self.inner_ref().refs_count
                )
            }

            pub fn size(&self) -> usize {
                self.inner_ref().size as usize
            }

            pub fn str_(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().str_)
            }

            pub fn useddef(&self) -> Vec<&$ret_life str> {
                self.useddef_iter().collect()
            }

            pub fn useddef_iter(&self) -> impl ExactSizeIterator<Item = &$ret_life str> {
                ptr_to_slice(self.inner_ref().useddef, self.inner_ref().useddef_count)
                    .iter()
                    .map(|x| ptr_to_str(*x))
            }

            pub fn usedrefs(&self) -> &$ret_life [i64] {
                ptr_to_slice(self.inner_ref().usedrefs, self.inner_ref().usedrefs_count)
            }
        }
    }
}

pub(crate) use type_entry_impl;
