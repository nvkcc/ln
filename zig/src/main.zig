const std = @import("std");
const ln = @import("ln");
const linux = std.os.linux;
const posix = std.posix;

const App = @import("app.zig");

pub fn main() !void {
    const app: App = App.init();

    std.debug.print("rows: {any}\n", .{app.maxOutputRows()});

    // var wsz: std.posix.winsize = undefined;
    // const ioctl_res = l.ioctl(l.STDOUT_FILENO, l.T.IOCGWINSZ, @intFromPtr(&wsz));
    // if (ioctl_res == -1) {}

    if (app.is_atty) {
        std.debug.print("IS A TTY!", .{});
    } else {
        std.debug.print("IS NOT TTY!", .{});
    }
}
