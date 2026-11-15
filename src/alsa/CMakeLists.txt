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

if(CMAKE_BUILD_TYPE MATCHES "dbg")
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -g -Werror"
    )
endif()
if(CMAKE_BUILD_TYPE MATCHES "dbg_asan")
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -g -Werror -fsanitize=undefined -fsanitize=address"
    )
    target_link_options(
        alsa-rme-adi2-volume-control
        BEFORE PUBLIC -fsanitize=undefined PUBLIC -fsanitize=address
    )
endif()
if(CMAKE_BUILD_TYPE MATCHES "rls")
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -O2"
    )
endif()

target_include_directories(alsa-rme-adi2-volume-control PRIVATE ${ALSA_INCLUDE_DIRS})
target_link_libraries(alsa-rme-adi2-volume-control ${ALSA_LIBRARIES})
target_compile_options(alsa-rme-adi2-volume-control PRIVATE -Wall -Wextra)

# Install
install(TARGETS alsa-rme-adi2-volume-control DESTINATION bin)
