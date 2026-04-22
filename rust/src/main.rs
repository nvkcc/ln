mod cmd;
mod logline;
mod vlist;

use logline::*;
use vlist::*;

use std::io::{stdout, BufRead, BufReader, LineWriter, Write};
use std::process::{ChildStdout, Command, Stdio};

const HEIGHT_RATIO: f32 = 0.7;

/// Light Gray.
const L: &str = "\x1b[38;5;246m";

/// Dark Gray.
const D: &str = "\x1b[38;5;240m";

/// Green.
const G: &str = "\x1b[32m";

/// Yellow.
const Y: &str = "\x1b[33m";

/// Reset.
const R: &str = "\x1b[m";

macro_rules! _write { ($f:expr, $($x:tt)+) => {{ let _ = std::write!($f, $($x)*); }}}

/// Prints one line in the `git log` output.
fn print_git_log_line<W: Write>(line: &str, mut f: W, vlist: &mut VList) {
    let ll = match line.split_once(SP) {
        Some((graph, line)) => {
            _write!(f, "{graph}");
            LogLine::from(line)
        }
        // entire line is just the graph visual.
        None => return (_ = writeln!(f, "{line}")),
    };

    // Write the SHA.
    if vlist.contains(ll.sha) {
        _write!(f, "{G}{}", ll.sha);
    } else {
        _write!(f, "{Y}{}", ll.sha);
    }

    // Write the refs, if they exist
    if ll.has_refs() {
        _write!(f, " {D}{{{}{D}}}", ll.refs)
    }

    // Write the subject (commit message) and the timestamp.
    let (n, u) = ll.get_time();
    let _ = writeln!(f, " {R}{} {D}({L}{n}{u}{D}){R}", ll.subj);
}

// Gets the upper bound on number of lines to print on a bounded run.
fn get_line_limit() -> u32 {
    let (_, lines) = crossterm::terminal::size().unwrap();
    (lines as f32 * HEIGHT_RATIO) as u32
}

/// Iterates over the git log and writes the outputs to `f`.
fn run<W: Write>(is_bounded: bool, log: ChildStdout, mut target: W) {
    let mut buffer = String::with_capacity(256);
    let mut limit = if is_bounded { get_line_limit() } else { u32::MAX };

    let vlist_raw = VList::raw();
    let mut vlist = VList::new(vlist_raw.as_ref().map(|v| v.as_str()));

    let mut log = BufReader::new(log);
    while limit > 0 {
        buffer.clear();
        let line = match log.read_line(&mut buffer) {
            Ok(0) | Err(_) => break,
            _ => buffer.trim_end(),
        };
        print_git_log_line(line, &mut target, &mut vlist);
        limit -= 1;
    }
}

fn parse_cli() -> (Command, bool) {
    let mut git_log = cmd::git_log();
    git_log.stdout(Stdio::piped());

    let mut is_bounded = false;
    for arg in std::env::args_os().skip(1) {
        if arg == "--bound" {
            is_bounded = true;
            continue;
        }
        git_log.arg(arg);
    }

    (git_log, is_bounded)
}

/// Here, we operate under the assumption that we ARE using this in a
/// tty context, and hence always have color on.
fn main() {
    let (mut git_log, is_bounded) = parse_cli();

    let mut git_log_p = git_log.spawn().unwrap(); // process
    let git_log_stdout = git_log_p.stdout.take().unwrap(); // stdout

    match cmd::less().spawn() {
        Ok(mut less) => {
            // `less` found: pass the git log output to less.
            let less_stdin = less.stdin.take().unwrap();
            run(is_bounded, git_log_stdout, LineWriter::new(less_stdin));
            let _ = less.wait();
        }
        Err(_) => {
            // `less` not found: just run normal git log and print to stdout.
            run(is_bounded, git_log_stdout, LineWriter::new(stdout()));
        }
    }
}
