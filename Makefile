BUILD_DIR := target

MAKEFILE_PATH := $(realpath $(lastword $(MAKEFILE_LIST)))
MAKEFILE_DIR := $(dir $(MAKEFILE_PATH))

# current: run
current: install

configure:
	cmake -S . -B $(BUILD_DIR)

build:
	cmake --build $(BUILD_DIR)

install: build
	cmake --install $(BUILD_DIR)

run: install
	# git -C /home/khang/repos/alatty ln --bound --all -n 10
	# git -C /home/khang/repos/alatty ln --all
	git -C /home/khang/repos/gitnu ln --bound --all -n 100
	#===================================================
	# git -C /home/khang/repos/gitnu ln --bound --all -n 100
