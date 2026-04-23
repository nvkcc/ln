/// This file should only be included once: in the main unit of translation.

#include "config.h"
#include <stdint.h>
#include <sys/ioctl.h> // for getting window size

/// The main allocated stack memory for this program. Will be split into two
/// buffers of equal size. Mainly used to clear pipes during termination.
static char BUF2[GIT_LN_BUF_SZ + GIT_LN_BUF_SZ];

/// Read buffer. Where bytes from `git log` output is read to.
static char *const R_BUF = BUF2;

/// Write buffer. To buffer writes to `less` STDIN.
static char *const W_BUF = BUF2 + GIT_LN_BUF_SZ;

enum git_ln_flag : uint8_t {
    GIT_LN_IS_ATTY = 0b001,
    GIT_LN_IS_BOUNDED = 0b010,
};

/// A combination of flags of git_ln_flag.
static enum git_ln_flag GIT_LN_FLAGS = 0;

/// The window size of the tty session, if it exists.
static struct winsize WIN;

/// The index of `argv` of the "--bound" flag. This tells us whether or not to
/// limit the printing of the log to the height of the current terminal window.
static unsigned short BOUNDED_ARG_IDX = 0;
