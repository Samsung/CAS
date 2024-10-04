#[derive(Debug, serde::Serialize, serde::Deserialize)]
pub struct Specs(pub Vec<StructSpec>);

pub const BUILTIN_VMLINUX_SPECS: &str = include_str!("../../../specs/vmlinux.json");

impl Specs {
    pub fn from_builtin<T: AsRef<str>>(name: T) -> Option<Self> {
        let data = match name.as_ref() {
            "vmlinux" => BUILTIN_VMLINUX_SPECS,
            _ => return None,
        };
        serde_json::from_str(data).ok()
    }
}

/// Structure specification
///
/// We are interested in analysis of structures storing function pointers.
/// These might be handlers to various syscalls, such as read, write or ioctl.
///
/// These interesting structures consists of members, which are pointers
/// to functions. Assignement between actual functions and structure members
/// is described in FTDB, in field "fops" of a root document.
///
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize, PartialEq, Eq)]
pub struct StructSpec {
    /// Name of a type (struct)
    pub name: String,

    /// List of member specification for the type
    pub members: Vec<MemberSpec>,
}

/// Specification for a single member
///
/// This is description of specific member of specific type. It provides
/// infomration about assigned category or which arguments of a function
/// are controlled by the user.
///
#[derive(Debug, Clone, serde::Serialize, serde::Deserialize, PartialEq, Eq)]
pub struct MemberSpec {
    /// Member name
    pub name: String,

    /// Assigned category
    pub category: String,

    /// Indices of arguments controlled by the user
    ///
    /// Negative values mean "that many arguments from the end". So "-1" means
    /// index of the last argument.
    ///
    pub user: Option<Vec<i64>>,
}
