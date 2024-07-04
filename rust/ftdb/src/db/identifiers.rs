use crate::macros::impl_inner_display;

/// Unique function identifier
///
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone, Copy, Hash)]
#[repr(transparent)]
pub struct FunctionId(pub u64);

impl_inner_display!(FunctionId);

impl From<u64> for FunctionId {
    fn from(value: u64) -> Self {
        FunctionId(value)
    }
}

impl From<&u64> for FunctionId {
    fn from(value: &u64) -> Self {
        FunctionId(*value)
    }
}

/// Unique global identifier
///
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone, Copy, Hash)]
#[repr(transparent)]
pub struct GlobalId(pub u64);

impl_inner_display!(GlobalId);

impl From<u64> for GlobalId {
    fn from(value: u64) -> Self {
        GlobalId(value)
    }
}

impl From<&u64> for GlobalId {
    fn from(value: &u64) -> Self {
        GlobalId(*value)
    }
}

/// Unique Type identifier
///
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone, Copy, Hash)]
#[repr(transparent)]
pub struct TypeId(pub u64);

impl_inner_display!(TypeId);

impl From<u64> for TypeId {
    fn from(value: u64) -> Self {
        TypeId(value)
    }
}

impl From<&u64> for TypeId {
    fn from(value: &u64) -> Self {
        TypeId(*value)
    }
}
