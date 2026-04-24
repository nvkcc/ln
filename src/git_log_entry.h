#ifndef b8513d4d5963ad53b6be5a7abac6bda91d7f7149
#define b8513d4d5963ad53b6be5a7abac6bda91d7f7149 1

#include "config.h"
#include "log.h"
#include <string.h>
#include <unistd.h>

struct git_log_entry {
    char *hash, *date, *subject, *refs, *ending_newline;
};

#define REF_LEN(c) (c->ending_newline - c->refs)
#define HAS_REFS(c)                                                            \
    (is_atty ? (c->refs + 3 == c->ending_newline)                              \
             : (c->refs == c->ending_newline))

#define sp '\002' // separator character
#define SP "\002" // separator string

// First `SP` is necessary to isolate out graph visuals.
// order is <hash> <time>  <commit message> <refs>.
// %h  | abbreviated commit hash
// %ar | author date, relative
// %s  | subject
// %D  | ref names without the " (", ")" wrapping.
#define GIT_LN_FMT_ARGS_0 SP "%h %ar" SP "%s" SP "%D"
#define GIT_LN_FMT_ARGS_1 SP "%h %ar" SP "%s" SP "%C(auto)%D"

static inline void git_log_entry_print(char *read_buf, char *write_buf,
                                       unsigned char is_atty, int fd) {
    char *x, *y, *newline;
    newline = memchr(read_buf, '\n', GIT_LN_BUF_SZ);
    // Look for the `sp` byte. We've set this in the --format flag for git log.
    if ((x = memchr(read_buf, sp, newline - read_buf)) == NULL) {
        write(fd, read_buf, newline - read_buf + 1);
        return;
    }
    // Look for the first space.
    if ((y = memchr(x, ' ', newline - x)) == NULL) {
        write(fd, read_buf, newline - read_buf + 1);
        return;
    }
    // Look for period labels that start with "mo" and change that to "M".
    // Otherwise, keep the first letter.
    if (*(y + 1) == 'm' && *(y + 2) == 'o') {
        *y = 'M'; // month -> M
    } else {
        *y = *(y + 1);
    }
    // Shift everything over to cover the `sp` byte.
    for (; x < y; ++x) {
        *x = *(x + 1);
    }
    // Print the closing parenthesis.
    if (!is_atty) {
        *x++ = ')', *x++ = '\n';
        write(fd, read_buf, x - read_buf);
    } else {
        // This ANSI code comes from inspecting the git output for the opening
        // parenthesis.
        memcpy(x, "\e[38;5;240m)\n", 13);
        write(fd, read_buf, x - read_buf + 13);
    }
}

#undef REF_LEN
#undef HAS_REFS

#endif
