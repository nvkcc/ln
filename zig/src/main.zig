const std = @import("std");
const ln = @import("ln");
const linux = std.os.linux;
const posix = std.posix;

const App = @import("app.zig");

pub fn main() !void {
    const app: App = App.init();
    var gpa = std.heap.DebugAllocator(.{}){};
    defer _ = gpa.deinit();

    std.debug.print("rows: {any}\n", .{app.maxOutputRows()});
    const git_log_argv: [4][]const u8 = .{ "git", "log", "--graph", "--oneline" };
    var proc = std.process.Child.init(&git_log_argv, gpa.allocator());
    proc.stdout_behavior = .Pipe;
    try proc.spawn();

    var buffer: [0x800]u8 = undefined;

    var file_reader = proc.stdout.?.readerStreaming(&buffer);
    var reader = &file_reader.interface;

    while (true) {
        const line: []u8 = try reader.takeDelimiter('\n') orelse return;
        // reader.takeDelimiter('\n');
        // const len = try reader.readSliceShort(&buffer);
        // if (len == 0) break;
        std.debug.print("Chunk: {s}\n", .{line});
    }
    // var reader = f_reader.interface;
    // while (true) {
    //     const len = try reader.readSliceShort(&chunk);
    //     if (len == 0) break;
    //     std.debug.print("Chunk: {s}\n", .{chunk[0..len]});
    // }

    const term = try proc.wait();
    try std.testing.expectEqual(term, std.process.Child.Term{ .Exited = 0 });
    // child.spawn();

    // var wsz: std.posix.winsize = undefined;
    // const ioctl_res = l.ioctl(l.STDOUT_FILENO, l.T.IOCGWINSZ, @intFromPtr(&wsz));
    // if (ioctl_res == -1) {}

    if (app.is_atty) {
        std.debug.print("IS A TTY!\n", .{});
    } else {
        std.debug.print("IS NOT TTY!\n", .{});
    }
}
