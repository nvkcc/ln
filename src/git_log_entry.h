#ifndef b8513d4d5963ad53b6be5a7abac6bda91d7f7149
#define b8513d4d5963ad53b6be5a7abac6bda91d7f7149 1

#include "config.h"
#include <string.h>
#include <unistd.h>

#define sp '\002' // separator character
#define SP "\002" // separator string

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

#endif
