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
