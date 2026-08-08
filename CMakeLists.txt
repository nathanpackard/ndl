cmake_minimum_required(VERSION 3.16)
project(ndl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ndl is a header-only library. Headers are included as <ndl/...>, so
# consumers need the directory *containing* this checkout on their include
# path -- which is exactly what this INTERFACE target provides.
add_library(ndl INTERFACE)
add_library(ndl::ndl ALIAS ndl)
target_include_directories(ndl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

option(NDL_BUILD_TESTS "Build the ndl unit test suite" ON)
option(NDL_BUILD_DEMOS "Build the ndl demo programs" ON)

if(NDL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(unitTests)
endif()

if(NDL_BUILD_DEMOS)
    add_subdirectory(demo/multiview)
endif()
