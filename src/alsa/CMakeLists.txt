# This file is part of the alsa-rme-adi2-volume-control project

cmake_minimum_required(VERSION 3.24)
project(alsa-rme-adi2-volume-control C)
include_directories(inc/)

# Configure CMake properties
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR true)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)

# Find ALSA
find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA REQUIRED alsa)

# Main executable
add_executable(alsa-rme-adi2-volume-control src/alsa-rme-adi2-volume-control.c)

if(CMAKE_BUILD_TYPE MATCHES "^dbg")
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -g -Werror -Wextra -Wpedantic -Wmissing-format-attribute"
    )
endif()

if(CMAKE_BUILD_TYPE MATCHES "dbg_asan")
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=address"
    )
    target_link_options(
        alsa-rme-adi2-volume-control
        PUBLIC -fno-omit-frame-pointer -fsanitize=address
    )
elseif(CMAKE_BUILD_TYPE MATCHES "dbg_tsan")
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -fsanitize=thread"
    )
    target_link_options(
        alsa-rme-adi2-volume-control
        PUBLIC -fsanitize=thread
    )
elseif(CMAKE_BUILD_TYPE MATCHES "dbg_ubsan")
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -fsanitize=undefined"
    )
    target_link_options(
        alsa-rme-adi2-volume-control
        PUBLIC -fsanitize=undefined
    )
elseif(CMAKE_BUILD_TYPE MATCHES "rls")
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -O2"
    )
elseif(CMAKE_BUILD_TYPE MATCHES "dbg")
    # empty case just to prevent falling into the default one (rls),
    # which applies optimizations
else()
    # the default case is rls, just to be safe
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -O2"
    )
endif()

if(
    (CMAKE_BUILD_TYPE MATCHES "dbg_asan") OR
    (CMAKE_BUILD_TYPE MATCHES "dbg_tsan") OR
    (CMAKE_BUILD_TYPE MATCHES "dbg_ubsan")
)
    target_link_libraries(
        alsa-rme-adi2-volume-control
        atomic
        m
        ${ALSA_LIBRARIES}
    )
else()
    target_link_libraries(
        alsa-rme-adi2-volume-control
        m
        ${ALSA_LIBRARIES}
    )
endif()

target_include_directories(alsa-rme-adi2-volume-control PRIVATE ${ALSA_INCLUDE_DIRS})
target_compile_options(alsa-rme-adi2-volume-control PRIVATE -Wall -Wextra)

# Install
install(TARGETS alsa-rme-adi2-volume-control DESTINATION bin)
