cmake_minimum_required(VERSION 3.20.0)

set(BOARD esp32_devkitc/esp32/procpu)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(baiatool)

file(GLOB_RECURSE SOURCE_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/*.c
)

target_sources(app PRIVATE ${SOURCE_FILES})

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/includes
)
