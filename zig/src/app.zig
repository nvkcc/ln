const Self = @This();

const std = @import("std");
const linux = std.os.linux;
const posix = std.posix;

/// When printing to the exact height of the tty window, reserve this many lines
/// as padding for visual reasons such as to still be able to see the previous
/// prompt, or to account for long lines that wrap.
const vertical_pad = 8;

/// Whether or not to bound the printed lines to the number of rows the
/// terminal window has.
is_bounded: bool,

/// Whether or not we're in a TTY. Decides if we print in color.
is_atty: bool,

/// Number of rows in the current TTY, if it exists.
win_rows: ?u16 = null,

fn getBoundedFlagFromCli() bool {
    for (std.os.argv) |argv| {
        if (std.mem.eql(u8, std.mem.span(argv), "--bound")) {
            return true;
        }
    }
    return false;
}

fn getWinRows() ?u16 {
    var w: posix.winsize = undefined;
    const res = linux.ioctl(linux.STDOUT_FILENO, linux.T.IOCGWINSZ, @intFromPtr(&w));
    return if (res == -1) null else w.row;
}

pub fn init() Self {
    const is_atty = std.posix.isatty(std.fs.File.stdout().handle);
    return .{
        .is_bounded = getBoundedFlagFromCli(),
        .is_atty = is_atty,
        .win_rows = if (is_atty) getWinRows() else null,
    };
}

/// Gets the maximum number of rows to print for `git log`. Will be used for the
/// "-n" (or "--max-count") flag for git log, among other things.
pub fn maxOutputRows(app: *const Self) ?u16 {
    if (!app.is_atty) {
        // Not a tty -> the screen-size dependent "--bound" flag is meaningless.
        return null;
    } else if (app.is_bounded) {
        // "--bound" flag is supplied.
        const rows = app.win_rows orelse return null;
        return if (rows > vertical_pad) rows - vertical_pad else rows;
    } else {
        // "--bound" flag is not supplied.
        return null;
    }
}
