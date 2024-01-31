use std::fmt::Display;

use super::{asms::*, deref::*, globalref::*, ifs::*, refcall::*, taint::*};
use crate::{
    db::{CallEntryInfo, Calls, Linkage, Literals},
    utils::{
        ptr_to_bool, ptr_to_slice, ptr_to_str, ptr_to_vec_str, try_ptr_to_str,
        try_ptr_to_type,
    },
};
use ftdb_sys::ftdb::{csitem, ftdb_func_entry, local_info};

#[derive(Debug)]
pub struct FunctionEntry<'a>(&'a ftdb_func_entry);

impl<'a> From<&'a ftdb_func_entry> for FunctionEntry<'a> {
    fn from(entry: &'a ftdb_func_entry) -> Self {
        Self(entry)
    }
}

impl<'a> Display for FunctionEntry<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "<Function: {} | {}>", self.id(), self.name())
    }
}

impl<'a> FunctionEntry<'a> {
    /// ftdb func entry asms data
    ///
    pub fn asms(&self) -> Asms<'a> {
        self.0.into()
    }

    /// ftdb func entry attributes value
    ///
    pub fn attributes(&self) -> Vec<&'a str> {
        ptr_to_vec_str(self.0.attributes, self.0.attributes_count)
    }

    /// Function body preprocessed and reformatted
    ///
    pub fn body(&self) -> &'a str {
        ptr_to_str(self.0.body)
    }

    /// Ids of other functions called within this function
    ///
    /// See also: [calls](FunctionEntry::calls)
    pub fn call_ids(&self) -> &'a [u64] {
        ptr_to_slice(self.0.calls, self.0.calls_count)
    }

    /// Function call details
    ///
    pub fn calls(&self) -> Calls<'a> {
        self.0.into()
    }

    /// ftdb func entry cshash value
    ///
    pub fn cshash(&self) -> &'a str {
        ptr_to_str(self.0.cshash)
    }

    /// ftdb func entry csmap values
    ///
    pub fn csmap(&self) -> Vec<CsItem> {
        ptr_to_slice(self.0.csmap, self.0.csmap_count)
            .iter()
            .map(|x| x.into())
            .collect()
    }

    /// ftdb func entry class value
    ///
    pub fn class(&self) -> Option<&'a str> {
        try_ptr_to_str(self.0.__class)
    }

    /// ftdb func entry classid value
    ///
    pub fn classid(&self) -> Option<u64> {
        try_ptr_to_type(self.0.classid)
    }

    /// ftdb func entry classOuterFn value
    ///
    pub fn class_outer_fn(&self) -> Option<&'a str> {
        try_ptr_to_str(self.0.classOuterFn)
    }

    /// ftdb func entry declbody value
    ///
    pub fn declbody(&self) -> &'a str {
        ptr_to_str(self.0.declbody)
    }

    /// ftdb func entry declcount value
    ///
    pub fn declcount(&self) -> u64 {
        self.0.declcount
    }

    /// ftdb func entry declhash value
    ///
    pub fn declhash(&self) -> &'a str {
        ptr_to_str(self.0.declhash)
    }

    ///
    ///
    pub fn decls(&self) -> &[u64] {
        ptr_to_slice(self.0.decls, self.0.decls_count)
    }

    ///
    ///
    pub fn derefs(&self) -> Vec<DerefInfo> {
        self.derefs_iter().collect()
    }

    ///
    ///
    pub fn derefs_iter(&self) -> impl Iterator<Item = DerefInfo> {
        ptr_to_slice(self.0.derefs, self.0.derefs_count)
            .iter()
            .map(|x| x.into())
    }

    /// ftdb func entry end_loc value
    ///
    pub fn end_loc(&self) -> &'a str {
        ptr_to_str(self.0.end_loc)
    }

    /// ftdb func entry fid value
    ///
    pub fn fid(&self) -> u64 {
        self.0.fid
    }

    /// ftdb func entry fids value
    ///
    pub fn fids(&self) -> &'a [u64] {
        ptr_to_slice(self.0.fids, self.0.fids_count)
    }

    /// ftdb func entry firstNonDeclStmt value
    ///
    pub fn first_non_decl_stmt(&self) -> &'a str {
        ptr_to_str(self.0.firstNonDeclStmt)
    }

    ///
    ///
    pub fn funrefs(&self) -> &[u64] {
        ptr_to_slice(self.0.funrefs, self.0.funrefs_count)
    }

    ///
    ///
    pub fn globalrefs(&self) -> &[u64] {
        ptr_to_slice(self.0.globalrefs, self.0.globalrefs_count)
    }

    ///
    ///
    pub fn globalref_info(&self) -> GlobalRefs {
        let refs = self.globalrefs();
        let data = ptr_to_slice(self.0.globalrefInfo, self.0.globalrefs_count);
        GlobalRefs::new(refs, data)
    }

    /// ftdb func entry hash value
    ///
    pub fn hash(&self) -> &'a str {
        ptr_to_str(self.0.hash)
    }

    /// ftdb func entry id value
    ///
    pub fn id(&self) -> u64 {
        self.0.id
    }

    ///
    ///
    pub fn ifs(&self) -> Vec<If> {
        self.ifs_iter().collect()
    }

    ///
    ///
    pub fn ifs_iter(&self) -> impl Iterator<Item = If> {
        ptr_to_slice(self.0.ifs, self.0.ifs_count)
            .iter()
            .map(|x| x.into())
    }

    /// ftdb func entry is inline value
    ///
    pub fn is_inline(&self) -> bool {
        ptr_to_bool(self.0.isinline)
    }

    /// ftdb func entry is member value
    ///
    pub fn is_member(&self) -> bool {
        ptr_to_bool(self.0.ismember)
    }

    /// ftdb func entry is template value
    ///
    pub fn is_template(&self) -> bool {
        ptr_to_bool(self.0.istemplate)
    }

    /// ftdb func entry is variadic value
    ///
    pub fn is_variadic(&self) -> bool {
        self.0.isvariadic != 0
    }

    /// ftdb func entry linkage value
    ///
    pub fn linkage(&self) -> Linkage {
        self.0.linkage.into()
    }

    /// ftdb func entry linkage string value
    ///
    pub fn linkage_string(&self) -> String {
        self.linkage().to_string()
    }

    /// ftdb func entry literals values
    ///
    pub fn literals(&self) -> Literals {
        self.0.into()
    }

    ///
    ///
    pub fn locals(&self) -> Vec<Local> {
        self.locals_iter().collect()
    }

    ///
    ///
    pub fn locals_iter(&self) -> impl Iterator<Item = Local> {
        ptr_to_slice(self.0.locals, self.0.locals_count)
            .iter()
            .map(|x| x.into())
    }

    /// Path to a file containing function
    ///
    /// This path might be an absolute path but also a relative one.
    /// It is yet unclear what is a base for relative paths. This
    /// topic needs more investigation.
    ///
    /// See also: [abs_location](FunctionEntry::abs_location)
    ///
    pub fn location(&self) -> &'a str {
        ptr_to_str(self.0.location)
    }

    /// ftdb func entry mids value
    ///
    pub fn mids(&self) -> &'a [u64] {
        ptr_to_slice(self.0.mids, self.0.mids_count)
    }

    /// ftdb func entry name value
    ///
    pub fn name(&self) -> &'a str {
        ptr_to_str(self.0.name)
    }

    /// ftdb func entry nargs value
    ///
    pub fn nargs(&self) -> u64 {
        self.0.nargs
    }

    /// ftdb func entry namespace value
    ///
    pub fn namespace(&self) -> Option<&'a str> {
        try_ptr_to_str(self.0.__namespace)
    }

    /// ftdb func entry refcount value
    ///
    pub fn refcount(&self) -> u64 {
        self.0.refcount
    }

    ///
    ///
    pub fn refcallrefs(&self) -> ! {
        todo!()
    }

    ///
    ///
    pub fn refcalls(&self) -> Vec<RefCallType> {
        self.refcalls_iter().collect()
    }

    ///
    ///
    pub fn refcalls_iter(&self) -> impl Iterator<Item = RefCallType> {
        ptr_to_slice(self.0.refcalls, self.0.refcalls_count)
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
    pub fn refcall_info(&self) -> Vec<CallEntryInfo> {
        self.refcall_info_iter().collect()
    }

    ///
    ///
    pub fn refcall_info_iter(&self) -> impl Iterator<Item = CallEntryInfo> {
        ptr_to_slice(self.0.refcall_info, self.0.refcalls_count)
            .iter()
            .map(|x| x.into())
    }

    /// ftdb func entry refs value
    ///
    pub fn refs(&self) -> &[u64] {
        ptr_to_slice(self.0.refs, self.0.refs_count)
    }

    /// ftdb func entry signature value
    ///
    pub fn signature(&self) -> &'a str {
        ptr_to_str(self.0.signature)
    }

    /// ftdb func entry start_loc value
    ///
    pub fn start_loc(&self) -> &'a str {
        ptr_to_str(self.0.start_loc)
    }

    /// ftdb func entry switches data
    ///
    pub fn switches(&self) -> ! {
        todo!()
    }

    /// ftdb func entry taint value
    ///
    pub fn taint(&self) -> Vec<Taint> {
        self.taint_iter().collect()
    }

    ///
    ///
    pub fn taint_iter(&self) -> impl Iterator<Item = Taint> {
        ptr_to_slice(self.0.taint, self.0.taint_count)
            .iter()
            .enumerate()
            .map(|(index, data)| Taint::new(index, data))
    }

    /// ftdb func entry template_parameters value
    ///
    pub fn template_parameters(&self) -> Option<&'a str> {
        try_ptr_to_str(self.0.template_parameters)
    }

    /// ftdb func entry types value
    ///
    pub fn types(&self) -> &[u64] {
        ptr_to_slice(self.0.types, self.0.types_count)
    }

    /// Unprocessed body of a function (as it is in a source file)
    ///
    pub fn unpreprocessed_body(&self) -> &'a str {
        ptr_to_str(self.0.unpreprocessed_body)
    }
}

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

pub struct Local<'a>(&'a local_info);

impl<'a> From<&'a local_info> for Local<'a> {
    fn from(inner: &'a local_info) -> Self {
        Self(inner)
    }
}

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
    pub fn location(&self) -> Option<&str> {
        try_ptr_to_str(self.0.location)
    }

    ///
    ///
    pub fn name(&self) -> &str {
        ptr_to_str(self.0.name)
    }

    ///
    ///
    pub fn type_(&self) -> u64 {
        self.0.type_
    }
}
