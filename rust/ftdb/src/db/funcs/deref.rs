use crate::{
    db::{Expr, ExprType, InnerRef, TypeId},
    utils::{ptr_to_slice, ptr_to_str, try_ptr_to_slice, try_ptr_to_type},
    CsId,
};
use ftdb_sys::ftdb::{deref_info, offsetref_info};

/// A structure representing various kinds of derefs
///
/// Note: This API is unstable and will be changed in the future. Main reason
/// is that meaning of some of DerefInfo's API depends on its ExprType.
///
/// For more details visit documentation in [`CAS-OSS`] repository. This document
/// contains various examples and how they relate to 'deref' objects.
///
/// [`CAS-OSS`]: https://github.com/Samsung/CAS/blob/master/clang-proc/deref.md
///
#[derive(Debug, Clone)]
pub struct DerefInfo<'a>(&'a deref_info);

impl<'a> From<&'a deref_info> for DerefInfo<'a> {
    fn from(inner: &'a deref_info) -> Self {
        Self(inner)
    }
}

impl<'s, 'r> InnerRef<'s, 'r, deref_info> for DerefInfo<'r> {
    fn as_inner_ref(&'s self) -> &'r deref_info {
        self.0
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum MemberAccessKind {
    /// Access through object (x.y)
    Object,

    /// Access through pointer (x->y)
    Pointer,
}

impl From<u64> for MemberAccessKind {
    fn from(value: u64) -> Self {
        match value {
            0 => MemberAccessKind::Object,
            1 => MemberAccessKind::Pointer,
            _ => unimplemented!(),
        }
    }
}

impl From<&u64> for MemberAccessKind {
    fn from(value: &u64) -> Self {
        match value {
            0 => MemberAccessKind::Object,
            1 => MemberAccessKind::Pointer,
            _ => unimplemented!(),
        }
    }
}

impl<'a> DerefInfo<'a> {
    /// Describes what kind of member access expression was used
    ///
    /// For example, for this deref: `pps->info.echo` function returns:
    /// Some([1, 0])
    ///
    /// Note that for readability values might be casted to `MemberAccessKind` enum
    ///
    pub fn access(&self) -> Option<&[u64]> {
        try_ptr_to_slice(self.0.access, self.0.access_count)
    }

    /// Returns number of referenced variables
    ///
    /// Most of the times the value is 1.
    ///
    /// For an ExprType::Array it's number of expressions referenced in the base array.
    /// For an ExprType::Logic it's number of expressions referenced on the LHS expr.
    ///
    pub fn basecnt(&self) -> Option<u64> {
        try_ptr_to_type(self.0.basecnt)
    }

    /// Returns id of a compound statement of this deref
    ///
    pub fn csid(&self) -> CsId {
        self.0.csid.try_into().expect("csid expected >= 0")
    }

    /// An expression this deref represents
    ///
    pub fn expr(&self) -> Expr {
        Expr::new(self.expr_raw())
    }

    /// A raw value of deref expression
    ///
    /// Format: `[/path/to/file.c:1:60]: foo->shmoo`
    ///
    pub fn expr_raw(&self) -> &str {
        ptr_to_str(self.0.expr)
    }

    /// Returns type of the expression
    ///
    pub fn kind(&self) -> ExprType {
        self.0.kind.into()
    }

    /// Index into concatenated `FuncEntry::calls` and `FuncEntry::refcalls`
    /// list containing call information for this function for corresponding function
    /// call through member reference
    ///
    /// Available for `ExprType::Member`
    ///
    pub fn mcall(&self) -> Option<&[i64]> {
        try_ptr_to_slice(self.0.mcall, self.0.mcall_count)
    }

    /// List of member/field references
    ///
    /// `ExprType::Member` - list of member refs for the var being dereferenced
    /// `ExprType::OffsetOf` - list of field refs (or -1 for array offset expressions)
    ///
    pub fn member(&self) -> Option<&[i64]> {
        try_ptr_to_slice(self.0.member, self.0.member_count)
    }

    /// Offse value which meaning depends on expression type
    ///
    /// It is computed constant offset value in the dereference expression.
    /// It's not present in `ExprType::Member` nor in `ExprType::Return` kinds.
    ///
    /// * [`ExprType::Function`] - index into concatenated `FuncEntry::calls` and
    ///                          `FuncEntry::refcalls`
    /// * [`ExprType::Assign`] - a value that describes the kind of this assignment operator
    /// * [`ExprType::LocalParm`] - an index of the function call argument this entry
    ///                             pertains to
    /// * [`ExprType::Init`] - how many elements (in case of structure type initializer
    ///                        list) have been actually initialized
    /// * [`ExprType::OffsetOf`] - computed offset value from a given member expression
    ///                            (or -1 if the value cannot be computed statically)
    /// * [`ExprType::Cond`] - Associated compound statement id in `FunctionEntry::csmap`
    /// * [`ExprType::Logic`] - Type of logic operator
    ///
    /// Logic operator values:
    ///
    /// * `<=>` - 9
    /// * `<`   - 10
    /// * `>`   - 11
    /// * `<=`  - 12
    /// * `>=`  - 13
    /// * `==`  - 14
    /// * `!=`  - 15
    /// * `&`   - 16
    /// * `^`   - 17
    /// * `|`   - 18
    /// * `&&`  - 19
    /// * `||`  - 20
    ///
    /// Assignment operator values:
    ///
    /// * `=`   - 21
    /// * `*=`  - 22
    /// * `/=`  - 23
    /// * `%=`  - 24
    /// * `+=`  - 25
    /// * `-=`  - 26
    /// * `<<=` - 27
    /// * `>>=` - 28
    /// * `&=`  - 29
    /// * `^=`  - 30
    /// * `|=`  - 31
    ///
    pub fn offset(&self) -> Option<i64> {
        try_ptr_to_type(self.0.offset)
    }

    /// Indicates order of expressions within function
    ///
    pub fn ords(&self) -> &[u64] {
        ptr_to_slice(self.0.ord, self.0.ord_count)
    }

    /// List of variables referenced in the dereference offset expression
    ///
    pub fn offsetrefs(&self) -> Vec<OffsetRef> {
        self.offsetrefs_iter().collect()
    }

    /// Iterate over variables referenced in the dereference offset expression
    ///
    /// * [`ExprType::Array`] - the first 'basecnt' elements describe the base variable of the array
    /// * [`ExprType::Init`] - the first element describes the variable being initialized
    /// * [`ExprType::Assign`] - the first element describes the variable expression being assigned to
    ///
    pub fn offsetrefs_iter(&self) -> impl ExactSizeIterator<Item = OffsetRef> {
        ptr_to_slice(self.0.offsetrefs, self.0.offsetrefs_count)
            .iter()
            .map(|x| x.into())
    }

    /// Computed constant offset value for the corresponding member reference
    ///
    /// Available for `ExprType::Member`
    ///
    pub fn shift(&self) -> Option<&[i64]> {
        try_ptr_to_slice(self.0.shift, self.0.shift_count)
    }

    /// Type of the corresponding member reference (through outermost cast or field type)
    ///
    ///
    pub fn type_(&self) -> Option<&[TypeId]> {
        try_ptr_to_slice(self.0.type_ as *const TypeId, self.0.type_count)
    }
}

#[derive(Debug, Clone)]
pub struct OffsetRef<'a>(&'a offsetref_info);

impl<'a> From<&'a offsetref_info> for OffsetRef<'a> {
    fn from(inner: &'a offsetref_info) -> Self {
        Self(inner)
    }
}

impl<'a> OffsetRef<'a> {
    /// Type of the cast used directly on the expression for the referenced variable
    ///
    /// For the first variable expression referenced for the parent
    /// [`ExprType::Assign`] entry this is the type of the variable expression
    ///
    pub fn cast(&self) -> Option<TypeId> {
        try_ptr_to_type(self.0.cast as *const TypeId)
    }

    /// Index which meaning depends the kind
    ///
    /// * For [`ExprType::RefCallRef`]  - index into parent array with corresponding
    ///   dereference entry
    /// * For [`ExprType::AddrCallRef`]  - constant address used as a base for the
    ///   function call
    ///
    pub fn di(&self) -> Option<u64> {
        try_ptr_to_type(self.0.di)
    }

    /// Unique id of the referenced variable
    ///
    /// * For [`ExprType::Address`] or [`ExprType::Integer`] this is the integer value
    ///   extracted from original expression
    /// * For [`ExprType::CallRef`], [`ExprType::RefCallRef`] and
    ///   [`ExprType::AddrCallRef`] - index into concatenated `FuncEntry::calls` and
    ///   `FuncEntry::refcalls` entries
    /// * For [`ExprType::Unary`], [`ExprType::Array`], [`ExprType::Member`] and
    ///   [`ExprType::Assign`] - index into `FuncEntry::derefs` array for this function
    ///   that points to the appropriate entry of this kind
    /// * For [`ExprType::Function`] - this if `FunctionId` for the function reference
    ///
    ///
    pub fn id(&self) -> OffsetRefId {
        match self.kind() {
            ExprType::String => OffsetRefId::String(ptr_to_str(self.0.id_string)),
            ExprType::Floating => OffsetRefId::Float(self.0.id_float),
            _ => OffsetRefId::Integer(self.0.id_integer),
        }
    }

    /// Scope of the referenced variable
    ///
    /// * For [`ExprType::Global`], [`ExprType::Local`], and [`ExprType::LocalParm`] -
    ///   ordinary variable id
    /// * For [`ExprType::Integer`], [`ExprType::Floating`], [`ExprType::String`] and
    ///   [`ExprType::Address`] - literal values
    /// * For [`ExprType::CallRef`], [`ExprType::RefCallRef`] and
    ///   [`ExprType::AddrCallRef`] - function call information
    /// * For [`ExprType::Unary`], [`ExprType::Array`], [`ExprType::Member`],
    ///   [`ExprType::Assign`], [`ExprType::Function`] and [`ExprType::Logic`] -
    ///   variable-like expressions
    ///
    ///
    pub fn kind(&self) -> ExprType {
        self.0.kind.into()
    }

    /// Index of the member reference in the member expression chain (or offsetof
    /// array expression) this particular variable contributes to
    ///
    /// Only present in [`ExprType::Member`] and [`ExprType::OffsetOf`] kind of
    /// parent entry.
    ///
    pub fn mi(&self) -> Option<u64> {
        try_ptr_to_type(self.0.mi)
    }
}

#[derive(Debug, Clone, PartialEq, PartialOrd)]
pub enum OffsetRefId<'a> {
    String(&'a str),
    Float(f64),
    Integer(i64),
}
