use super::utils::{ptr_to_slice, ptr_to_str};
use std::fmt::Display;

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum ExprType {
    Function,
    Global,
    Local,
    LocalParm,
    StringLiteral,
    CharLiteral,
    IntegerLiteral,
    FloatLiteral,
    CallRef,
    RefCallRef,
    AddrCallRef,
    Address,
    Integer,
    Floating,
    String,
    Unary,
    Array,
    Member,
    Assign,
    OffsetOf,
    Init,
    Return,
    Cond,
    Logic,
    Undef,
}

impl From<ftdb_sys::ftdb::exprType> for ExprType {
    fn from(src: ftdb_sys::ftdb::exprType) -> Self {
        match src {
            ftdb_sys::ftdb::exprType::EXPR_FUNCTION => Self::Function,
            ftdb_sys::ftdb::exprType::EXPR_GLOBAL => Self::Global,
            ftdb_sys::ftdb::exprType::EXPR_LOCAL => Self::Local,
            ftdb_sys::ftdb::exprType::EXPR_LOCALPARM => Self::LocalParm,
            ftdb_sys::ftdb::exprType::EXPR_STRINGLITERAL => Self::StringLiteral,
            ftdb_sys::ftdb::exprType::EXPR_CHARLITERAL => Self::CharLiteral,
            ftdb_sys::ftdb::exprType::EXPR_INTEGERLITERAL => Self::IntegerLiteral,
            ftdb_sys::ftdb::exprType::EXPR_FLOATLITERAL => Self::FloatLiteral,
            ftdb_sys::ftdb::exprType::EXPR_CALLREF => Self::CallRef,
            ftdb_sys::ftdb::exprType::EXPR_REFCALLREF => Self::RefCallRef,
            ftdb_sys::ftdb::exprType::EXPR_ADDRCALLREF => Self::AddrCallRef,
            ftdb_sys::ftdb::exprType::EXPR_ADDRESS => Self::Address,
            ftdb_sys::ftdb::exprType::EXPR_INTEGER => Self::Integer,
            ftdb_sys::ftdb::exprType::EXPR_FLOATING => Self::Floating,
            ftdb_sys::ftdb::exprType::EXPR_STRING => Self::String,
            ftdb_sys::ftdb::exprType::EXPR_UNARY => Self::Unary,
            ftdb_sys::ftdb::exprType::EXPR_ARRAY => Self::Array,
            ftdb_sys::ftdb::exprType::EXPR_MEMBER => Self::Member,
            ftdb_sys::ftdb::exprType::EXPR_ASSIGN => Self::Assign,
            ftdb_sys::ftdb::exprType::EXPR_OFFSETOF => Self::OffsetOf,
            ftdb_sys::ftdb::exprType::EXPR_INIT => Self::Init,
            ftdb_sys::ftdb::exprType::EXPR_RETURN => Self::Return,
            ftdb_sys::ftdb::exprType::EXPR_COND => Self::Cond,
            ftdb_sys::ftdb::exprType::EXPR_LOGIC => Self::Logic,
            _ => Self::Undef,
        }
    }
}

impl Display for ExprType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let value = match self {
            ExprType::Function => "function",
            ExprType::Global => "global",
            ExprType::Local => "local",
            ExprType::LocalParm => "parm",
            ExprType::StringLiteral => "string_literal",
            ExprType::CharLiteral => "char_literal",
            ExprType::IntegerLiteral => "interger_literal",
            ExprType::FloatLiteral => "float_literal",
            ExprType::CallRef => "callref",
            ExprType::RefCallRef => "refcallref",
            ExprType::AddrCallRef => "addrcallref",
            ExprType::Address => "address",
            ExprType::Integer => "integer",
            ExprType::Floating => "float",
            ExprType::String => "string",
            ExprType::Unary => "unary",
            ExprType::Array => "array",
            ExprType::Member => "member",
            ExprType::Assign => "assign",
            ExprType::OffsetOf => "offsetof",
            ExprType::Init => "init",
            ExprType::Return => "return",
            ExprType::Cond => "cond",
            ExprType::Logic => "logic",
            ExprType::Undef => "undef",
        };
        write!(f, "{}", value)
    }
}

/// Represents a linkage type
///
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum Linkage {
    /// Refers to a translation unit scope
    Internal,

    /// Refers to combination of all translation units
    External,

    /// Unknown or unsupported linkage type
    Unknown,
}

impl From<ftdb_sys::ftdb::functionLinkage> for Linkage {
    fn from(raw: ::ftdb_sys::ftdb::functionLinkage) -> Self {
        match raw {
            ftdb_sys::ftdb::functionLinkage::LINKAGE_INTERNAL => Self::Internal,
            ftdb_sys::ftdb::functionLinkage::LINKAGE_EXTERNAL => Self::External,
            _ => Self::Unknown,
        }
    }
}

impl Display for Linkage {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let repr = match self {
            Linkage::Internal => "internal",
            Linkage::External => "external",
            Linkage::Unknown => "unknown",
        };
        write!(f, "{}", repr)
    }
}

/// Set of literals group by types
///
#[derive(Debug, Clone, PartialEq, PartialOrd)]
pub struct Literals<'a> {
    pub character: &'a [u32],
    pub floating: &'a [f64],
    pub integer: &'a [i64],
    pub string: Vec<&'a str>,
}

impl<'a> From<&'a ftdb_sys::ftdb::ftdb_global_entry> for Literals<'a> {
    fn from(src: &'a ftdb_sys::ftdb::ftdb_global_entry) -> Self {
        Literals {
            character: ptr_to_slice(src.character_literals, src.character_literals_count),
            floating: ptr_to_slice(src.floating_literals, src.floating_literals_count),
            integer: ptr_to_slice(src.integer_literals, src.integer_literals_count),
            string: ptr_to_slice(src.string_literals, src.string_literals_count)
                .iter()
                .map(|ptr| ptr_to_str(*ptr))
                .collect(),
        }
    }
}

impl<'a> From<&'a ftdb_sys::ftdb::ftdb_func_entry> for Literals<'a> {
    fn from(src: &'a ftdb_sys::ftdb::ftdb_func_entry) -> Self {
        Literals {
            character: ptr_to_slice(src.character_literals, src.character_literals_count),
            floating: ptr_to_slice(src.floating_literals, src.floating_literals_count),
            integer: ptr_to_slice(src.integer_literals, src.integer_literals_count),
            string: ptr_to_slice(src.string_literals, src.string_literals_count)
                .iter()
                .map(|ptr| ptr_to_str(*ptr))
                .collect(),
        }
    }
}

impl<'a> Display for Literals<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "<Literals: {} chars, {} floats, {} ints, {} strs>",
            self.character.len(),
            self.floating.len(),
            self.integer.len(),
            self.string.len()
        )
    }
}

/// Class of type
///
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum TypeClass {
    EmptyRecord,

    /// A builtin type
    ///
    /// Examples:
    ///
    /// ```c
    /// int a;
    /// char b;
    /// long c;
    /// ```
    Builtin,

    /// A pointer type
    ///
    /// Examples:
    ///
    ///```c
    /// const char* ptr;
    /// ```
    Pointer,

    MemberPointer,
    Attributed,
    Complex,
    Vector,
    ExtendedVector,
    DependentSizedArray,
    PackExpansion,
    UnresolvedUsing,
    Auto,
    TemplateTypeParm,
    DependentName,
    SubstTemplateTypeParm,
    RecordSpecialization,
    RecordTemplate,

    /// A 'struct' type
    ///
    /// Examples:
    ///
    /// ```c
    /// struct color {
    ///     uint8_t r;
    ///     uint8_t g;
    ///     uint8_t b;
    /// };
    /// ```
    Record,

    /// Struct forward declaration (without definition)
    ///
    /// ```c
    /// struct color;
    /// ```
    ///
    RecordForward,

    /// A static array (of known size)
    ConstArray,

    /// An incomplete array type
    IncompleteArray,

    /// Variable Length Array (VLA) type
    VariableArray,

    /// A type definition
    Typedef,

    /// An enumeration
    ///
    /// Examples:
    ///
    /// ```c
    /// enum level {
    ///     LOW
    ///     MEDIUM,
    ///     HIGH,
    /// };
    /// ```
    Enum,

    /// Enum forward declaration
    ///
    /// ```c
    /// enum class level;
    /// ```
    EnumForward,

    /// A pointer which is decayed array type
    DecayedPointer,

    /// A pointer to function
    ///
    /// Examples:
    ///
    /// ```c
    /// int (struct severity *, unsigned long)
    /// ```
    Function,

    /// Other types
    Undef,
}

impl From<ftdb_sys::ftdb::TypeClass> for TypeClass {
    fn from(src: ftdb_sys::ftdb::TypeClass) -> Self {
        match src {
            ftdb_sys::ftdb::TypeClass::TYPECLASS_EMPTYRECORD => Self::EmptyRecord,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_BUILTIN => Self::Builtin,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_POINTER => Self::Pointer,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_MEMBERPOINTER => Self::MemberPointer,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_ATTRIBUTED => Self::Attributed,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_COMPLEX => Self::Complex,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_VECTOR => Self::Vector,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_EXTENDEDVECTOR => Self::ExtendedVector,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_DEPENDENT_SIZED_ARRAY => Self::DependentSizedArray,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_PACKEXPANSION => Self::PackExpansion,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_UNRESOLVEDUSING => Self::UnresolvedUsing,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_AUTO => Self::Auto,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_TEMPLATETYPEPARM => Self::TemplateTypeParm,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_DEPENDENTNAME => Self::DependentName,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_SUBSTTEMPLATETYPEPARM => {
                Self::SubstTemplateTypeParm
            }
            ftdb_sys::ftdb::TypeClass::TYPECLASS_RECORDSPECIALIZATION => Self::RecordSpecialization,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_RECORDTEMPLATE => Self::RecordTemplate,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_RECORD => Self::Record,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_RECORDFORWARD => Self::RecordForward,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_CONSTARRAY => Self::ConstArray,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_INCOMPLETEARRAY => Self::IncompleteArray,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_VARIABLEARRAY => Self::VariableArray,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_TYPEDEF => Self::Typedef,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_ENUM => Self::Enum,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_ENUMFORWARD => Self::EnumForward,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_DECAYEDPOINTER => Self::DecayedPointer,
            ftdb_sys::ftdb::TypeClass::TYPECLASS_FUNCTION => Self::Function,

            _ => Self::Undef,
        }
    }
}
