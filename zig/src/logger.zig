const std = @import("std");

/// The default implementation for the log function. Custom log functions may
/// forward log messages to this function.
///
/// Uses a 64-byte buffer for formatted printing which is flushed before this
/// function returns.
pub fn customLog(
    comptime message_level: std.log.Level,
    comptime scope: @Type(.enum_literal),
    comptime format: []const u8,
    args: anytype,
) void {
    const level_txt = comptime message_level.asText();
    const prefix2 = if (scope == .default) ": " else "(" ++ @tagName(scope) ++ "): ";
    var buffer: [64]u8 = undefined;
    const stderr = std.debug.lockStderrWriter(&buffer);
    defer std.debug.unlockStderrWriter();
    const color = switch (message_level) {
        .debug => "\x1b[36m",
        .info => "\x1b[32m",
        .warn => "\x1b[33m",
        .err => "\x1b[31m",
    };
    nosuspend stderr.print(color ++ level_txt ++ "\x1b[m" ++ prefix2 ++ format ++ "\n", args) catch return;
}

fn num_digits(n: u16) usize {
    return std.math.log10(n) + 1;
}

test "num_digits" {
    try std.testing.expectEqual(num_digits(1), 1);
    try std.testing.expectEqual(num_digits(9), 1);
    try std.testing.expectEqual(num_digits(10), 2);
    try std.testing.expectEqual(num_digits(99), 2);
    try std.testing.expectEqual(num_digits(100), 3);
    try std.testing.expectEqual(num_digits(999), 3);
    try std.testing.expectEqual(num_digits(1000), 4);
}
