//! Verified list of SHAs.

use crate::cmd::git_dir;

use std::collections::HashSet;
use std::path::Path;

/// Verified list of SHAs.
pub struct VList<'a> {
    set: Option<HashSet<&'a str>>,
    len: usize,
}

impl<'a> VList<'a> {
    /// Create a new verified list of SHAs from lines of text. Each line should
    /// be a SHA.
    pub fn new(text: Option<&'a str>) -> Self {
        Self { set: text.map(|v| v.lines().collect()), len: 40 }
    }

    pub fn raw() -> Option<String> {
        let _out = git_dir().output().ok()?;
        let _str = core::str::from_utf8(&_out.stdout).ok()?;
        let v_file = Path::new(_str).join(".verified");
        std::fs::read_to_string(v_file).ok()
    }

    pub fn contains(&mut self, sha: &str) -> bool {
        let Some(set) = self.set.as_mut() else { return false };
        let len = sha.len();
        // Truncate the SHAs to fit the displayed length.
        if self.len != len {
            let mut buf = Vec::with_capacity(set.len());
            buf.extend(set.drain());
            buf.iter_mut().for_each(|v| *v = &v[..len]);
            set.extend(buf);
            self.len = len;
        }
        set.contains(sha)
    }
}
