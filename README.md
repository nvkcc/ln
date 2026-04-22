# ln

`ln` (English: /lɑːn/) is a
[natural](https://wikipedia.org/wiki/Natural_logarithm) way to consume the
git log. This is an entirely opinionated wrapper for `git log`, and to some
extent it's really just a really convoluted shell alias for `git log`.

![git-ln](https://github.com/user-attachments/assets/c1e10bf0-eb37-4376-8e5d-a5890fe66459)

If you want something pretty darn close, feel free to simply

```
$ LN_FMT='%C(yellow)%h%C(auto)%d %Creset%s %C(241)(%C(246)%ar%C(241))%Creset'
$ git config set pretty.ln $LN_FMT
$ git log --graph --oneline --format=ln
```

## Architecture

`ln` spawns a total of 3 child processes.

1. An [`exec()`](https://linux.die.net/man/3/exec) call to `git log`.
2. A parser that reads the output of `git log` and does simple string parsing to
   make the output pretty.
3. An `exec()` call to `less` with `STDIN` piped so it appears as though we did
   something like `git log | less` in a shell.

And for that we need two pipes.

```
╭─────────╮   pipe #1   ╭─────────╮   pipe #2   ╭─────────╮
│ git log ├─────────────┤ printer ├─────────────┤  less   │
│   (A)   ├[1]───────[0]┤   (B)   ├[1]───────[0]┤   (C)   │
╰─────────╯             ╰─────────╯             ╰─────────╯
```

It is possible that the `git log` output is bigger than the (fixed) capacity of
the `pipe #1`. This is important because while debugging if you don't let `(B)`
read continuously from `pipe #1`, then there is a chance that `(A)` would block
on writing to that pipe. Likewise for `pipe #2`.

You might be wondering why must we fork one last time to run `less`. Why not
just call `exec()` on the parent process and hand control to less? That's pretty
much what it would look like with the shell command `git log | less` right?
Well, we fork a child process for `less` because it consumes `STDIN` lazily (it
doesn't flush/clear `STDIN` upon exit). So if we call `exec("less", ...)` on the
parent process and `less` exits early, then we might be back in the situation in
the previous paragraph where the `(B)` blocks on writing to `pipe #2`, causing
`ln` to survive as a zombie process.

## CLI argument handling

`ln` does basically no CLI argument parsing. It looks out for one and only one
flag, `--bound`. This will tell `ln` to truncate the `git log` output to the
height of the terminal window, if such a terminal window exists. All other CLI
arguments are passed straight on to the `exec()` call to `git log`.

## Installing

To install `ln`, simply run

```
$ make configure
$ make install
```

In particular, a binary called `git-ln` will be installed (to whichever prefix
you set in `CMakeLists.txt`). This allows you to run `git ln` (without the
hyphen), enabling commands like `git -C <repo> ln --all`.

## On memory safety

There is a quick and simple `valgrind` command in the Makefile to evaluate
`ln`'s memory profile. It does find _one_ leak when `ln` is compiled with
`glibc`, but that's due to `fdopen`'s leak ( (cf)
[here](https://sourceware.org/bugzilla/show_bug.cgi?id=31840) and
[here](https://stackoverflow.com/questions/78569261/)).
