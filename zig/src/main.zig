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
const argv_ls: [3][]const u8 = .{ "less", "-RFG", "--cmd=/HEAD\n" };

/// Gathers the CLI arguments to send to `git-log`. Amazingly, somehow zig
/// manages to pass on the "is-a-tty"-ness to the child process, so we don't
/// need to pass the "--color=always" option.
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
        const argv: []const u8 = std.mem.span(argv_z);
        std.log.debug("argv: {s}", .{argv});
        if (!std.mem.eql(u8, argv, "--bound")) {
            try argv_gl.append(allocator, argv);
        }
    }
    try argv_gl.append(allocator, "--graph");
    try argv_gl.append(allocator, "--format=" //
        ++ "%C(yellow)%h" // commit SHA
        ++ "%C(auto)" //
        ++ "%(decorate:prefix= {,suffix=},pointer= \x1b[33m-> )" // refs
        ++ " %s " // commit subject (message)
        ++ "%C(240)(%C(246)\x02%ar"); // relative author time

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
    proc_ls.stdin_behavior = .Inherit;
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

    var proc_gl = std.process.Child.init(argv_gl, fba.allocator());
    try proc_gl.spawn();

    // try argv_gl.append(fba.allocator(), "git");
    // var j = 0;
    // var argv_gl2: [64][]const u8 = undefined;
    // argv_gl2[j] = "git";
    // j += 1;
    // //     // git options.
    // //     "git",
    // //     "-c",
    // //     "color.diff.commit=241", // This colors the parentheses around the refs.
    // //     "--no-pager",
    // //     // git-log options.
    // //     "log",
    // // };
    // for (0..64) |i| {
    //     std.debug.print("{d} = {s}\n", .{ i, argv_gl2[i] });
    // }
    // argv_gl2[0] = "git";
    // argv_gl2[1] = "-c";

    // var buffer: [0x800]u8 = undefined;
    // var f_reader = proc_gl.stdout.?.readerStreaming(&buffer);
    // var reader = &f_reader.interface;
    // while (true) {
    //     const line: []u8 = try reader.takeDelimiter('\n') orelse break;
    //     std.debug.print("Chunk: {s}\n", .{line});
    // }

    // var buffer: [0x800]u8 = undefined;
    //
    // var file_reader = proc_gl.stdout.?.readerStreaming(&buffer);
    // var reader = &file_reader.interface;
    //
    // while (true) {
    //     const line: []u8 = try reader.takeDelimiter('\n') orelse break;
    //     std.debug.print("Chunk: {s}\n", .{line});
    // }
    //
    // _ = try proc_gl.wait();
    //
    // _ = try proc_ls.wait();
    //
    // std.debug.print("The end.\n", .{});
}

// try std.testing.expectEqual(term_less, std.process.Child.Term{ .Exited = 0 });
