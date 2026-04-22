use std::io;
use std::io::Write;
use std::process::Command;
use std::time::{Duration, Instant};

const TEST_DIR: &str = "/Users/khang/repos/math";

fn run() -> io::Result<()> {
    let mut cmd = Command::new("git");
    cmd.args(["-C", TEST_DIR, "ln", "-n", "40"]);
    let mut proc = cmd.spawn()?;
    proc.wait()?;
    Ok(())
}

fn print_avg_time<W: Write>(t: &[Duration], f: &mut W) {
    let z = t.iter().fold(Duration::ZERO, |a, v| a + *v);
    let z = z / t.len() as u32;
    writeln!(f, "{z:?}").unwrap();
}

fn main() {
    let warmup = 200;
    let actual = 200;

    let mut f = std::fs::File::create("log.txt").unwrap();

    let mut timings = vec![];

    for _ in 0..warmup {
        let _start = Instant::now();
        run().unwrap();
        timings.push(_start.elapsed());
    }
    print_avg_time(&timings, &mut f);
    timings.clear();

    for _ in 0..actual {
        let _start = Instant::now();
        run().unwrap();
        timings.push(_start.elapsed());
    }
    print_avg_time(&timings, &mut f);
}
