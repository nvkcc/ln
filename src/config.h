#ifndef a429bc252c63072c1a5942357ab9cc29a1d0e377
#define a429bc252c63072c1a5942357ab9cc29a1d0e377 1

// Colors
#define RESET "\e[m"
#define YELLOW "\e[33m"
#define DARK_GRAY "\e[38;5;240m"
#define LIGHT_GRAY "\e[38;5;246m"

// Binaries.
#define LESS "less" // Quick hack: make this invalid to debug the output.
#define GIT "git"

#define GIT_LN_BUF_SZ 2048

/// When printing to the exact height of the tty window, reserve this many lines
/// as padding for visual reasons such as to still be able to see the previous
/// prompt.
#define GIT_LN_SCREEN_VERTICAL_PAD 8

#define GIT_LN_MAX_ARGS 64

#endif
