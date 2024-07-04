use crate::utils::{ptr_to_slice, ptr_to_str};

pub struct SwitchInfo<'a>(&'a ftdb_sys::ftdb::switch_info);

impl<'a> SwitchInfo<'a> {
    ///
    ///
    pub fn condition(&self) -> &'a str {
        ptr_to_str(self.0.condition)
    }

    ///
    ///
    pub fn cases(&self) -> Vec<CaseInfo> {
        self.cases_iter().collect()
    }

    ///
    ///
    pub fn cases_iter(&self) -> impl ExactSizeIterator<Item = CaseInfo> {
        ptr_to_slice(self.0.cases, self.0.cases_count)
            .iter()
            .map(|x| x.into())
    }
}

impl<'a> From<&'a ftdb_sys::ftdb::switch_info> for SwitchInfo<'a> {
    fn from(value: &'a ftdb_sys::ftdb::switch_info) -> Self {
        Self(value)
    }
}

pub struct CaseInfo<'a>(&'a ftdb_sys::ftdb::case_info);

impl<'a> CaseInfo<'a> {
    ///
    ///
    pub fn lhs(&self) -> CaseData<'a> {
        CaseData(&self.0.lhs)
    }

    pub fn rhs(&self) -> Option<CaseData<'a>> {
        let rhs = unsafe { self.0.rhs.as_ref() };
        rhs.map(|x| x.into())
    }
}

impl<'a> From<&'a ftdb_sys::ftdb::case_info> for CaseInfo<'a> {
    fn from(value: &'a ftdb_sys::ftdb::case_info) -> Self {
        Self(value)
    }
}

pub struct CaseData<'a>(&'a ftdb_sys::ftdb::case_data);

impl<'a> CaseData<'a> {
    ///
    ///
    pub fn expr_value(&self) -> i64 {
        self.0.expr_value
    }

    ///
    ///
    pub fn enum_code(&self) -> &'a str {
        ptr_to_str(self.0.enum_code)
    }

    ///
    ///
    pub fn macro_code(&self) -> &'a str {
        ptr_to_str(self.0.macro_code)
    }

    ///
    ///
    pub fn raw_code(&self) -> &'a str {
        ptr_to_str(self.0.raw_code)
    }
}

impl<'a> From<&'a ftdb_sys::ftdb::case_data> for CaseData<'a> {
    fn from(value: &'a ftdb_sys::ftdb::case_data) -> Self {
        Self(value)
    }
}
