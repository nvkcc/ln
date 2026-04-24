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

////////////////////////////////////////////////////////////////////////////////
/// Derivatives from the global variables.
////////////////////////////////////////////////////////////////////////////////

/// Gets the maximum number of rows to print for `git log`. Returns 0 if there
/// is no bound. Will be used for the "-n" (or "--max-count") flag for git log.
static inline int git_log_max_count() {
    if ((GIT_LN_FLAGS & GIT_LN_IS_ATTY) == 0) {
        // Not a tty -> the screen-size dependent "--bound" flag is meaningless.
        return 0;
    }
    if (GIT_LN_FLAGS & GIT_LN_IS_BOUNDED) {
        // "--bound" flag is supplied.
        return (WIN.ws_row > GIT_LN_SCREEN_VERTICAL_PAD)
                   ? WIN.ws_row - GIT_LN_SCREEN_VERTICAL_PAD
                   : WIN.ws_row;
    } else {
        // "--bound" flag is not supplied.
        return 0;
    }
}
