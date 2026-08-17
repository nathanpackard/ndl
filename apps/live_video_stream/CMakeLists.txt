if(NOT TARGET ndl::ndl)
    # Being configured standalone (not pulled in via the root ndl
    # CMakeLists.txt through add_subdirectory), so set up our own project
    # and minimal stand-ins for the ndl::ndl and ndl_video_codec targets --
    # same pattern demo/color_video/CMakeLists.txt uses.
    cmake_minimum_required(VERSION 3.16)
    project(ndl_live_video_stream_app LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    add_library(ndl INTERFACE)
    add_library(ndl::ndl ALIAS ndl)
    target_include_directories(ndl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

    add_library(ndl_video_codec STATIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../../imageIO/mp4/minimp4_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../imageIO/mp4/minih264e_impl.cpp
    )
    target_link_libraries(ndl_video_codec PUBLIC ndl::ndl)
    if(NOT MSVC)
        target_link_libraries(ndl_video_codec PUBLIC m)
    endif()
endif()

find_package(Threads REQUIRED)

add_executable(live_video_stream live_video_stream.cpp)
target_link_libraries(live_video_stream PRIVATE ndl::ndl ndl_video_codec Threads::Threads)

# Same NDL_TEST_DATA_DIR pattern every demo uses -- this app streams
# unitTests/data/waves.mp4, the same clip demo/color_video already loads.
# NDL_REPO_ROOT_DIR is this app's own WebSocketServer staticRoot -- the
# repo root (not just this app's own directory) specifically so its served
# HTML page can reference web/ndlviewer.js by a plain relative path
# (../../web/ndlviewer.js) without needing to copy that shared file into
# every app that wants it.
target_compile_definitions(live_video_stream PRIVATE
    NDL_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../unitTests/data"
    NDL_REPO_ROOT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../.."
)
