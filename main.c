#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// #define DEBUG

#define sp '' // separator character
#define SP "" // separator string

#define S(x) x, sizeof(x) - 1
#define fwrite_literal(x) fwrite(S(x), 1, stdout);
#define fwrite1(x, y) fwrite(x, y, 1, stdout);

// Colors
#define RESET "\e[m"
#define YELLOW "\e[33m"
#define DARK_GRAY "\e[38;5;240m"
#define LIGHT_GRAY "\e[38;5;246m"

#define READ 0
#define WRITE 1

// Tell the compiler that x is unlikely to be true.
#define unlikely(x) __builtin_expect(!!(x), 0)

#define exit_perror(msg, action)                                               \
  {                                                                            \
    perror(msg);                                                               \
    action;                                                                    \
  }

static inline void print_refs(char *refs) {
  char *v = refs;
  for (char *x = refs; *x != sp; *(v++) = *(x++)) {
    if (strncmp(x, S("origin")) == 0) {
      x += 6, *(v++) = '*';
    } else if (strncmp(x, S("\e[33m")) == 0) x[3] = '7'; // yellow -> gray.
  }
  fwrite1(refs, v - refs);
}

// v should be a pointer to the start of the sp-terminated date string.
// we aim to print only the first letter of the english bit.
static inline void print_relative_date(char *v) {
  // fwrite1(date, strchr(date, sp) - date - 1);
  // return;
  char *x = strchr(v, ' ');
  if (*(x + 1) == 'm' && *(x + 2) == 'o') *x = 'M'; // month -> M
  else *x = *(x + 1);
  fwrite1(v, x - v + 1);
}

// First `SP` is necessary to isolate out graph visuals
// order is <time> <hash> <commit message> <refs>
// %h  | abbreviated commit hash
// %ar | author date, relative
// %s  | subject
// %D  | ref names without the " (", ")" wrapping.
#define FMT_ARGS________ SP "%h %ar" SP "%<(80,trunc)%s %D" SP
#define FMT_ARGS_COLORED SP "%h %ar" SP "%<(80,trunc)%s %C(auto)%D" SP

typedef struct {
  char *hash, *date, *subject, *refs;
  short reflen, subjlen;
} commit;

static commit c;

// We're not gonna check for nullptrs here because life's too short. Just make
// sure the `--format` flag of `git-log` is properly checked, and we're gucci.
void print_git_log_line_colored() {
  // Print the commit hash.
  fwrite_literal(YELLOW);
  fwrite1(c.hash, c.date - c.hash);
  fwrite_literal(RESET);

  // Print the refs. (branches/tags/etc.)
  if (unlikely(c.reflen > 5)) { /* Print the refs with color */
    fwrite_literal(DARK_GRAY "{");
    print_refs(c.refs);
    fwrite_literal(DARK_GRAY "} " RESET);
  }

  // Print the subject.
  fwrite1(c.subject, c.subjlen + 1);

  // Print the relative time.
  fwrite_literal(DARK_GRAY "(" LIGHT_GRAY);
  print_relative_date(c.date);
  fwrite_literal(DARK_GRAY ")" RESET "\n");
}

// We're not gonna check for nullptrs here because life's too short. Just make
// sure the `--format` flag of `git-log` is properly checked, and we're gucci.
void print_git_log_line_uncolored() {
  // Print the commit hash.
  fwrite1(c.hash, c.date - c.hash);

  // Print the refs. (branches/tags/etc.)
  if (unlikely(c.reflen > 0)) { /* Print the refs with color */
    fwrite_literal("{");
    print_refs(c.refs);
    fwrite_literal("} ");
  }

  // Print the subject.
  fwrite1(c.subject, c.subjlen + 1);

  // Print the relative time.
  fwrite_literal("(");
  print_relative_date(c.date);
  fwrite_literal(")\n");
}

void print_git_log(int pipe, char is_atty) {
  FILE *fd = fdopen(pipe, "r");
  if (fd == NULL) exit_perror("fdopen on read-end failed.", return);

  void (*printer)() =
      is_atty ? &print_git_log_line_colored : print_git_log_line_uncolored;

  for (char line[512], *p; fgets(line, sizeof(line), fd) != NULL;) {
    c.hash = strchr(line, sp);

    // This line is just the graph visual.
    if (unlikely(c.hash == NULL)) {
      fwrite1(line, strlen(line));
      continue;
    } else {
      // Print the graph visual.
      fwrite1(line, c.hash - line);
    }

    c.date = strchr(++c.hash, ' ') + 1;
    c.subject = strchr(c.date, sp) + 1;

    c.subjlen = 80;
    while (c.subject[c.subjlen - 1] == ' ') c.subjlen--;

    c.refs = c.subject + 81;
    c.reflen = strchr(c.refs, sp) - c.refs;

    printer();
  }
}

int main(const int argc, const char **argv) {
  const char IS_ATTY = isatty(STDOUT_FILENO) ? 1 : 0;

  int p_log[2];

  if (pipe(p_log)) exit_perror("Unable to start pipe.", return 1);

  switch (fork()) {
  case -1:
    exit_perror("Fork failed.", return 1);
  case 0: { // Child process: run `git log`.
    const char *args[argc + 5];
    args[0] = "git", args[1] = "log";

    // Copy the remaining arguments over to `args`.
    memcpy(args + 2, argv + 1, (argc - 1) * sizeof(char *));

    // Core git-ln flavors.
    args[argc + 1] = "--graph";
    if (IS_ATTY) {
      args[argc + 2] = "--format=" FMT_ARGS_COLORED;
      args[argc + 3] = "--color=always";
      args[argc + 4] = NULL;
    } else {
      args[argc + 2] = "--format=" FMT_ARGS________;
      args[argc + 3] = NULL;
    }

    close(p_log[READ]);
    dup2(p_log[WRITE], STDOUT_FILENO);
    execvp("git", (char **)args);
  }
  default: { // Parent process.
    close(p_log[WRITE]);

#ifdef DEBUG // in debug mode, just print git log.
    print_git_log(p_log[READ], IS_ATTY);
    return 0;
#endif

    if (system("which less > /dev/null 2>&1")) { /* `less` isn't installed */
      print_git_log(p_log[READ], IS_ATTY);
    } else { /* `less` is installed */
      int p_less[2];
      if (pipe(p_less)) exit_perror("Unable to start pipe.", return 1);
      switch (fork()) {
      case -1:
        exit_perror("Fork failed.", return 1);
      case 0: { // Child process: run the printer.
        close(p_less[READ]);
        dup2(p_less[WRITE], STDOUT_FILENO);
        print_git_log(p_log[READ], IS_ATTY);
        break;
      }
      default: { // Parent process. We choose to run `less` with the parent
                 // because we want to hand back control to the TTY with
                 // `less` in the foreground.
        close(p_less[WRITE]);
        dup2(p_less[READ], STDIN_FILENO);
        execlp("less", "less", "-RF", NULL);
      }
      }
    }
    return 0;
  }
  }
}
