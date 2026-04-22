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
    (IS_ATTY ? (c->refs + 3 == c->ending_newline)                              \
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

static unsigned char SHA_LEN = 0;

/// Parses the contents of `buf` as a line of git log.
static inline void git_log_entry_parse(struct git_log_entry *c, char *buf,
                                       int buf_len) {
    c->ending_newline = memchr(buf, '\n', buf_len);

    // Find where the commit SHA starts.
    if ((c->hash = memchr(buf, sp, c->ending_newline - buf - 1)) == NULL) {
        // If even the commit SHA is not found, then there is nothing left to
        // parse.
        return;
    }

    // WARNING: for the next section, for each call to `memchr` we will skip
    // the NULL check. This is because we assume a certain structure of the git
    // log entry based on FMT_ARGS_* as defined above.

    c->hash++;
    if (SHA_LEN) { // Fact: SHA_LEN is kept constant between runs of git log.
        c->date = c->hash + 1 + SHA_LEN;
    } else {
        c->date = memchr(c->hash, ' ', 32);
        SHA_LEN = c->date - c->hash - 1;
    }
    c->date++;
    c->subject = memchr(c->date, sp, buf_len - (c->date - buf));
    c->subject++;
    c->refs = memchr(c->subject, sp, buf_len - (c->subject - buf));
    c->refs++;
}

static inline void git_log_entry_print(struct git_log_entry *c, char *read_buf,
                                       char *write_buf, unsigned char IS_ATTY,
                                       int fd) {
    // If there is no commit SHA found, that implies that the entire line is
    // just the graph visual. So we just print the entire line.
    if (!c->hash) {
        write(fd, read_buf, c->ending_newline - read_buf + 1);
        return;
    }
    char *ptr = write_buf;

#define PRINT(src, len)                                                        \
    memcpy(ptr, src, len);                                                     \
    ptr += len;
#define PRINT_TTY(src)                                                         \
    if (IS_ATTY) {                                                             \
        PRINT(src, sizeof(src) - 1);                                           \
    }
    PRINT(read_buf, c->hash - read_buf - 1); // print the graph visual
    PRINT_TTY(YELLOW);
    PRINT(c->hash, c->date - c->hash); // print the commit SHA, plus a space.

    if (HAS_REFS(c)) {
        PRINT_TTY(RESET); // There are no refs.
    } else {
        PRINT_TTY(DARK_GRAY);
        *(ptr++) = '{';
        PRINT(c->refs, REF_LEN(c)); // print the branches/remotes.
        PRINT_TTY(DARK_GRAY);
        *(ptr++) = '}';
        PRINT_TTY(RESET);
        *(ptr++) = ' ';
    }

    PRINT(c->subject, c->refs - c->subject - 1);
    *(ptr++) = ' ';

    // Print the date.
    PRINT_TTY(DARK_GRAY);
    *(ptr++) = '(';
    PRINT_TTY(LIGHT_GRAY);
    // We borrow `c->refs` to point to the first space after the number in the
    // author's relative date.
    c->refs = memchr(c->date, ' ', 8);
    if (*(c->refs + 1) == 'm' && *(c->refs + 2) == 'o') {
        *c->refs = 'M'; // month -> M
    } else {
        *c->refs = *(c->refs + 1);
    }
    PRINT(c->date, c->refs - c->date + 1);
    PRINT_TTY(DARK_GRAY);
    *(ptr++) = ')';
    PRINT_TTY(RESET);
    *(ptr++) = '\n';

    log_trace("Writing to fd...");
    write(fd, write_buf, ptr - write_buf);
    log_trace("Done writing to fd!");
#undef PRINT
#undef PRINT_TTY
}

#undef REF_LEN
#undef HAS_REFS

#endif
