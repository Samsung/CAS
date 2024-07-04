use super::{Handle, Owned};
use std::{marker::PhantomData, ptr::NonNull};

/// Type implementing this trait is said to be given access to a collection of Items
///
pub trait FtdbCollection<Item> {
    /// Get raw pointer to the Item
    ///
    /// # Safety
    ///
    /// See std::primitive::pointer::add method documentation for more details
    ///
    unsafe fn get_ptr(&self, index: usize) -> *mut Item;

    /// Return number of elements in the collection
    ///
    fn len(&self) -> usize;

    /// Checks whether collection is empty or not
    ///
    fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// Get a reference to the Item
    ///
    fn get(&self, index: usize) -> &Item {
        unsafe { self.get_ptr(index).as_ref() }.expect("Pointer must be valid")
    }
}

/// Structure representing iterator over ReturnType elements that also guards FTDB
/// database from being released
///
pub struct IntoOwnedIterator<Db, ReturnType, InnerType> {
    /// Structure providing access to FTDB inner data
    db: Db,

    /// Current iteration index
    cur: usize,

    ///
    phantom: PhantomData<(ReturnType, InnerType)>,
}

impl<Db, ReturnType, InnerType> IntoOwnedIterator<Db, ReturnType, InnerType> {
    pub fn new(db: Db) -> Self {
        IntoOwnedIterator {
            db,
            cur: 0,
            phantom: PhantomData,
        }
    }
}

impl<Db, ReturnType, InnerType> Iterator for IntoOwnedIterator<Db, ReturnType, InnerType>
where
    Db: FtdbCollection<InnerType> + Handle,
    ReturnType: From<Owned<InnerType>>,
{
    type Item = ReturnType;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur < self.db.len() {
            let entry = unsafe { self.db.get_ptr(self.cur) };
            assert!(
                !entry.is_null(),
                "Collection behind the pointer cannot contain null elements"
            );
            self.cur += 1;
            let entry = Owned {
                db: unsafe { NonNull::new_unchecked(entry) },
                handle: self.db.handle(),
            };
            Some(entry.into())
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let remaining = self.db.len() - self.cur;
        (remaining, Some(remaining))
    }
}

impl<Db, ReturnType, InnerType> ExactSizeIterator for IntoOwnedIterator<Db, ReturnType, InnerType>
where
    Db: FtdbCollection<InnerType> + Handle,
    ReturnType: From<Owned<InnerType>>,
{
}

pub struct BorrowedIterator<'a, Db, ReturnType, InnerType> {
    db: &'a Db,
    cur: usize,
    phantom: PhantomData<(ReturnType, InnerType)>,
}

impl<'a, Db, ReturnType, InnerType> BorrowedIterator<'a, Db, ReturnType, InnerType> {
    pub fn new(db: &'a Db) -> Self {
        Self {
            db,
            cur: 0,
            phantom: PhantomData,
        }
    }
}

impl<'a, Db, ReturnType, InnerType> Iterator for BorrowedIterator<'a, Db, ReturnType, InnerType>
where
    Db: FtdbCollection<InnerType>,
    ReturnType: 'a + From<&'a InnerType>,
    InnerType: 'a,
{
    type Item = ReturnType;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur < self.db.len() {
            let entry = self.db.get(self.cur);
            self.cur += 1;
            Some(entry.into())
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let remaining = self.db.len() - self.cur;
        (remaining, Some(remaining))
    }
}

impl<'a, Db, ReturnType, InnerType> ExactSizeIterator
    for BorrowedIterator<'a, Db, ReturnType, InnerType>
where
    Db: FtdbCollection<InnerType>,
    ReturnType: 'a + From<&'a InnerType>,
    InnerType: 'a,
{
}

mod impls {
    use super::FtdbCollection;
    use ftdb_sys::ftdb::{
        ftdb, ftdb_fops_entry, ftdb_fops_member_entry, ftdb_func_entry, ftdb_funcdecl_entry,
        ftdb_global_entry, ftdb_type_entry, ftdb_unresolvedfunc_entry,
    };

    impl FtdbCollection<ftdb_fops_entry> for ftdb {
        #[inline(always)]
        unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_fops_entry {
            self.fops.add(index)
        }

        #[inline(always)]
        fn len(&self) -> usize {
            self.fops_count as usize
        }
    }

    impl FtdbCollection<ftdb_funcdecl_entry> for ftdb {
        #[inline(always)]
        unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_funcdecl_entry {
            self.funcdecls.add(index)
        }

        #[inline(always)]
        fn len(&self) -> usize {
            self.funcdecls_count as usize
        }
    }

    impl FtdbCollection<ftdb_func_entry> for ftdb {
        #[inline(always)]
        unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_func_entry {
            self.funcs.add(index)
        }

        #[inline(always)]
        fn len(&self) -> usize {
            self.funcs_count as usize
        }
    }

    impl FtdbCollection<ftdb_global_entry> for ftdb {
        #[inline(always)]
        unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_global_entry {
            self.globals.add(index)
        }

        #[inline(always)]
        fn len(&self) -> usize {
            self.globals_count as usize
        }
    }

    impl FtdbCollection<ftdb_fops_member_entry> for ftdb_fops_entry {
        #[inline(always)]
        unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_fops_member_entry {
            self.members.add(index)
        }

        #[inline(always)]
        fn len(&self) -> usize {
            self.members_count as usize
        }
    }

    impl FtdbCollection<ftdb_type_entry> for ftdb {
        #[inline(always)]
        unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_type_entry {
            self.types.add(index)
        }

        #[inline(always)]
        fn len(&self) -> usize {
            self.types_count as usize
        }
    }

    impl FtdbCollection<ftdb_unresolvedfunc_entry> for ftdb {
        #[inline(always)]
        unsafe fn get_ptr(&self, index: usize) -> *mut ftdb_unresolvedfunc_entry {
            self.unresolvedfuncs.add(index)
        }

        #[inline(always)]
        fn len(&self) -> usize {
            self.unresolvedfuncs_count as usize
        }
    }
}
