if(NOT TARGET ndl::ndl)
    # Being configured standalone (not pulled in via the root ndl
    # CMakeLists.txt through add_subdirectory), so set up our own project
    # and a minimal stand-in for the ndl::ndl interface target.
    cmake_minimum_required(VERSION 3.16)
    project(ndl_multiview_demo LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    add_library(ndl INTERFACE)
    add_library(ndl::ndl ALIAS ndl)
    target_include_directories(ndl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
endif()

add_executable(multiview multiview.cpp)
target_link_libraries(multiview PRIVATE ndl::ndl)
