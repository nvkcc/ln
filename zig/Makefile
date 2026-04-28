current: run

build:
	zig build run -- --bound

install:
	zig build --prefix /usr/local -Doptimize=ReleaseFast install

run: install
	git -C /home/khang/repos/alatty ln --bound --all -n 300

test:
	zig build test
