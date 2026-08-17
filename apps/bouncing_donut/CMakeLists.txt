if(NOT TARGET ndl::ndl)
    # Being configured standalone (not pulled in via the root ndl
    # CMakeLists.txt through add_subdirectory), so set up our own project
    # and a minimal stand-in for the ndl::ndl target -- same pattern
    # apps/live_video_stream/CMakeLists.txt uses. Unlike that app, no
    # ndl_video_codec target is needed here: there's no video file, no
    # decoder, just a procedurally-generated density field.
    cmake_minimum_required(VERSION 3.16)
    project(ndl_bouncing_donut_app LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    add_library(ndl INTERFACE)
    add_library(ndl::ndl ALIAS ndl)
    target_include_directories(ndl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
endif()

find_package(Threads REQUIRED)

add_executable(bouncing_donut bouncing_donut.cpp)
target_link_libraries(bouncing_donut PRIVATE ndl::ndl Threads::Threads)

# NDL_REPO_ROOT_DIR is this app's own WebSocketServer staticRoot -- the
# repo root, so its served HTML page can reference web/ndlviewer.js by a
# plain relative path without needing its own copy -- same reasoning as
# apps/live_video_stream/CMakeLists.txt's own comment.
target_compile_definitions(bouncing_donut PRIVATE
    NDL_REPO_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../.."
)
