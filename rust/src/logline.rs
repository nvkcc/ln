pub struct LogLine<'a> {
    pub sha: &'a str,
    pub time: &'a str,
    pub subj: &'a str,
    pub refs: &'a str,
}

pub const SP: &str = "\u{2}";

impl<'a> From<&'a str> for LogLine<'a> {
    fn from(z: &'a str) -> Self {
        macro_rules!next{{$z:expr}=>{$z.next().unwrap()}}
        let mut z = z.split(SP);
        Self { sha: next!(z), time: next!(z), subj: next!(z), refs: next!(z) }
    }
}

impl<'a> LogLine<'a> {
    /// Get `(n, u)` where n is the number and u is the units.
    pub fn get_time(&self) -> (&'a str, char) {
        let (n, u) = self.time.split_once(' ').unwrap();
        (n, if u.starts_with("mo") { 'M' } else { u.chars().next().unwrap() })
    }

    /// Whether or not this log line contains any git refs/branches.
    pub fn has_refs(&self) -> bool {
        self.refs.len() > 3
    }
}
