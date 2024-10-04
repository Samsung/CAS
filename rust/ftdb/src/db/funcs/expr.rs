use crate::{FunctionId, GlobalId, LocalId, Location};

#[derive(Debug, Clone, PartialEq, PartialOrd)]
pub enum ExprData<'a> {
    /// a variable (local, global or other's function param)
    LocalVar(LocalId),

    /// a global variable
    GlobalVar(GlobalId),

    /// a function
    FunctionPtrVar(FunctionId),

    /// an integer literal
    IntegerLiteral(i64),

    /// a character literal
    CharacterLiteral(i32),

    /// a float literal
    FloatingLiteral(f64),

    /// a string literal
    StringLiteral(&'a str),

    /// Unknown, unsupported or unimplemented
    None,
}

pub struct Expr<'a>(&'a str);

impl<'a> TryFrom<&'a str> for Expr<'a> {
    type Error = &'static str;

    fn try_from(value: &'a str) -> Result<Self, Self::Error> {
        if value.is_empty() {
            return Err("expr is empty");
        }
        if value.chars().nth(0) != Some('[') {
            return Err("expr must start with [");
        }
        if !value.contains("]") {
            return Err("expr must contain closing ]");
        }
        let bracket = value.find("]").unwrap();
        let sub = &value[bracket..];
        if sub.len() <= 3 {
            return Err("expr not in valid format: [/path]: expr");
        }
        Ok(Expr(value))
    }
}

impl<'a> Expr<'a> {
    /// Create an Expr instance wrapping raw string
    ///
    /// Note that in debug mode this function runs series of string format validations,
    /// so issues related to the expr string might be detected earlier.
    ///
    /// In release mode validation is not run
    ///
    pub fn new(input: &'a str) -> Expr<'a> {
        if cfg!(debug_assertions) {
            match input.try_into() {
                Ok(expr) => expr,
                Err(reason) => panic!("error: {reason}"),
            }
        } else {
            Expr(input)
        }
    }

    /// Extract location part of the expr string
    ///
    /// Seek examples in test cases in the source file
    ///
    pub fn location(&self) -> Location<'a> {
        let index = self.0.find(']').expect("invalid expr string format");
        let loc = &self.0[1..index];
        loc.into()
    }

    /// Returns expression part of the expr string
    ///
    /// Seek examples in test cases in the source file
    ///
    pub fn expr(&self) -> &'a str {
        let index = self.0.find(']').expect("invalid expr string format");
        &self.0[index + 3..]
    }
}

#[cfg(test)]
mod tests {
    use rstest::rstest;

    use super::*;

    #[test]
    fn location() {
        let input = "[/media/storage/source_code/path:1:42]: foo->bar";
        let expected: Location = "/media/storage/source_code/path:1:42".into();
        let expr: Expr = input.try_into().unwrap();
        assert_eq!(expected, expr.location());
    }

    #[test]
    fn expr() {
        let input = "[/media/storage/source_code/path:1:42]: foo->bar";
        let expected = "foo->bar";
        let expr: Expr = input.try_into().unwrap();
        assert_eq!(expected, expr.expr());
    }

    #[rstest]
    #[case::no_location("foo->bar")]
    #[case::missing_brackets("/media/storage/something:1.42: foo->bar")]
    #[case::only_location("/media/storage/something:1.42")]
    #[case::missing_expr("[/media/storage/something:1.42]: ")]
    fn invalid_inputs(#[case] input: &str) {
        let expr: Result<Expr, _> = input.try_into();
        assert!(expr.is_err(), "should not be parsed");
    }
}
