#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h> // for getting window size
#include <sys/wait.h>
#include <unistd.h>

// #define DEBUG_MODE

#include "git_log_entry.h"
#include "globals.h"

// Gotchas:
// 1. Every system has a limited `pipe` buffer size. For sufficiently large git
//    logs, if the pipe is constantly written to but not read from, it will
//    block at some point.
// 2. Certain calls to `printf` will just remove all STDOUT outputs of an exec()
//    call. Use a proper logger instead.

// =============================================================================
// Macro definitions.
// =============================================================================

#define S(literal) literal, sizeof(literal) - 1

#define PIPE_OR_RETURN(fd)                                                     \
    if (pipe(fd) == -1) {                                                      \
        perror("pipe failed");                                                 \
        return 1;                                                              \
    }
#define FORK_OR_RETURN(pid)                                                    \
    if ((pid = fork()) == -1) {                                                \
        perror("fork failed");                                                 \
        return 1;                                                              \
    }

#define SEND_STDERR(msg) write(STDERR_FILENO, msg, sizeof(msg));
#define SEND_STDERR_LN(msg) write(STDERR_FILENO, msg "\n", sizeof(msg) + 1);

// =============================================================================
// Main line.
// =============================================================================

struct pipedata {
    int fd[2];
    pid_t pid;
};

/// Checks for the "--bounded" argument, if any. Return its location too. 0 if
/// not found (it will never be at position 0).
static void setup_bounded_cli_arg_idx(int argc, const char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--bound", 7) == 0) {
            BOUNDED_ARG_IDX = i;
        }
    }
}

static int32_t get_row_limit() {
    if (!IS_ATTY) {
        // Not a tty -> the screen-size dependent "--bound" flag is meaningless.
        return INT32_MAX;
    }
    if (BOUNDED_ARG_IDX > 0) {
        // "--bounded" flag is supplied.
        return (WIN.ws_row > GIT_LN_SCREEN_VERTICAL_PAD)
                   ? WIN.ws_row - GIT_LN_SCREEN_VERTICAL_PAD
                   : WIN.ws_row;
    } else {
        // "--bounded" flag is not supplied.
        return INT32_MAX;
    }
}

int exec_git_log(const int argc, const char *argv[]) {
    const char *args[argc // [extra args]
                     + 1  // "--no-pager"
                     + 1  // "log"
                     + 1  // "--graph"
                     + 1  // "--format=..."
                     + 1  // "--color=..."
                     + 2  // -n ...
                     + 1  // NULL
    ];
    int j = 0, i;
    args[j++] = GIT;
    args[j++] = "--no-pager"; // (+1 arg)
    args[j++] = "log";        // (+1 arg)
    char num_buf[8];
    if (IS_ATTY && BOUNDED_ARG_IDX > 0) {
        snprintf(num_buf, 8, "%d", get_row_limit() + 1);
        args[j++] = "-n";
        args[j++] = num_buf;
        log_info("Restricted git log to %s", num_buf);
    }
    // Copy the values of `argv` into `args`. (+(argc - 1) args)
    for (i = 1; i < argc; ++i) {
        if (i != BOUNDED_ARG_IDX) {
            args[j++] = argv[i];
        }
    }
    args[j++] = "--graph"; // (+1 arg)
    if (IS_ATTY) {
        args[j++] = "--format=" GIT_LN_FMT_ARGS_1; // (+1 arg)
        args[j++] = "--color=always";              // (+1 arg)
    } else {
        args[j++] = "--format=" GIT_LN_FMT_ARGS_0;
        args[j++] = "--color=never";
    }
    args[j++] = NULL; // (+1 arg)

    // I have no idea why but if we print the next line, then no output
    // comes out of `git log` in the execvp at all. ???
    // for (i = 0; i < argc + 6; ++i) { printf("[%d] = %s\n", i, args[i]); }

#ifdef DEBUG_MODE
    log_trace("git-log running execvp (%d args)", j);
    for (int i = 0; i < j; ++i) {
        log_trace("git-log[%d] = %s", i, args[i]);
    }
#endif
    i = execvp(GIT, (char **)args);
    log_trace("git-log execvp failed");
    perror("git log failed");
    return 1;
}

/// Reads the output of `git log` (input_stream), parses it, and sends it to
/// `less` (output_fd).
void run_parse_print_loop(FILE *input_stream, int output_fd) {
    int32_t n = get_row_limit();
    struct git_log_entry c;
    log_trace("less printer started with limit %d", n);
    while (n-- > 0 && fgets(R_BUF, GIT_LN_BUF_SZ, input_stream)) {
        git_log_entry_parse(&c, R_BUF, GIT_LN_BUF_SZ);
        git_log_entry_print(&c, R_BUF, W_BUF, IS_ATTY, output_fd);
    }
}

void exec_less() {
    // In this if-block, both branches lead to an `execlp`. So if all goes
    // well, this is the last point of C code execution.
    if (IS_ATTY) {
#define cmd "--cmd=/HEAD ->\n"
        memcpy(R_BUF, S(cmd));
        int up = WIN.ws_row / 2;
        if (up > 0) {
            up--;
        }
        memset(R_BUF + sizeof(cmd) - 1, 'k', up);
#undef cmd
        log_trace("execlp(\"less\") with cmd");
        execlp(LESS, LESS, "-RFG", R_BUF, NULL);
    } else {
        log_trace("execlp(\"less\") with no cmd");
        execlp(LESS, LESS, "-RFG", NULL);
    }
}

/// `fd` must be the read-end of a pipe. This function reads it until there is
/// no more data, and then closes it.
void clear_and_close(int fd) {
    while (read(fd, BUF2, sizeof(BUF2)) > 0) {
    }
    close(fd);
}

int main(const int argc, const char *argv[]) {
    unsigned int i, j;
    FILE *stream;
    char *p;

    // Pipe data for `git log`.
    struct pipedata gl;

    // Pipe data for reading from `git log` and printing to `less`.
    struct pipedata pt;

    // PID for the `less` child.
    pid_t less_pid;

#ifdef DEBUG_MODE
    FILE *f = fopen("/home/khang/Downloads/log.txt", "w");
    log_add_fp(f, LOG_TRACE);
    log_set_quiet(1);
    gl.pid = -1, pt.pid = -1;
#endif

    // Setup global variables.
    setup_bounded_cli_arg_idx(argc, argv);
    IS_ATTY = isatty(STDOUT_FILENO) ? 1 : 0;
    /// Compute the window size.
    if (IS_ATTY) {
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &WIN) != 0) {
            SEND_STDERR_LN("Failed to get terminal window size.");
            return 1;
        }
    }
    // There are only two possible state combinations at this point:
    // (1.) is NOT a tty.
    // (2.) is a tty, AND ioctl successfully returned.
    //
    // For case (1.), we shall print an unbounded number of git log entries,
    // even if the "--bound" flag is supplied. This is because that flag relies
    // on the existence of a screen. Nevertheless, we still need to know its
    // index in `argv` because we cannot forward it to the call to `git log`.

    PIPE_OR_RETURN(gl.fd) FORK_OR_RETURN(gl.pid);
    /* Open fds: { gl.fd[0], gl.fd[1] }. */

    if (gl.pid == 0) {
        dup2(gl.fd[1], STDOUT_FILENO);
        close(gl.fd[0]);
        close(gl.fd[1]);
        return exec_git_log(argc, argv);
    } else {
        close(gl.fd[1]);
    }
    /* Open fds: { gl.fd[0] }. */

    PIPE_OR_RETURN(pt.fd) FORK_OR_RETURN(pt.pid);
    /* Open fds: { gl.fd[0], pt.fd[0], pt.fd[1] }. */

    if (pt.pid == 0) {
        log_trace("[%d, %d] call fdopen() in less printer", gl.pid, pt.pid);
        // Run the printer on the git log file descriptor.
        if (!(stream = fdopen(gl.fd[0], "rb"))) {
            close(gl.fd[0]);
            close(pt.fd[0]);
            close(pt.fd[1]);
            log_trace("[%d, %d] fdopen failed", gl.pid, pt.pid);
            perror("fdopen failed");
            return 1;
        }
        run_parse_print_loop(stream, pt.fd[1]);
        clear_and_close(gl.fd[0]);
        fclose(stream);
        log_trace("[%d, %d] less printer loop ended", gl.pid, pt.pid);
        close(gl.fd[0]);
        close(pt.fd[0]);
        close(pt.fd[1]);
        log_trace("[%d, %d] less printer completed", gl.pid, pt.pid);
        return 0;
    } else {
        close(gl.fd[0]);
        close(pt.fd[1]);
    }
    /* Open fds: { pt.fd[0] }. */

    FORK_OR_RETURN(less_pid);
    if (less_pid == 0) {
        log_trace("[%d, %d] start less", gl.pid, pt.pid);
        dup2(pt.fd[0], STDIN_FILENO);
        exec_less();

        // At this point, something wrong happened with `less`. Possibly because
        // it is not installed. In that case, just print all the outputs to
        // stdout. No parsing is required here. Just reflect whatever is printed
        // above.

        log_trace("[%d, %d] call fdopen() in final reflector", gl.pid, pt.pid);
        if ((stream = fdopen(pt.fd[0], "rb"))) {
            while (fgets(R_BUF, GIT_LN_BUF_SZ, stream) == R_BUF) {
                p = memchr(R_BUF, '\n', GIT_LN_BUF_SZ);
                write(STDOUT_FILENO, R_BUF, p - R_BUF + 1);
            }
            close(pt.fd[0]);
            fclose(stream);
            log_trace("[%d, %d] bypass less", gl.pid, pt.pid);
            return 0;
        } else {
            // `fdopen` has failed. Clear the pipe.
            clear_and_close(pt.fd[0]);
            log_trace("[%d, %d] fdopen failed", gl.pid, pt.pid);
            perror("fdopen failed");
            return 1;
        }
    }

    waitpid(less_pid, NULL, 0);
    clear_and_close(pt.fd[0]);
    waitpid(gl.pid, NULL, 0);
    waitpid(pt.pid, NULL, 0);
    log_trace("The end.");
    return 0;
}
