use crate::{
    macros::impl_expr_iface,
    utils::{ptr_to_slice, ptr_to_str},
    BorrowedIterator, DerefId, ExprType, FunctionId, InnerRef, Location, TypeId,
};
use ftdb_sys::ftdb::{call_info, callref_data, callref_info, ftdb_func_entry, refcall};

#[derive(Debug, Clone)]
pub struct CallEntryIterator<'a> {
    db: &'a ftdb_func_entry,
    cur: usize,
}

impl<'a> CallEntryIterator<'a> {
    pub(crate) fn new(func: &'a ftdb_func_entry) -> Self {
        CallEntryIterator { db: func, cur: 0 }
    }
}

impl<'a> Iterator for CallEntryIterator<'a> {
    type Item = CallEntry<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur < self.db.calls_count as usize {
            unsafe {
                let id = self.db.calls.add(self.cur);
                let ci = self.db.call_info.add(self.cur);
                let cri = self.db.callrefs.add(self.cur);
                self.cur += 1;
                Some(CallEntry {
                    id: FunctionId::from(*id),
                    ci: &*ci,
                    cr: &*cri,
                })
            }
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let remaining = self.db.calls_count as usize - self.cur;
        (remaining, Some(remaining))
    }
}

impl<'a> ExactSizeIterator for CallEntryIterator<'a> {}

/// Function call details
///
/// Groups together common fields related to calls:
///   - `calls`
///   - `callrefs`
///   - `call_info`
///
#[derive(Debug, Clone)]
pub struct CallEntry<'a> {
    /// Id of a called function
    pub id: FunctionId,

    /// Inner data structure, representing `call_info`
    ci: &'a call_info,

    /// Inner data structure representing one of `callrefs` entries
    cr: &'a callref_info,
}

impl<'a> CallEntry<'a> {
    /// Details about the function call (where in source code, how many arguments, etc)
    ///
    pub fn info(&self) -> CallInfo<'a> {
        self.ci.into()
    }

    /// List of info about function call arguments
    ///
    pub fn data(&self) -> Vec<CallRefData<'a>> {
        self.data_iter().collect()
    }

    /// Iterate over info about function call arguments
    ///
    pub fn data_iter(&self) -> impl ExactSizeIterator<Item = CallRefData<'a>> {
        ptr_to_slice(self.cr.callarg, self.cr.callarg_count)
            .iter()
            .map(|x| x.into())
    }
}

/// Details about the function call (such as where in the source code it is, how many
/// arguments has been passed, call expression, etc).
///
/// Represents a single `call_info` entry.
///
#[derive(Debug, Clone)]
pub struct CallInfo<'a>(&'a call_info);

impl<'a> From<&'a call_info> for CallInfo<'a> {
    fn from(inner: &'a call_info) -> Self {
        Self(inner)
    }
}

impl<'a> CallInfo<'a> {
    /// Ids of derefs
    ///
    pub fn args(&self) -> &'a [DerefId] {
        ptr_to_slice(self.0.args as *const DerefId, self.0.args_count)
    }

    /// Ending position of an expression in a source code (line and column)
    ///
    pub fn end(&self) -> &'a str {
        ptr_to_str(self.0.end)
    }

    /// Call expression as a string
    ///
    /// Expression is given in a form of: `[/path/to/file:940:19]: expresion string`
    ///
    pub fn expr(&self) -> &'a str {
        ptr_to_str(self.0.expr)
    }

    /// Location of an expression
    ///
    pub fn loc(&self) -> Location<'a> {
        ptr_to_str(self.0.loc).into()
    }

    /// Some internal ordering value
    ///
    /// TODO: To be verified
    ///
    pub fn ord(&self) -> u64 {
        self.0.ord
    }

    /// Starting position of an expression in a source code (line and column)
    ///
    pub fn start(&self) -> &'a str {
        ptr_to_str(self.0.start)
    }
}

/// An iterator over fptr calls
///
pub struct RefCallsIterator<'a> {
    func: &'a ftdb_func_entry,
    cur: usize,
}

impl<'a> RefCallsIterator<'a> {
    pub(crate) fn new(func: &'a ftdb_func_entry) -> Self {
        Self { func, cur: 0 }
    }
}

impl<'a> Iterator for RefCallsIterator<'a> {
    type Item = CallRefInfo<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.cur < self.func.refcalls_count as usize {
            unsafe {
                let rc = self.func.refcalls.add(self.cur);
                let rci = self.func.refcall_info.add(self.cur);
                let rcr = self.func.refcallrefs.add(self.cur);
                self.cur += 1;
                Some(CallRefInfo::new(&*rc, &*rci, &*rcr))
            }
        } else {
            None
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let remaining = self.func.refcalls_count as usize - self.cur;
        (remaining, Some(remaining))
    }
}

// Memory size of refcalls and refcallrefs is known
impl<'a> ExactSizeIterator for RefCallsIterator<'a> {}

/// Represents a single call over a function pointer
///
/// Groups together common fields related to refcalls:
///
/// * `refcalls`
/// * `refcallrefs`
/// * `refcall_info`
///
#[derive(Debug, Clone)]
pub struct CallRefInfo<'a> {
    rc: &'a refcall,
    rci: &'a call_info,
    rcr: &'a callref_info,
}

/// Represents what is the source of a call - a member struct or a raw fptr
///
#[derive(Debug, Clone)]
pub enum RefCallType {
    /// This was a struct member call
    MemberCall {
        /// Struct's field type
        fptr_type: TypeId,

        /// Struct's type
        struct_type: TypeId,

        /// Index in a type's refs - what is the order of a field
        refs_index: u64,
    },

    /// This was a function pointer call
    Call(
        /// Function pointer's type
        TypeId,
    ),
}

impl<'a> CallRefInfo<'a> {
    /// Create a new instance of RefCall wrapping ftdb_sys structs
    ///
    fn new(rc: &'a refcall, rci: &'a call_info, rcr: &'a callref_info) -> Self {
        Self { rc, rci, rcr }
    }

    /// Returns type of call - if it's a function or field's struct call
    ///
    pub fn kind(&self) -> RefCallType {
        if self.rc.ismembercall != 0 {
            RefCallType::MemberCall {
                fptr_type: self.rc.fid.into(),
                struct_type: self.rc.cid.into(),
                refs_index: self.rc.field_index,
            }
        } else {
            RefCallType::Call(self.rc.fid.into())
        }
    }

    /// Provides more details on function call, like where in the source code
    /// it happened, what is the number of arguments, etc.
    ///
    /// Corresponds to `call_info` struct
    ///
    pub fn info(&self) -> CallInfo<'a> {
        self.rci.into()
    }

    /// List of info about arguments passed to the fptr call
    ///
    ///
    pub fn data(&self) -> Vec<CallRefData<'a>> {
        self.data_iter().collect()
    }

    /// Iterate over info about arguments passed to the fptr call
    ///
    /// Corresponds to `callref_info` struct
    ///
    pub fn data_iter(&self) -> impl ExactSizeIterator<Item = CallRefData<'a>> {
        BorrowedIterator::<callref_info, CallRefData, callref_data>::new(self.rcr)
    }
}

/// Describes an argument of a function call
///
/// Represents a `callref_info` from FTDB.
///
#[derive(Debug, Clone)]
pub struct CallRefData<'a>(&'a callref_data);

impl<'a> From<&'a callref_data> for CallRefData<'a> {
    fn from(inner: &'a callref_data) -> Self {
        Self(inner)
    }
}

impl<'s, 'r> InnerRef<'s, 'r, callref_data> for CallRefData<'r> {
    fn as_inner_ref(&'s self) -> &'r callref_data {
        self.0
    }
}

impl_expr_iface!(CallRefData);

impl<'a> CallRefData<'a> {
    /// Position of this function call argument
    ///
    pub fn pos(&self) -> u64 {
        self.as_inner_ref().pos
    }
}
