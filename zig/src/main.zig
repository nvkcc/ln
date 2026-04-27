const std = @import("std");
const ln = @import("ln");
const linux = std.os.linux;
const posix = std.posix;

const App = @import("app.zig");

/// Args for `git log`.
// const argv_gl: [4][]const u8 = .{ "git", "log", "--graph", "--oneline" };

/// Args for `less`.
const argv_ls: [3][]const u8 = .{ "lalsdfjkes", "-RFG", "--cmd=/HEAD\n" };

fn git_log_args() [64][]const u8 {
    var buf: [64][]const u8 = undefined;
    buf[0] = "git";
    buf[1] = "-c";
    return buf;
}

pub fn main() !void {
    const app: App = App.init();

    var gpa = std.heap.DebugAllocator(.{}){};
    var fbuffer: [0x800]u8 = undefined;
    var fba: std.heap.FixedBufferAllocator = .init(&fbuffer);
    defer _ = gpa.deinit();

    // Child process for `git log`.
    // var proc_gl = std.process.Child.init(&argv_gl, gpa.allocator());
    // proc_gl.stdout_behavior = .Pipe;
    // try proc_gl.spawn();

    var proc_ls = std.process.Child.init(&argv_ls, gpa.allocator());
    proc_ls.stdin_behavior = .Inherit;
    try proc_ls.spawn();

    proc_ls.waitForSpawn() catch |err| switch (err) {
        std.process.Child.SpawnError.FileNotFound => {
            std.debug.print("`less` is not installed.\n", .{});
            // TODO: spawn git log here.
            // try proc_gl.spawn();
            return;
        },
        else => return err,
    };
    std.debug.print("GOT HERE\n", .{});

    // var argv_gl: std.ArrayList([]const u8) = [_][]u8{};
    var argv_gl_init = [5][]const u8{
        // git options.
        "git",
        "-c",
        "color.diff.commit=241", // This colors the parentheses around the refs.
        "--no-pager",
        // git-log options.
        "log",
    };
    var argv_gl: std.ArrayList([]const u8) = .fromOwnedSlice(argv_gl_init[0..]);
    if (app.maxOutputRows()) |_| {
        try argv_gl.append(fba.allocator(), "-n");
        try argv_gl.append(fba.allocator(), "10000");
    }
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
