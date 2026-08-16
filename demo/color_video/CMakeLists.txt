if(NOT TARGET ndl::ndl)
    # Being configured standalone (not pulled in via the root ndl
    # CMakeLists.txt through add_subdirectory), so set up our own project
    # and minimal stand-ins for the ndl::ndl and ndl_video_codec targets.
    cmake_minimum_required(VERSION 3.16)
    project(ndl_color_video_demo LANGUAGES CXX)
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

add_executable(color_video color_video.cpp)
target_link_libraries(color_video PRIVATE ndl::ndl ndl_video_codec)

# Same NDL_..._OUTPUT_DIR / NDL_TEST_DATA_DIR pattern every other demo
# uses -- this one loads unitTests/data/waves.mp4.
target_compile_definitions(color_video PRIVATE
    NDL_COLOR_VIDEO_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/output"
    NDL_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../unitTests/data"
)
