#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

// #define DEBUG_MODE

#include "git_log_entry.h"
#include "globals.h"
#include "log.h"

// =============================================================================
// Macro definitions.
// =============================================================================

#define MAX(A, B) (A < B ? B : A)
#define MIN(A, B) (A < B ? A : B)
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

#define write_stderr(msg) write(STDERR_FILENO, msg "\n", sizeof(msg) + 1);

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
            argv[i] = "--graph";
            GIT_LN_FLAGS |= GIT_LN_IS_BOUNDED;
        }
    }
}

/// Checked for possible overflow at `main()` function already.
static const char *args[GIT_LN_MAX_ARGS];
int exec_git_log(const int argc, const char *argv[], int max_rows) {
    const char **p = args;
    *p++ = GIT;
    *p++ = "-c";
    *p++ =
        "color.diff.commit=241"; // This colors the parentheses around the refs.
    *p++ = "--no-pager";
    *p++ = "log";
    if (max_rows > 0) {
        snprintf(BUFFER, 12, "%u", max_rows);
        *p++ = "-n";
        *p++ = BUFFER;
        log_info("Restricted git log to %s", BUFFER);
    }
    // Copy the values of `argv` into `args`.
    p = mempcpy(p, &argv[1], sizeof(char *) * (argc - 1));
    *p++ = "--graph";
    *p++ = "--format="
           "%C(yellow)%h" // commit SHA
           "%C(auto)"
           "%(decorate:prefix= {,suffix=},pointer= \x1b[33m-> )" // refs
           " %s "                     // commit subject (message)
           "%C(240)(%C(246)" SP "%ar" // relative author time
        ;
    if (GIT_LN_FLAGS & GIT_LN_IS_ATTY) {
        *p++ = "--color=always";
    }
    *p++ = NULL; // (+1 arg)

    // I have no idea why but if we print the next line, then no output
    // comes out of `git log` in the execvp at all. ???
    // for (i = 0; i < argc + 6; ++i) { printf("[%d] = %s\n", i, args[i]); }

#ifdef DEBUG_MODE
    log_trace("git-log running execvp (%d args)", p - args);
    for (int i = 0; i < p - args; ++i) {
        log_trace("git-log[%d] = %s", i, args[i]);
    }
#endif
    execvp(GIT, (char **)args);
    log_trace("git-log execvp failed");
    perror("git log failed");
    return 1;
}

/// Reads the output of `git log` (input_stream), parses it, and sends it to
/// `less` (output_fd).
void run_parse_print_loop(FILE *input_stream, int output_fd,
                          const int max_rows) {
    log_trace("less printer started with limit %d", max_rows);

    unsigned char is_atty = (GIT_LN_FLAGS & GIT_LN_IS_ATTY) ? 1 : 0;
    int i = 0;
    while (fgets(BUFFER, GIT_LN_BUFSIZ, input_stream) == BUFFER) {
        if (max_rows == 0 || i++ < max_rows) {
            git_log_entry_print(BUFFER, is_atty, output_fd);
        }
    }
}

void exec_less() {
    // In this if-block, both branches lead to an `execlp`. So if all goes
    // well, this is the last point of C code execution.
    if (GIT_LN_FLAGS & GIT_LN_IS_ATTY) {
        int up = WIN_ROWS / 2, adjust_down = 1;
        while (up > 0 && adjust_down > 0) {
            up -= 1;
            adjust_down--;
        }
        char *p = BUFFER;
#define cmd "--cmd=/HEAD ->\n"
        p = mempcpy(p, cmd, sizeof(cmd) - 1);
        p += snprintf(p, 4, "%d", up);
        *p++ = 'k';
        *p++ = '\0';
#undef cmd
        log_trace("execlp(\"less\") with cmd, up = %d, win-rows = %d", up,
                  WIN_ROWS);
        log_trace("execlp(\"less\") with BUFFER = %s", BUFFER);
        execlp(LESS, LESS, "-RFG", BUFFER, NULL);
    } else {
        log_trace("execlp(\"less\") with no cmd");
        execlp(LESS, LESS, "-RFG", NULL);
    }
}

// There are only two possible states after this function returns:
// (1.) is NOT a tty.
// (2.) is a tty, AND ioctl successfully returned.
//
// For case (1.), we shall print an unbounded number of git log entries,
// even if the "--bound" flag is supplied. This is because that flag relies
// on the existence of a screen. Nevertheless, we still need to know its
// index in `argv` because we cannot forward it to the call to `git log`.
int setup_tty(const int argc, const char *argv[]) {
    // Setup global variables.
    setup_bounded_cli_arg_idx(argc, argv);
    if (isatty(STDOUT_FILENO)) {
        GIT_LN_FLAGS |= GIT_LN_IS_ATTY;
        /// Compute the window size whether or not the "--bound" flag is present
        /// because we need it for to centre the search result in `less`.
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != 0) {
            write_stderr("Failed to get terminal window size.");
            return 1;
        }
        WIN_ROWS = w.ws_row;
    }
    return 0;
}

int main(const int argc, const char *argv[]) {
    FILE *stream;
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

    if (argc + 12 > GIT_LN_MAX_ARGS) {
        write_stderr("Too many args. CLI arg copy buffer might overflow.");
        return 1;
    }

    if (setup_tty(argc, argv)) {
        return 1;
    }
    int max_rows = git_log_max_count();

    PIPE_OR_RETURN(gl.fd) FORK_OR_RETURN(gl.pid);
    /* Open fds: { gl.fd[0], gl.fd[1] }. */

    if (gl.pid == 0) {
        dup2(gl.fd[1], STDOUT_FILENO);
        close(gl.fd[0]);
        close(gl.fd[1]);
        return exec_git_log(argc, argv, max_rows);
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
        run_parse_print_loop(stream, pt.fd[1], max_rows);
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
            while (fgets(BUFFER, GIT_LN_BUFSIZ, stream) == BUFFER) {
                argv[0] = memchr(BUFFER, '\n', GIT_LN_BUFSIZ);
                write(STDOUT_FILENO, BUFFER, argv[0] - BUFFER + 1);
            }
            close(pt.fd[0]);
            fclose(stream);
            log_trace("[%d, %d] bypass less", gl.pid, pt.pid);
            return 0;
        } else {
            // `fdopen` has failed. Clear the pipe.
            kill(pt.pid, SIGPIPE);
            log_trace("[%d, %d] fdopen failed", gl.pid, pt.pid);
            perror("fdopen failed");
            return 1;
        }
    }

    waitpid(less_pid, NULL, 0);
    kill(pt.pid, SIGPIPE);
    waitpid(gl.pid, NULL, 0);
    waitpid(pt.pid, NULL, 0);
    log_trace("The end.");
    return 0;
}
