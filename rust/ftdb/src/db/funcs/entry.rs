use super::Location;
use crate::{
    utils::{ptr_to_str, try_ptr_to_str, try_ptr_to_type},
    CsId, LocalId, TypeId,
};
use ftdb_sys::ftdb::{asm_info, csitem, local_info};
use std::fmt::Display;

/// Represents asm block
///
#[derive(Debug, Clone)]
pub struct AsmEntry<'a>(&'a asm_info);

impl<'a> From<&'a asm_info> for AsmEntry<'a> {
    fn from(inner: &'a asm_info) -> Self {
        Self(inner)
    }
}

impl<'a> AsmEntry<'a> {
    /// Id of a compound statement this Asm block belongs to
    ///
    pub fn csid(&self) -> CsId {
        CsId::try_from(self.0.csid).expect("csid expected >= 0")
    }

    /// Line of code in asm
    ///
    pub fn str_(&self) -> &'a str {
        ptr_to_str(self.0.str_)
    }
}

impl<'a> Display for AsmEntry<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.str_())
    }
}

/// Represents a compound statement item
///
/// To define what compound statement in FTDB is, let's see an example:
///
/// ```c
/// int func(int a) {
///     // this whole block is <compound statement 0>
///     int b = 0;      // local with csid 0;
///     if(a)
///     {
///         // inside this 'if->true' block is a <compound statement 1>
///         func(a-1);  // call with csid 1
///     }
///     else
///     {
///         // inside this else is <cs 2>
///         int c;      // local with csid 2
///         return b;   // return with csid 2
///     }
///     // [...]
/// }
/// ```
///
#[derive(Debug, Clone)]
pub struct CsItem<'a>(&'a csitem);

impl<'a> From<&'a csitem> for CsItem<'a> {
    fn from(inner: &'a csitem) -> Self {
        Self(inner)
    }
}

impl<'a> CsItem<'a> {
    /// Id of a compound statement
    ///
    pub fn id(&self) -> CsId {
        self.0.id.into()
    }

    /// Id of a parent compound statement
    ///
    pub fn pid(&self) -> Option<CsId> {
        if self.0.pid >= 0 {
            Some(CsId::from(self.0.pid as u64))
        } else {
            None
        }
    }

    /// Block name (like if, do, switch, while) or 'none' if a root block
    ///
    pub fn cf(&self) -> &'a str {
        ptr_to_str(self.0.cf)
    }
}

/// Describes function local variables
///
#[derive(Debug, Clone)]
pub struct Local<'a>(pub(crate) &'a local_info);

impl<'a> Local<'a> {
    /// Id of a compound statement this local belongs to
    ///
    /// Note that function parameters are not part of compound statements
    ///
    #[inline]
    pub fn csid(&self) -> Option<CsId> {
        try_ptr_to_type(self.0.csid)
            .map(|csid| CsId::try_from(csid).expect("csid expected to be >= 0"))
    }

    /// Id of a local variable
    ///
    #[inline]
    pub fn id(&self) -> LocalId {
        self.0.id.into()
    }

    /// Returns true if variable is a function parameter
    ///
    #[inline]
    pub fn is_parm(&self) -> bool {
        self.0.isparm != 0
    }

    /// Returns true if variable is a static local variable
    ///
    pub fn is_static(&self) -> bool {
        self.0.isstatic != 0
    }

    /// Returns true if variable is used within a function
    ///
    #[inline]
    pub fn is_used(&self) -> bool {
        self.0.isused != 0
    }

    /// Location of variable definition
    ///
    #[inline]
    pub fn location(&self) -> Option<Location<'a>> {
        try_ptr_to_str(self.0.location).map(|l| l.into())
    }

    /// Name of a variable
    ///
    #[inline]
    pub fn name(&self) -> &str {
        ptr_to_str(self.0.name)
    }

    /// Id of variable's type
    ///
    #[inline]
    pub fn type_id(&self) -> TypeId {
        self.0.type_.into()
    }
}

macro_rules! func_entry_impl {
    ($struct_name:ident) => {
        func_entry_impl!($struct_name, '_);
    };

    ($struct_name:ident<$struct_life:lifetime>) => {
        func_entry_impl!($struct_name<$struct_life>, $struct_life);
    };

    ($struct_name:ident$(<$struct_life:lifetime>)?, $ret_life:lifetime) => {

        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {

            /// List of ASM sections
            ///
            pub fn asms(&self) -> Vec<$crate::AsmEntry> {
                self.asms_iter().collect()
            }

            /// Iterate over ASM sections
            ///
            pub fn asms_iter(&self) -> impl ExactSizeIterator<Item = $crate::AsmEntry> {
                $crate::BorrowedIterator::<
                    ftdb_sys::ftdb::ftdb_func_entry,
                    $crate::AsmEntry<$ret_life>,
                    ftdb_sys::ftdb::asm_info
                >::new(self.as_inner_ref())
            }

            /// List of function attributes
            ///
            #[inline]
            pub fn attributes(&self) -> Vec<&$ret_life str> {
                self.attributes_iter().collect()
            }

            /// Iterate over function attributes
            ///
            #[inline]
            pub fn attributes_iter(&self) -> impl ExactSizeIterator<Item = &$ret_life str> {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().attributes,
                    self.as_inner_ref().attributes_count,
                )
                .iter()
                .map(|x| $crate::utils::ptr_to_str(*x))
            }

            /// Function body preprocessed and reformatted
            ///
            #[inline]
            pub fn body(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().body)
            }

            /// Ids of functions called within this function
            ///
            #[inline]
            pub fn call_ids(&self) -> &$ret_life [$crate::db::FunctionId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().calls as *const $crate::db::FunctionId,
                    self.as_inner_ref().calls_count
                )
            }

            /// Function call details
            ///
            #[inline]
            pub fn calls(&self) -> Vec<$crate::CallEntry> {
                self.calls_iter().collect()
            }

            /// Iterate over function call details
            ///
            #[inline]
            pub fn calls_iter(&self) -> impl ExactSizeIterator<Item = $crate::CallEntry> {
                $crate::db::CallEntryIterator::new(self.as_inner_ref())
            }

            /// Returns Compound statement hash
            ///
            /// It's a hash calculated from a function body (between '{' and '}')
            ///
            #[inline]
            pub fn cshash(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().cshash)
            }

            /// List of compound statements in a function
            ///
            #[inline]
            pub fn csmap(&self) -> Vec<$crate::db::CsItem> {
                self.csmap_iter().collect()
            }

            /// Iterate over compound statements in a function
            ///
            #[inline]
            pub fn csmap_iter(&self) -> impl ExactSizeIterator<Item = $crate::db::CsItem> {
                $crate::utils::ptr_to_slice(self.as_inner_ref().csmap, self.as_inner_ref().csmap_count)
                    .iter()
                    .map(|x| x.into())
            }

            /// Returns a function declaration string
            ///
            #[inline]
            pub fn declbody(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().declbody)
            }

            /// Returns number of variables declared in a function (without parameters)
            ///
            #[inline]
            pub fn declcount(&self) -> u64 {
                self.as_inner_ref().declcount
            }

            /// Returns hash of a function declaration
            ///
            #[inline]
            pub fn declhash(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().declhash)
            }

            /// List of positions in refs (indicates nested position)
            ///
            #[inline]
            pub fn decls(&self) -> &[u64] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().decls,
                    self.as_inner_ref().decls_count
                )
            }

            /// List of deref details
            ///
            #[inline]
            pub fn derefs(&self) -> Vec<$crate::db::DerefInfo> {
                self.derefs_iter().collect()
            }

            /// Iterate over deref details
            ///
            #[inline]
            pub fn derefs_iter(&self) -> impl Iterator<Item = $crate::db::DerefInfo> {
                $crate::utils::ptr_to_slice(self.as_inner_ref().derefs, self.as_inner_ref().derefs_count)
                    .iter()
                    .map(|x| x.into())
            }

            /// Returns location in a source code where function definition ends
            ///
            #[inline]
            pub fn end_loc(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().end_loc)
            }

            /// Returns id of a first file in which this function has been found
            ///
            #[inline]
            pub fn fid(&self) -> $crate::db::FileId {
                self.as_inner_ref().fid.into()
            }

            /// Return ids of all files in which this function has been found
            ///
            #[inline]
            pub fn fids(&self) -> &$ret_life [$crate::db::FileId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().fids as *const $crate::db::FileId,
                    self.as_inner_ref().fids_count
                )
            }

            /// Returns a location of a first statement that is not a declaration
            ///
            /// Note: TBV
            ///
            #[inline]
            pub fn first_non_decl_stmt(&self) -> $crate::db::Location<$ret_life> {
                $crate::utils::ptr_to_str(self.as_inner_ref().firstNonDeclStmt).into()
            }

            /// List of referenced functions
            ///
            /// Note that some of these might be functions accessible from 'funcs'
            /// section, but other might be from 'funcdecls'.
            ///
            #[inline]
            pub fn funrefs(&self) -> &$ret_life [$crate::db::FunctionId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().funrefs as *const $crate::db::FunctionId,
                    self.as_inner_ref().funrefs_count
                )
            }

            /// List of referenced global variables
            ///
            #[inline]
            pub fn globalrefs(&self) -> &$ret_life [$crate::db::GlobalId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().globalrefs as *const $crate::db::GlobalId,
                    self.as_inner_ref().globalrefs_count,
                )
            }

            /// List of reference locations from globals
            ///
            #[inline]
            pub fn globalrefs_info(&self) -> $crate::db::GlobalRefs {
                let refs = self.globalrefs();
                let data = $crate::utils::ptr_to_slice(
                    self.as_inner_ref().globalrefInfo,
                    self.as_inner_ref().globalrefs_count,
                );
                $crate::db::GlobalRefs::new(refs, data)
            }

            /// Hash of a function
            ///
            #[inline]
            pub fn hash(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().hash)
            }

            /// Identifier of a function within this database
            ///
            /// Note that the pool of `FunctionId` is shared with `funcdecl` entries
            ///
            #[inline]
            pub fn id(&self) -> $crate::db::FunctionId {
                self.as_inner_ref().id.into()
            }

            /// List of if statements within this function
            ///
            #[inline]
            pub fn ifs(&self) -> Vec<$crate::db::If> {
                self.ifs_iter().collect()
            }

            /// Iterate over if statements within this function
            ///
            #[inline]
            pub fn ifs_iter(&self) -> impl ExactSizeIterator<Item = $crate::db::If<$ret_life>> {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().ifs,
                    self.as_inner_ref().ifs_count
                )
                .iter()
                .map($crate::db::If::from)
            }

            /// Returns true if function is inline
            ///
            #[inline]
            pub fn is_inline(&self) -> bool {
                $crate::utils::ptr_to_bool(self.as_inner_ref().isinline)
            }

            /// Returns true if function has variadic parameters
            ///
            #[inline]
            pub fn is_variadic(&self) -> bool {
                self.as_inner_ref().isvariadic != 0
            }

            /// Returns type of linkage
            ///
            #[inline]
            pub fn linkage(&self) -> $crate::db::Linkage {
                self.as_inner_ref().linkage.into()
            }

            /// Provides access to literals used within a function
            ///
            #[inline]
            pub fn literals(&self) -> $crate::db::Literals {
                self.as_inner_ref().into()
            }

            /// List of function locals
            ///
            #[inline]
            pub fn locals(&self) -> Vec<$crate::db::Local> {
                self.locals_iter().collect()
            }

            /// Iterate over function locals
            ///
            /// Note that order in which local variables are iterated is not guaranteed.
            ///
            #[inline]
            pub fn locals_iter(&self) -> impl ExactSizeIterator<Item = $crate::db::Local> {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().locals,
                    self.as_inner_ref().locals_count
                )
                .iter()
                .map($crate::db::Local)
            }

            /// Find local by its id
            ///
            #[inline]
            pub fn local_by_id(&self, id: $crate::db::LocalId) -> Option<$crate::db::Local> {
                self.locals_iter().filter(|l| l.id() == id).next()
            }

            /// Absolute path to a file containing function
            ///
            #[inline]
            pub fn location(&self) -> $crate::db::Location<$ret_life> {
                $crate::utils::ptr_to_str(self.as_inner_ref().location).into()
            }

            /// List of modules with function definition
            ///
            #[inline]
            pub fn mids(&self) -> &$ret_life [$crate::db::ModuleId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().mids as *const $crate::db::ModuleId,
                    self.as_inner_ref().mids_count
                )
            }

            /// Returns function name
            ///
            #[inline]
            pub fn name(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().name)
            }

            /// Returns number of function arguments
            ///
            /// In case of variadic functions, additional arguments are not counted.
            ///
            #[inline]
            pub fn nargs(&self) -> u64 {
                self.as_inner_ref().nargs
            }

            /// Number of files in which this function is defined
            ///
            #[inline]
            pub fn refcount(&self) -> u64 {
                self.as_inner_ref().refcount
            }

            /// List of calls by reference
            ///
            #[inline]
            pub fn refcalls(&self) -> Vec<$crate::db::CallRefInfo> {
                self.refcalls_iter().collect()
            }

            /// Iterate over calls by reference
            ///
            #[inline]
            pub fn refcalls_iter(&self) -> impl ExactSizeIterator<Item = $crate::db::CallRefInfo> {
                $crate::db::RefCallsIterator::new(self.as_inner_ref())
            }

            /// List of referenced types
            ///
            #[inline]
            pub fn refs(&self) -> &$ret_life [$crate::db::TypeId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().refs as *const $crate::db::TypeId,
                    self.as_inner_ref().refs_count
                )
            }

            /// Returns function's signature
            ///
            #[inline]
            pub fn signature(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().signature)
            }

            /// Returns location in a source code where function definition begins
            ///
            #[inline]
            pub fn start_loc(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().start_loc)
            }

            /// List of switch-cases informations
            ///
            #[inline]
            pub fn switches(&self) -> Vec<$crate::db::SwitchInfo> {
                self.switches_iter().collect()
            }

            /// Iterate over switch-cases statements data if available
            ///
            /// # Examples
            ///
            /// Imagine this function:
            ///
            /// ```c
            ///
            /// enum my_enum {
            ///     CASE_1,
            ///     CASE_2 = 8,
            ///     // [...]
            /// };
            ///
            /// #define SECOND_CASE CASE_2
            ///
            /// u64 process_my_enum(enum my_enum type)
            /// {
            ///     [...]
            ///     switch (type) {
            ///     case CASE_1:
            ///         // [...]
            ///         break;
            ///     case SECOND_CASE:
            ///         // [...]
            ///         break;
            ///     case 10 ... 20:
            ///         // [...]
            ///         break;
            ///     default:
            ///         // [...]
            ///     }
            ///     // [...]
            /// }
            /// ```
            ///
            /// For FTDB representation of it, the following should be true:
            ///
            /// ```
            /// use ftdb::Case;
            ///
            /// # fn run(db: &ftdb::Ftdb) {
            /// let func = db.funcs_by_name("process_my_enum").next().unwrap();
            /// let s = func.switches_iter().next().unwrap();
            /// assert_eq!(s.condition(), "type");
            ///
            /// let mut cases = s.cases_iter();
            /// match cases.next().unwrap() {
            ///     Case::Value(lhs) => {
            ///         assert_eq!(lhs.expr_value(), 0);
            ///         assert_eq!(lhs.enum_code(), "CASE_1");
            ///         assert_eq!(lhs.macro_code(), "");
            ///         assert_eq!(lhs.raw_code(), "CASE_1");
            ///     },
            ///     _ => panic!()
            /// }
            ///
            /// match cases.next().unwrap() {
            ///     Case::Value(lhs) => {
            ///         assert_eq!(lhs.expr_value(), 8);
            ///         assert_eq!(lhs.enum_code(), "CASE_2");
            ///         assert_eq!(lhs.macro_code(), "SECOND_CASE");
            ///         assert_eq!(lhs.raw_code(), "CASE_2");
            ///     },
            ///     _ => panic!(),
            /// }
            ///
            /// match cases.next().unwrap() {
            ///     Case::Range(lhs, rhs) => {
            ///         assert_eq!(lhs.expr_value(), 10);
            ///         assert_eq!(lhs.enum_code(), "");
            ///         assert_eq!(lhs.macro_code(), "");
            ///         assert_eq!(lhs.raw_code(), "10");
            ///         assert_eq!(rhs.expr_value(), 20);
            ///         assert_eq!(rhs.enum_code(), "");
            ///         assert_eq!(rhs.macro_code(), "");
            ///         assert_eq!(rhs.raw_code(), "20");
            ///     },
            ///     _ => panic!(),
            /// }
            ///
            /// # }
            /// ```
            ///
            #[inline]
            pub fn switches_iter(&self) -> impl ExactSizeIterator<Item = $crate::db::SwitchInfo> {
                $crate::utils::ptr_to_slice(self.as_inner_ref().switches, self.as_inner_ref().switches_count)
                    .iter()
                    .map(|x| x.into())
            }

            /// Returns list of taint info for function parameters
            ///
            #[inline]
            pub fn taint(&self) -> Vec<$crate::db::Taint> {
                self.taint_iter().collect()
            }

            /// Iterate over taint info for function parameters
            ///
            #[inline]
            pub fn taint_iter(&self) -> impl ExactSizeIterator<Item = $crate::db::Taint> {
                $crate::utils::ptr_to_slice(self.as_inner_ref().taint, self.as_inner_ref().taint_count)
                    .iter()
                    .enumerate()
                    .map(|(index, data)| $crate::db::Taint::new(index.into(), data))
            }

            /// Id of types used in the function signature
            ///
            /// Number of entries is equal to `nargs() + 1`.
            /// First element represent returned type. Other elements are types of
            /// function parameters.
            ///
            #[inline]
            pub fn types(&self) -> &$ret_life [$crate::db::TypeId] {
                $crate::utils::ptr_to_slice(
                    self.as_inner_ref().types as *const $crate::db::TypeId,
                    self.as_inner_ref().types_count
                )
            }

            /// Body of a function before preprocessing (as it is in a source file)
            ///
            #[inline]
            pub fn unpreprocessed_body(&self) -> &$ret_life str {
                $crate::utils::ptr_to_str(self.as_inner_ref().unpreprocessed_body)
            }
        }
    }

}

pub(crate) use func_entry_impl;
