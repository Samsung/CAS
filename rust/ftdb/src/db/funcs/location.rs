/// Wraps raw string that represents a file location
///
/// A syntax of a location is:
///
/// ```rust
/// let x = "/media/storage/dir/file.cpp:1:2";
/// ````
///
/// Where 1:2 means line:index.
///
#[derive(Debug, PartialEq, Eq, PartialOrd, Ord, Clone, Copy, Hash)]
pub struct Location<'a>(&'a str);

impl<'a> Location<'a> {
    /// Get file portion of a location
    ///
    #[inline]
    pub fn file(&self) -> &'a str {
        let f = self.file_with_pos();
        let index = f.find(':').unwrap_or(self.0.len());
        &f[..index]
    }

    /// Get file + directory part of a location
    ///
    #[inline]
    pub fn filepath(&self) -> &'a str {
        let index = self.0.find(':').unwrap_or(self.0.len());
        &self.0[..index]
    }

    /// Get file portion of a location with position (line, column)
    ///
    #[inline]
    pub fn file_with_pos(&self) -> &'a str {
        let index = self.0.rfind('/').map(|index| index + 1).unwrap_or_default();
        &self.0[index..]
    }

    /// Get parent directory
    ///
    #[inline]
    pub fn parent(&self) -> &'a str {
        let index = self.0.rfind('/').unwrap_or_default();
        &self.0[..index]
    }

    /// Get line number from a Location if possible
    ///
    #[inline]
    pub fn linenum(&self) -> Option<usize> {
        self.0.find(':').and_then(|left_index| {
            let index = left_index + 1;
            let right_index = index + self.0[index..].find(':').unwrap_or(self.0[index..].len());
            let value = &self.0[index..right_index];
            value.parse().ok()
        })
    }
}

impl<'a> From<&'a str> for Location<'a> {
    fn from(value: &'a str) -> Self {
        Self(value)
    }
}

impl<'a> std::fmt::Display for Location<'a> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use rstest::rstest;

    #[rstest]
    #[case::full_path("/opt/aosp/kernel/config.h:100:50", "config.h")]
    #[case::no_parent("config.h:100:50", "config.h")]
    #[case::no_parent_no_pos("config.h", "config.h")]
    #[case::empty("", "")]
    fn file(#[case] input: &str, #[case] expected: &str) {
        let loc = Location::from(input);
        assert_eq!(expected, loc.file());
    }

    #[rstest]
    #[case::full_path("/opt/aosp/kernel/config.h:100:50", "/opt/aosp/kernel/config.h")]
    #[case::no_parent("config.h:100:50", "config.h")]
    #[case::no_parent_no_pos("config.h", "config.h")]
    #[case::empty("", "")]
    fn filepath(#[case] input: &str, #[case] expected: &str) {
        let loc = Location::from(input);
        assert_eq!(expected, loc.filepath());
    }

    #[rstest]
    #[case::full_path("/opt/aosp/kernel/config.h:100:50", "config.h:100:50")]
    #[case::no_parent("config.h:100:50", "config.h:100:50")]
    #[case::no_parent_no_pos("config.h", "config.h")]
    #[case::empty("", "")]
    fn file_with_pos(#[case] input: &str, #[case] expected: &str) {
        let loc = Location::from(input);
        assert_eq!(expected, loc.file_with_pos());
    }

    #[rstest]
    #[case::full_path("/opt/aosp/kernel/config.h:100:50", "/opt/aosp/kernel")]
    #[case::no_parent("config.h:100:50", "")]
    #[case::no_parent_no_pos("config.h", "")]
    #[case::empty("", "")]
    fn parent(#[case] input: &str, #[case] expected: &str) {
        let loc = Location::from(input);
        assert_eq!(expected, loc.parent());
    }

    #[rstest]
    #[case::full_path("/opt/aosp/kernel/config.h:100:50", Some(100))]
    #[case::no_parent("config.h:23:1", Some(23))]
    #[case::no_parent("config.h:25", Some(25))]
    #[case::no_parent_no_pos("config.h", None)]
    #[case::empty("", None)]
    fn linenum(#[case] input: &str, #[case] expected: Option<usize>) {
        let loc = Location::from(input);
        assert_eq!(expected, loc.linenum());
    }
}
