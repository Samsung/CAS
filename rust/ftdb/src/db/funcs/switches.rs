use std::fmt::Display;

use crate::utils::{ptr_to_slice, ptr_to_str};

#[derive(Debug, Clone)]
pub struct SwitchInfo<'a>(&'a ftdb_sys::ftdb::switch_info);

impl<'a> SwitchInfo<'a> {
    /// A code snippet representing a condition on which switch operates
    ///
    /// Example of a condition string: `sizeof (*&im_node->child[1])`
    ///
    pub fn condition(&self) -> &'a str {
        ptr_to_str(self.0.condition)
    }

    /// A list of details about each case statement of the switch
    ///
    pub fn cases(&self) -> Vec<Case> {
        self.cases_iter().collect()
    }

    /// Iterate over details about each case statement of the switch
    ///
    pub fn cases_iter(&self) -> impl ExactSizeIterator<Item = Case> {
        ptr_to_slice(self.0.cases, self.0.cases_count)
            .iter()
            .map(|ci| {
                let lhs = (&ci.lhs).into();
                match unsafe { ci.rhs.as_ref() } {
                    Some(rhs) => Case::Range(lhs, rhs.into()),
                    None => Case::Value(lhs),
                }
            })
    }
}

impl<'a> Display for SwitchInfo<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        writeln!(f, "switch ({})", self.condition())?;
        for case in self.cases_iter() {
            write!(f, "  {}", case)?;
        }
        Ok(())
    }
}

impl<'a> From<&'a ftdb_sys::ftdb::switch_info> for SwitchInfo<'a> {
    fn from(value: &'a ftdb_sys::ftdb::switch_info) -> Self {
        Self(value)
    }
}

/// Represents a single case statement
///
#[derive(Debug, Clone)]
pub enum Case<'a> {
    /// A standard case statement such as: `case 1:`
    ///
    Value(CaseData<'a>),

    /// Range case - not part of a standard but a compiler extension
    ///
    /// Example: `case 0 ... 125:`
    ///
    Range(CaseData<'a>, CaseData<'a>),
}

impl<'a> Display for Case<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Case::Value(lhs) => write!(f, "case: {}", lhs),
            Case::Range(lhs, rhs) => write!(f, "case: {} ... {}", lhs, rhs),
        }
    }
}

#[derive(Debug, Clone)]
pub struct CaseData<'a>(&'a ftdb_sys::ftdb::case_data);

impl<'a> CaseData<'a> {
    /// Value of a switch
    ///
    pub fn expr_value(&self) -> i64 {
        self.0.expr_value
    }

    /// Enum variant (if used in a case)
    ///
    pub fn enum_code(&self) -> &'a str {
        ptr_to_str(self.0.enum_code)
    }

    /// Macro text (if used in a case)
    ///
    pub fn macro_code(&self) -> &'a str {
        ptr_to_str(self.0.macro_code)
    }

    /// A raw string of what is passed to a case statement
    ///
    pub fn raw_code(&self) -> &'a str {
        ptr_to_str(self.0.raw_code)
    }
}

impl<'a> Display for CaseData<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.raw_code())
    }
}

impl<'a> From<&'a ftdb_sys::ftdb::case_data> for CaseData<'a> {
    fn from(value: &'a ftdb_sys::ftdb::case_data) -> Self {
        Self(value)
    }
}
