cmake_minimum_required(VERSION 3.20.0)

# Supported boards:
#   esp32c6_devkitc/esp32c6/hpcore  (default)
#   esp32_devkitc/esp32/procpu
if(NOT DEFINED BOARD)
  set(BOARD esp32c6_devkitc/esp32c6/hpcore)
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(baiatool)

file(GLOB_RECURSE SOURCE_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/*.c
)

target_sources(app PRIVATE ${SOURCE_FILES})

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/includes
)

target_link_options(app PRIVATE -Wl,--wrap=esp_event_post)

# tf-psa-crypto extras/md.c has unused-parameter warnings that become errors
# via zephyr_interface; suppress them for that target only.
if(TARGET extras)
  target_compile_options(extras PRIVATE -Wno-unused-parameter)
endif()
