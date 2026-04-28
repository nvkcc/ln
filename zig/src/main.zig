const std = @import("std");
const ln = @import("ln");
const mem = std.mem;

const App = @import("app.zig");
var fixed_buffer: [0x1000]u8 = undefined;
var fba: std.heap.FixedBufferAllocator = .init(&fixed_buffer);
const allocator = fba.allocator();

pub const std_options: std.Options = .{
    .log_level = .debug,
    .logFn = @import("logger.zig").customLog,
};

pub fn main_inner() !u8 {
    const app: App = App.init();

    const argv_gl = try app.git_log_args(allocator);
    // for (0..argv_gl.len) |i| { std.log.info("[{d}] = {s}\x1b[m", .{ i, argv_gl[i] }); }

    // If we're not even in a TTY, then don't bother with a pager.
    if (!app.is_atty) {
        var gl = std.process.Child.init(argv_gl, allocator);
        try gl.spawn();
        return (try gl.wait()).Exited;
    }

    // Spawn `less` first to see if it exists.
    const argv_ls = try app.less_args(allocator);
    var proc_ls = std.process.Child.init(argv_ls, allocator);
    proc_ls.stdin_behavior = .Pipe;
    try proc_ls.spawn();

    // Prepare the `git log` child process, but don't spawn yet. Whether or not
    // we pipe it depends on if `less` went well.
    var proc_gl = std.process.Child.init(argv_gl, allocator);

    // If `less` is not installed, then just run a full bypass to git log.
    proc_ls.waitForSpawn() catch |err| switch (err) {
        error.FileNotFound => {
            std.log.warn("`less` is not installed.\n", .{});
            try proc_gl.spawn();
            const term = try proc_gl.wait();
            return term.Exited;
        },
        else => return err,
    };

    proc_gl.stdout_behavior = .Pipe;
    try proc_gl.spawn();

    // This should be safe because we already set the stdin_behavior above.
    const proc_ls_stdin = proc_ls.stdin orelse unreachable;
    proc_ls.stdin = null; // Move the value out, a la Rust's Option::take.

    const read_buf = try allocator.alloc(u8, 0x400);
    const write_buf = try allocator.alloc(u8, 0x400);
    var f_reader = proc_gl.stdout.?.reader(read_buf);
    var reader = &f_reader.interface;
    var output = proc_ls_stdin.writer(write_buf);
    loop: while (true) {
        var line = reader.takeDelimiterInclusive('\n') catch |e| switch (e) {
            error.EndOfStream => break :loop,
            else => return e,
        };
        // Look for the separator character. If none is found, then skip parsing
        // and just print the line to stdout.
        const n = mem.indexOfScalar(u8, line, 2) orelse {
            _ = try output.interface.write(line);
            continue;
        };
        // Unreachable because we expect `git` to at least have one space
        // character after the separator, since we use %ar, which prints
        // something like "3 days ago".
        const m = (mem.indexOfScalar(u8, line[n..], ' ') orelse unreachable) + n;
        line[m] = if (line[m + 1] == 'm' and line[m + 2] == 'o') 'M' else line[m + 1];
        @memmove(line[n..m], line[n + 1 .. m + 1]);
        const j = (mem.indexOfScalar(
            u8,
            line[m..],
            if (app.is_atty) '\x1b' else ')',
        ) orelse unreachable) + m;
        @memmove(line[m .. m + line.len - j], line[j..]);
        line = line[0 .. m + line.len - j];
        _ = try output.interface.write(line);
    }
    proc_ls_stdin.close();
    _ = try proc_ls.wait();
    const term = try proc_gl.wait();
    return term.Exited;
}

pub fn main() !void {
    _ = try main_inner();
}

test {
    _ = @import("logger.zig");
}
