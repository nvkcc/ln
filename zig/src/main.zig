const std = @import("std");
const ln = @import("ln");
const mem = std.mem;
const linux = std.os.linux;
const posix = std.posix;

pub const std_options: std.Options = .{
    .log_level = .debug,
    .logFn = @import("logger.zig").customLog,
};

const App = @import("app.zig");

/// Args for `less`.
const argv_ls: [2][]const u8 = .{
    "less",
    "-RFG",
};

/// Gathers the CLI arguments to send to `git-log`.
fn git_log_args(
    allocator: mem.Allocator,
    app: *const App,
) error{ OutOfMemory, NoSpaceLeft }![][]const u8 {
    var argv_gl: std.ArrayList([]const u8) = try .initCapacity(allocator, 16);
    argv_gl.appendSliceAssumeCapacity(&[_][]const u8{
        // git options.
        "git",
        "-c",
        "color.diff.commit=241", // This colors the parentheses around the refs.
        "--no-pager",
        // git-log options.
        "log",
    });
    const git_log_n_val_buf = try allocator.alloc(u8, 16);
    if (app.maxOutputRows()) |rows| {
        try argv_gl.append(allocator, "-n");
        const n = try std.fmt.bufPrint(git_log_n_val_buf, "{d}", .{rows});
        try argv_gl.append(allocator, n);
    }

    for (std.os.argv[1..]) |argv_z| {
        const argv: []const u8 = mem.span(argv_z);
        std.log.debug("argv: {s}", .{argv});
        if (!mem.eql(u8, argv, "--bound")) {
            try argv_gl.append(allocator, argv);
        }
    }
    try argv_gl.append(allocator, "--graph");
    try argv_gl.append(allocator, "--format=" //
        ++ "%C(yellow)%h" // commit SHA
        ++ "%C(auto)" //
        ++ "%(decorate:prefix= {,suffix=},pointer= \x1b[33m-> )" // refs
        ++ " %s " // commit subject (message)
        ++ "%C(240)(%C(246)\x02%ar%C(240))%C(reset)" // relative author time
    );
    if (app.is_atty) {
        try argv_gl.append(allocator, "--color=always");
    }

    return argv_gl.items;
}

pub fn main() !void {
    const app: App = App.init();

    var fbuffer: [0x1000]u8 = undefined;
    var fba: std.heap.FixedBufferAllocator = .init(&fbuffer);
    var gpa = std.heap.DebugAllocator(.{}){};
    defer _ = gpa.deinit();

    const argv_gl = try git_log_args(fba.allocator(), &app);
    for (0..argv_gl.len) |i| {
        std.log.info("* [{d}] = {s}\x1b[m", .{ i, argv_gl[i] });
    }

    // Spawn `less` first to see if it exists.
    var proc_ls = std.process.Child.init(&argv_ls, fba.allocator());
    proc_ls.stdin_behavior = .Pipe;
    try proc_ls.spawn();

    // If `less` is not installed, then just run a full bypass to git log.
    proc_ls.waitForSpawn() catch |err| switch (err) {
        std.process.Child.SpawnError.FileNotFound => {
            std.log.warn("`less` is not installed.\n", .{});
            var proc_gl = std.process.Child.init(argv_gl, fba.allocator());
            try proc_gl.spawn();
            return;
        },
        else => return err,
    };

    // This should be safe because we already set the stdin_behavior above.
    const proc_ls_stdin = proc_ls.stdin orelse unreachable;

    try posix.dup2(proc_ls_stdin.handle, std.fs.File.stdin().handle);

    var proc_gl = std.process.Child.init(argv_gl, fba.allocator());
    proc_gl.stdout_behavior = .Pipe;
    try proc_gl.spawn();

    const read_buf = try fba.allocator().alloc(u8, 0x400);
    const write_buf = try fba.allocator().alloc(u8, 0x400);
    var f_reader = proc_gl.stdout.?.readerStreaming(read_buf);
    var reader = &f_reader.interface;
    // var stdout = std.fs.File.stdout().writer(write_buf);
    var stdout = proc_ls_stdin.writer(write_buf);
    loop: while (true) {
        var line = reader.takeDelimiterInclusive('\n') catch |e| switch (e) {
            error.EndOfStream => break :loop,
            else => return e,
        };
        // Look for the separator character. If none is found, then skip parsing
        // and just print the line to stdout.
        const n = mem.indexOfScalar(u8, line, 2) orelse {
            _ = try stdout.interface.write(line);
            continue;
        };
        // Unreachable because we expect `git` to at least have one space
        // character after the separator, since we use %ar, which prints
        // something like "3 days ago".
        const m = (mem.indexOfScalar(u8, line[n..], ' ') orelse unreachable) + n;
        if (line[m + 1] == 'm' and line[m + 2] == 'o') {
            line[m] = 'M';
        } else {
            line[m] = line[m + 1];
        }
        @memmove(line[n..m], line[n + 1 .. m + 1]);

        const j = j: {
            const t: u8 = if (app.is_atty) '\x1b' else ')';
            break :j (mem.indexOfScalar(u8, line[m..], t) orelse unreachable) + m;
        };
        @memmove(line[m .. m + line.len - j], line[j..]);
        line = line[0 .. m + line.len - j];
        _ = try stdout.interface.write(line);
    }
}

// try std.testing.expectEqual(term_less, std.process.Child.Term{ .Exited = 0 });
