const std = @import("std");
const ln = @import("ln");
const l = std.os.linux;

pub fn main() !void {
    const isatty = std.posix.isatty(std.fs.File.stdout().handle);
    if (isatty) {
        std.debug.print("IS A TTY!", .{});
    } else {
        std.debug.print("IS NOT TTY!", .{});
    }
}
