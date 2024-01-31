use std::ops::Deref;
pub struct Sources<'a>(Vec<&'a str>);

impl<'a> Deref for Sources<'a> {
    type Target = Vec<&'a str>;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl<'a> From<Vec<&'a str>> for Sources<'a> {
    fn from(inner: Vec<&'a str>) -> Self {
        Self(inner)
    }
}
