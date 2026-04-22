cmake_minimum_required(VERSION 3.14)

project(ln C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_FLAGS "-O3")
set(CMAKE_INSTALL_PREFIX /usr/local)

set(BUILD_TESTS OFF)

set(log_level TRACE)

add_executable(git-ln src/main.c src/log.c)

# Create compile_commands.json for clangd to latch on to.
file(CREATE_LINK ${CMAKE_BINARY_DIR}/compile_commands.json compile_commands.json SYMBOLIC)

install(TARGETS git-ln)
