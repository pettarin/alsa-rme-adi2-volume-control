# This file is part of the alsa-rme-adi2-volume-control project

cmake_minimum_required(VERSION 3.24)
project(alsa-rme-adi2-volume-control C)
include_directories(inc/)

# Configure CMake properties
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_COMPILE_WARNING_AS_ERROR true)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY bin)

# Configure custom properties
set(DBG_SANITIZED_BUILD_TYPES dbg_asan dbg_tsan dbg_ubsan)
set(DBG_BUILD_TYPES dbg ${DBG_SANITIZED_BUILD_TYPES})
set(ALL_BUILD_TYPES ${DBG_BUILD_TYPES} rls)
list(JOIN ALL_BUILD_TYPES ", " ALL_BUILD_TYPES_STR)
if(DEFINED CACHE{CMAKE_BUILD_TYPE})
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${ALL_BUILD_TYPES})
endif()

# Find ALSA
find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA REQUIRED alsa)

# Main executable
add_executable(alsa-rme-adi2-volume-control src/alsa-rme-adi2-volume-control.c)

if(NOT (CMAKE_BUILD_TYPE IN_LIST ALL_BUILD_TYPES))
    if(NOT (CMAKE_BUILD_TYPE STREQUAL ""))
        message(WARNING "Build type '${CMAKE_BUILD_TYPE}' is not among the supported build types: ${ALL_BUILD_TYPES_STR}")
    endif()
    set(CMAKE_BUILD_TYPE "rls")
    if(DEFINED CACHE{CMAKE_BUILD_TYPE})
        set(CMAKE_BUILD_TYPE "rls" CACHE STRING "Supported build types: ${ALL_BUILD_TYPES_STR}" FORCE)
    endif()
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(CMAKE_BUILD_TYPE IN_LIST DBG_BUILD_TYPES)
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -g -Wpedantic -Wmissing-format-attribute"
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "dbg_asan")
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -fno-omit-frame-pointer -fsanitize=address"
    )
    target_link_options(
        alsa-rme-adi2-volume-control
        PUBLIC -fno-omit-frame-pointer -fsanitize=address
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "dbg_tsan")
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -fsanitize=thread"
    )
    target_link_options(
        alsa-rme-adi2-volume-control
        PUBLIC -fsanitize=thread
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "dbg_ubsan")
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -fsanitize=undefined"
    )
    target_link_options(
        alsa-rme-adi2-volume-control
        PUBLIC -fsanitize=undefined
    )
elseif(CMAKE_BUILD_TYPE STREQUAL "rls")
    set(
        CMAKE_C_FLAGS
        "${CMAKE_C_FLAGS} -O2"
    )
endif()

# m (math) and ALSA are needed on all platforms
if(CMAKE_BUILD_TYPE IN_LIST DBG_SANITIZED_BUILD_TYPES)
    # atomic is needed on armhf
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
