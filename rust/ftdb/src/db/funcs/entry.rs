use super::Location;
use crate::{
    utils::{ptr_to_str, try_ptr_to_str, try_ptr_to_type},
    TypeId,
};
use ftdb_sys::ftdb::{csitem, local_info};

pub struct CsItem<'a>(&'a csitem);

impl<'a> From<&'a csitem> for CsItem<'a> {
    fn from(inner: &'a csitem) -> Self {
        Self(inner)
    }
}

impl<'a> CsItem<'a> {
    pub fn pid(&self) -> i64 {
        self.0.pid
    }

    pub fn id(&self) -> u64 {
        self.0.id
    }

    pub fn cf(&self) -> &'a str {
        ptr_to_str(self.0.cf)
    }
}

pub struct Local<'a>(pub(crate) &'a local_info);

impl<'a> Local<'a> {
    ///
    ///
    pub fn csid(&self) -> Option<i64> {
        try_ptr_to_type(self.0.csid)
    }

    ///
    ///
    pub fn id(&self) -> u64 {
        self.0.id
    }

    ///
    ///
    pub fn is_parm(&self) -> bool {
        self.0.isparm != 0
    }

    ///
    ///
    pub fn is_static(&self) -> bool {
        self.0.isstatic != 0
    }

    ///
    ///
    pub fn is_used(&self) -> bool {
        self.0.isused != 0
    }

    ///
    ///
    pub fn location(&self) -> Option<Location<'a>> {
        try_ptr_to_str(self.0.location).map(|l| l.into())
    }

    ///
    ///
    pub fn name(&self) -> &str {
        ptr_to_str(self.0.name)
    }

    ///
    ///
    pub fn type_id(&self) -> TypeId {
        self.0.type_.into()
    }
}

macro_rules! func_decl_entry_impl {
    ($struct_name:ident) => {
        func_decl_entry_impl!($struct_name, '_);
    };

    ($struct_name:ident<$struct_life:lifetime>) => {
        func_decl_entry_impl!($struct_name<$struct_life>, $struct_life);
    };

    ($struct_name:ident$(<$struct_life:lifetime>)?, $ret_life:lifetime) => {
        use super::{deref::*, globalref::*, ifs::*, refcall::*, switches::*, taint::*, Location};
        use $crate::utils::{ptr_to_bool, ptr_to_slice, ptr_to_str, try_ptr_to_str, try_ptr_to_type};
        use $crate::{CallEntryInfo, GlobalId, FunctionId, Linkage, Literals, TypeId, CsItem, Local};


        impl$(<$struct_life>)? $struct_name $(<$struct_life>)? {
            /// ftdb func entry asms data
            ///
            pub fn asms(&self) -> Asms<$ret_life> {
                self.inner_ref().into()
            }

            /// ftdb func entry attributes value
            ///
            pub fn attributes(&self) -> Vec<&$ret_life str> {
                self.attributes_iter().collect()
            }

            pub fn attributes_iter(&self) -> impl ExactSizeIterator<Item = &$ret_life str> {
                ptr_to_slice(
                    self.inner_ref().attributes,
                    self.inner_ref().attributes_count,
                )
                .iter()
                .map(|x| ptr_to_str(*x))
            }

            /// Function body preprocessed and reformatted
            ///
            #[inline]
            pub fn body(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().body)
            }

            /// Ids of other functions called within this function
            ///
            /// See also: [calls](FtdbFunctionEntry::calls)
            #[inline]
            pub fn call_ids(&self) -> &$ret_life [FunctionId] {
                ptr_to_slice(
                    self.inner_ref().calls as *const FunctionId,
                    self.inner_ref().calls_count
                )
            }

            /// Function call details
            ///
            #[inline]
            pub fn calls(&self) -> Calls<$ret_life> {
                self.inner_ref().into()
            }
            /// Returns Compound statement hash
            ///
            /// It's a hash calculated from a function body (between '{' and '}')
            ///
            pub fn cshash(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().cshash)
            }

            /// ftdb func entry csmap values
            ///
            pub fn csmap(&self) -> Vec<CsItem> {
                self.csmap_iter().collect()
            }

            ///
            ///
            pub fn csmap_iter(&self) -> impl ExactSizeIterator<Item = CsItem> {
                ptr_to_slice(self.inner_ref().csmap, self.inner_ref().csmap_count)
                    .iter()
                    .map(|x| x.into())
            }

            /// ftdb func entry class value
            ///
            #[inline]
            pub fn class(&self) -> Option<&$ret_life str> {
                try_ptr_to_str(self.inner_ref().__class)
            }

            /// ftdb func entry classid value
            ///
            #[inline]
            pub fn classid(&self) -> Option<u64> {
                try_ptr_to_type(self.inner_ref().classid)
            }

            /// ftdb func entry classOuterFn value
            ///
            #[inline]
            pub fn class_outer_fn(&self) -> Option<&$ret_life str> {
                try_ptr_to_str(self.inner_ref().classOuterFn)
            }

            /// ftdb func entry declbody value
            ///
            #[inline]
            pub fn declbody(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().declbody)
            }

            /// ftdb func entry declcount value
            ///
            #[inline]
            pub fn declcount(&self) -> u64 {
                self.inner_ref().declcount
            }

            /// ftdb func entry declhash value
            ///
            #[inline]
            pub fn declhash(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().declhash)
            }

            ///
            ///
            #[inline]
            pub fn decls(&self) -> &[u64] {
                ptr_to_slice(self.inner_ref().decls, self.inner_ref().decls_count)
            }

            ///
            ///
            #[inline]
            pub fn derefs(&self) -> Vec<DerefInfo> {
                self.derefs_iter().collect()
            }

            ///
            ///
            #[inline]
            pub fn derefs_iter(&self) -> impl Iterator<Item = DerefInfo> {
                ptr_to_slice(self.inner_ref().derefs, self.inner_ref().derefs_count)
                    .iter()
                    .map(|x| x.into())
            }

            /// ftdb func entry end_loc value
            ///
            #[inline]
            pub fn end_loc(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().end_loc)
            }

            /// ftdb func entry fid value
            ///
            #[inline]
            pub fn fid(&self) -> u64 {
                self.inner_ref().fid
            }

            /// ftdb func entry fids value
            ///
            #[inline]
            pub fn fids(&self) -> &$ret_life [u64] {
                ptr_to_slice(self.inner_ref().fids, self.inner_ref().fids_count)
            }

            /// ftdb func entry firstNonDeclStmt value
            ///
            #[inline]
            pub fn first_non_decl_stmt(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().firstNonDeclStmt)
            }

            ///
            ///
            #[inline]
            pub fn funrefs(&self) -> &$ret_life [u64] {
                ptr_to_slice(self.inner_ref().funrefs, self.inner_ref().funrefs_count)
            }

            ///
            ///
            #[inline]
            pub fn globalrefs(&self) -> &$ret_life [GlobalId] {
                ptr_to_slice(
                    self.inner_ref().globalrefs as *const GlobalId,
                    self.inner_ref().globalrefs_count,
                )
            }

            ///
            ///
            #[inline]
            pub fn globalref_info(&self) -> GlobalRefs {
                let refs = self.globalrefs();
                let data = ptr_to_slice(
                    self.inner_ref().globalrefInfo,
                    self.inner_ref().globalrefs_count,
                );
                GlobalRefs::new(refs, data)
            }

            /// ftdb func entry hash value
            ///
            #[inline]
            pub fn hash(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().hash)
            }

            /// ftdb func entry id value
            ///
            #[inline]
            pub fn id(&self) -> FunctionId {
                self.inner_ref().id.into()
            }

            ///
            ///
            #[inline]
            pub fn ifs(&self) -> Vec<If> {
                self.ifs_iter().collect()
            }

            ///
            ///
            #[inline]
            pub fn ifs_iter(&self) -> impl ExactSizeIterator<Item = If<$ret_life>> {
                ptr_to_slice(self.inner_ref().ifs, self.inner_ref().ifs_count)
                    .iter()
                    .map(|x| x.into())
            }

            /// ftdb func entry is inline value
            ///
            #[inline]
            pub fn is_inline(&self) -> bool {
                ptr_to_bool(self.inner_ref().isinline)
            }

            /// ftdb func entry is member value
            ///
            #[inline]
            pub fn is_member(&self) -> bool {
                ptr_to_bool(self.inner_ref().ismember)
            }

            /// ftdb func entry is template value
            ///
            #[inline]
            pub fn is_template(&self) -> bool {
                ptr_to_bool(self.inner_ref().istemplate)
            }

            /// ftdb func entry is variadic value
            ///
            #[inline]
            pub fn is_variadic(&self) -> bool {
                self.inner_ref().isvariadic != 0
            }

            /// ftdb func entry linkage value
            ///
            #[inline]
            pub fn linkage(&self) -> Linkage {
                self.inner_ref().linkage.into()
            }

            /// ftdb func entry linkage string value
            ///
            #[inline]
            pub fn linkage_string(&self) -> String {
                self.linkage().to_string()
            }

            /// ftdb func entry literals values
            ///
            #[inline]
            pub fn literals(&self) -> Literals {
                self.inner_ref().into()
            }

            ///
            ///
            #[inline]
            pub fn locals(&self) -> Vec<Local> {
                self.locals_iter().collect()
            }

            ///
            ///
            #[inline]
            pub fn locals_iter(&self) -> impl ExactSizeIterator<Item = Local> {
                ptr_to_slice(self.inner_ref().locals, self.inner_ref().locals_count)
                    .iter()
                    .map(Local)
            }

            /// Absolute path to a file containing function
            ///
            #[inline]
            pub fn location(&self) -> Location<$ret_life> {
                ptr_to_str(self.inner_ref().location).into()
            }

            /// ftdb func entry mids value
            ///
            #[inline]
            pub fn mids(&self) -> &$ret_life [u64] {
                ptr_to_slice(self.inner_ref().mids, self.inner_ref().mids_count)
            }

            /// ftdb func entry name value
            ///
            #[inline]
            pub fn name(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().name)
            }

            /// ftdb func entry nargs value
            ///
            #[inline]
            pub fn nargs(&self) -> u64 {
                self.inner_ref().nargs
            }

            /// ftdb func entry namespace value
            ///
            #[inline]
            pub fn namespace(&self) -> Option<&$ret_life str> {
                try_ptr_to_str(self.inner_ref().__namespace)
            }

            /// ftdb func entry refcount value
            ///
            #[inline]
            pub fn refcount(&self) -> u64 {
                self.inner_ref().refcount
            }

            ///
            ///
            #[inline]
            pub fn refcallrefs(&self) -> ! {
                todo!()
            }

            ///
            ///
            #[inline]
            pub fn refcalls(&self) -> Vec<RefCallType> {
                self.refcalls_iter().collect()
            }

            ///
            ///
            #[inline]
            pub fn refcalls_iter(&self) -> impl ExactSizeIterator<Item = RefCallType> {
                ptr_to_slice(self.inner_ref().refcalls, self.inner_ref().refcalls_count)
                    .iter()
                    .map(|rc| {
                        if rc.ismembercall != 0 {
                            RefCallType::MemberCall(rc.fid, rc.cid, rc.field_index)
                        } else {
                            RefCallType::Call(rc.fid)
                        }
                    })
            }

            ///
            ///
            #[inline]
            pub fn refcall_info(&self) -> Vec<CallEntryInfo> {
                self.refcall_info_iter().collect()
            }

            ///
            ///
            #[inline]
            pub fn refcall_info_iter(&self) -> impl ExactSizeIterator<Item = CallEntryInfo> {
                ptr_to_slice(
                    self.inner_ref().refcall_info,
                    self.inner_ref().refcalls_count,
                )
                .iter()
                .map(|x| x.into())
            }

            /// ftdb func entry refs value
            ///
            #[inline]
            pub fn refs(&self) -> &$ret_life [u64] {
                ptr_to_slice(self.inner_ref().refs, self.inner_ref().refs_count)
            }

            /// ftdb func entry signature value
            ///
            #[inline]
            pub fn signature(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().signature)
            }

            /// ftdb func entry start_loc value
            ///
            #[inline]
            pub fn start_loc(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().start_loc)
            }

            /// ftdb func entry switches data
            ///
            #[inline]
            pub fn switches(&self) -> Vec<SwitchInfo> {
                self.switches_iter().collect()
            }

            ///
            ///
            #[inline]
            pub fn switches_iter(&self) -> impl ExactSizeIterator<Item = SwitchInfo> {
                ptr_to_slice(self.inner_ref().switches, self.inner_ref().switches_count)
                    .iter()
                    .map(|x| x.into())
            }

            /// ftdb func entry taint value
            ///
            #[inline]
            pub fn taint(&self) -> Vec<Taint> {
                self.taint_iter().collect()
            }

            ///
            ///
            #[inline]
            pub fn taint_iter(&self) -> impl ExactSizeIterator<Item = Taint> {
                ptr_to_slice(self.inner_ref().taint, self.inner_ref().taint_count)
                    .iter()
                    .enumerate()
                    .map(|(index, data)| Taint::new(index, data))
            }

            /// ftdb func entry template_parameters value
            ///
            #[inline]
            pub fn template_parameters(&self) -> Option<&$ret_life str> {
                try_ptr_to_str(self.inner_ref().template_parameters)
            }

            /// ftdb func entry types value
            ///
            #[inline]
            pub fn types(&self) -> &$ret_life [TypeId] {
                ptr_to_slice(
                    self.inner_ref().types as *const TypeId,
                    self.inner_ref().types_count
                )
            }

            /// Unprocessed body of a function (as it is in a source file)
            ///
            #[inline]
            pub fn unpreprocessed_body(&self) -> &$ret_life str {
                ptr_to_str(self.inner_ref().unpreprocessed_body)
            }
        }
    }
}

pub(crate) use func_decl_entry_impl;
