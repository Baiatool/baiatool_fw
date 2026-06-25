cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

include(../unit.cmake)

project(sntp_unit_test)

target_sources(app PRIVATE
    src/main.c
    src/stubs.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../src/services/sntp.c
)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../includes
)

# sntp_simple is provided directly by stubs.c (CONFIG_SNTP=n disables the
# Zephyr library, so there is no duplicate symbol at link time).

# native_sim uses host glibc with -std=c17, which hides POSIX extensions by
# default. gmtime_r is POSIX.1-2008, not C17, so it needs this flag.
target_compile_definitions(app PRIVATE _POSIX_C_SOURCE=200809L)
