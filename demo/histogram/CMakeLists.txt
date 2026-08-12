if(NOT TARGET ndl::ndl)
    # Being configured standalone (not pulled in via the root ndl
    # CMakeLists.txt through add_subdirectory), so set up our own project
    # and a minimal stand-in for the ndl::ndl interface target.
    cmake_minimum_required(VERSION 3.16)
    project(ndl_histogram_demo LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    add_library(ndl INTERFACE)
    add_library(ndl::ndl ALIAS ndl)
    target_include_directories(ndl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
endif()

add_executable(histogram histogram.cpp)
target_link_libraries(histogram PRIVATE ndl::ndl)

# Reuses the same source images as the unit tests and the other demos, and
# writes its own PNG outputs into its own build output folder -- same
# NDL_TEST_DATA_DIR / NDL_HISTOGRAM_OUTPUT_DIR pattern the other targets use,
# baked in at configure time so the binary finds its files regardless of
# where the build directory lives relative to the source tree.
target_compile_definitions(histogram PRIVATE
    NDL_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../unitTests/data"
    NDL_HISTOGRAM_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/output"
)
