const Self = @This();

const std = @import("std");
const mem = std.mem;
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

/// Gets the CLI arguments to send to `less`.
pub fn less_args(
    self: *const Self,
    allocator: mem.Allocator,
) error{ OutOfMemory, NoSpaceLeft }![][]const u8 {
    var args: std.ArrayList([]const u8) = try .initCapacity(allocator, 3);
    args.appendSliceAssumeCapacity(&[_][]const u8{ "less", "-RFG" });

    var num_buf: ?[]u8 = null;
    if (self.win_rows) |rows| {
        const half = rows / 2;
        const n = half - @min(half, 1);
        num_buf = try allocator.alloc(u8, std.math.log10(n) + 1 + 13 + 10);
        const num_str = try std.fmt.bufPrint(num_buf.?, "--cmd=/HEAD\n{d}k", .{n});
        try args.append(allocator, num_str);
    }

    return args.items;
}

/// Gets the CLI arguments to send to `git-log`.
pub fn git_log_args(
    self: *const Self,
    allocator: mem.Allocator,
) error{ OutOfMemory, NoSpaceLeft }![][]const u8 {
    var args: std.ArrayList([]const u8) = try .initCapacity(allocator, 16);
    args.appendSliceAssumeCapacity(&[_][]const u8{
        // git options.
        "git",
        "-c",
        "color.diff.commit=241", // This colors the parentheses around the refs.
        "--no-pager",
        // git-log options.
        "log",
    });
    var num_buf: ?[]u8 = null;
    if (self.maxOutputRows()) |rows| {
        try args.append(allocator, "-n");
        num_buf = try allocator.alloc(u8, std.math.log10(rows) + 1);
        const num_str = try std.fmt.bufPrint(num_buf.?, "{d}", .{rows});
        try args.append(allocator, num_str);
    }

    for (std.os.argv[1..]) |argv_z| {
        const argv: []const u8 = mem.span(argv_z);
        if (!mem.eql(u8, argv, "--bound")) {
            try args.append(allocator, argv);
        }
    }
    try args.append(allocator, "--graph");
    try args.append(allocator, "--format=" //
        ++ "%C(yellow)%h" // commit SHA
        ++ "%C(auto)" //
        ++ "%(decorate:prefix= {,suffix=},pointer= \x1b[33m-> )" // refs
        ++ " %s " // commit subject (message)
        ++ "%C(240)(%C(246)\x02%ar%C(240))%C(reset)" // relative author time
    );
    if (self.is_atty) {
        try args.append(allocator, "--color=always");
    }
    return args.items;
}
