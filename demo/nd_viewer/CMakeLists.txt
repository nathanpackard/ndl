if(NOT TARGET ndl::ndl)
    # Being configured standalone (not pulled in via the root ndl
    # CMakeLists.txt through add_subdirectory), so set up our own project
    # and a minimal stand-in for the ndl::ndl interface target.
    cmake_minimum_required(VERSION 3.16)
    project(ndl_nd_viewer_demo LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    add_library(ndl INTERFACE)
    add_library(ndl::ndl ALIAS ndl)
    target_include_directories(ndl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
endif()

add_executable(nd_viewer nd_viewer.cpp)
target_link_libraries(nd_viewer PRIVATE ndl::ndl)

# Same NDL_..._OUTPUT_DIR pattern every other demo uses, baked in at
# configure time so the binary finds its own output folder regardless of
# where the build directory lives relative to the source tree.
target_compile_definitions(nd_viewer PRIVATE
    NDL_ND_VIEWER_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/output"
)
