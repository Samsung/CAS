use std::ops::Deref;
pub struct Modules<'a>(Vec<&'a str>);

impl<'a> Deref for Modules<'a> {
    type Target = Vec<&'a str>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<'a> From<Vec<&'a str>> for Modules<'a> {
    fn from(inner: Vec<&'a str>) -> Self {
        Self(inner)
    }
}
