const std = @import("std");
const ln = @import("ln");
const linux = std.os.linux;
const posix = std.posix;

const App = @import("app.zig");

/// Args for `git log`.
const argv_gl: [4][]const u8 = .{ "git", "log", "--graph", "--oneline" };

/// Args for `less`.
const argv_ls: [3][]const u8 = .{ "less", "-RFG", "--cmd=/HEAD\n" };

pub fn main() !void {
    const app: App = App.init();
    _ = app;

    var gpa = std.heap.DebugAllocator(.{}){};
    defer _ = gpa.deinit();

    // Child process for `git log`.
    var proc_gl = std.process.Child.init(&argv_gl, gpa.allocator());
    proc_gl.stdout_behavior = .Pipe;
    try proc_gl.spawn();

    var proc_ls = std.process.Child.init(&argv_ls, gpa.allocator());
    proc_ls.stdin_behavior = .Inherit;
    const err_ls = proc_ls.spawn();

    var buffer: [0x800]u8 = undefined;

    var file_reader = proc_gl.stdout.?.readerStreaming(&buffer);
    var reader = &file_reader.interface;

    while (true) {
        const line: []u8 = try reader.takeDelimiter('\n') orelse break;
        std.debug.print("Chunk: {s}\n", .{line});
    }

    _ = try proc_gl.wait();

    var proc_ls = std.process.Child.init(&argv_ls, gpa.allocator());
    _ = try proc_ls.wait();

    std.debug.print("The end.\n", .{});
}

// try std.testing.expectEqual(term_less, std.process.Child.Term{ .Exited = 0 });
