cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

get_filename_component(SRC_DIR "../../src" REALPATH)
get_filename_component(INCLUDE_DIR "../../include" REALPATH)

# set(BOARD_ROOT ${CMAKE_SOURCE_DIR}/../../../)

macro(unit_test)
    file(GLOB TEST_SOURCES ${ARGN})

    target_sources(app PRIVATE ${TEST_SOURCES})

    set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

endmacro()