const std = @import("std");
const ln = @import("ln");
const l = std.os.linux;

const App = struct { is_bounded: bool, is_atty: bool };

fn getBoundedFlagFromCli() bool {
    for (std.os.argv) |argv| {
        if (std.mem.eql(u8, std.mem.span(argv), "--bound")) {
            return true;
        }
    }
    return false;
}

pub fn main() !void {
    const app: App = .{
        .is_bounded = getBoundedFlagFromCli(),
        .is_atty = std.posix.isatty(std.fs.File.stdout().handle),
    };

    if (app.is_atty) {
        std.debug.print("IS A TTY!", .{});
    } else {
        std.debug.print("IS NOT TTY!", .{});
    }
}
