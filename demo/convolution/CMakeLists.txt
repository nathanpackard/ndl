if(NOT TARGET ndl::ndl)
    # Being configured standalone (not pulled in via the root ndl
    # CMakeLists.txt through add_subdirectory), so set up our own project
    # and a minimal stand-in for the ndl::ndl interface target.
    cmake_minimum_required(VERSION 3.16)
    project(ndl_convolution_demo LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    add_library(ndl INTERFACE)
    add_library(ndl::ndl ALIAS ndl)
    target_include_directories(ndl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

    # Optional, same reasoning as the root CMakeLists.txt: this demo uses
    # fft.h's fftn()/ifftn(), which parallelize via std::execution::par --
    # TBB makes that actually multi-threaded under libstdc++, but everything
    # still works (just single-threaded) without it.
    find_package(TBB QUIET)
    if(TBB_FOUND)
        target_link_libraries(ndl INTERFACE TBB::tbb)
    endif()
endif()

add_executable(convolution convolution.cpp)
target_link_libraries(convolution PRIVATE ndl::ndl)

# Reuses the same source images as the unit tests, and writes its (larger,
# meant-for-visual-inspection) PNG outputs into its own build output folder --
# same NDL_TEST_DATA_DIR / NDL_TEST_OUTPUT_DIR pattern unitTests/CMakeLists.txt
# uses, baked in at configure time so the binary finds its files regardless of
# where the build directory lives relative to the source tree.
target_compile_definitions(convolution PRIVATE
    NDL_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../unitTests/data"
    NDL_CONVOLUTION_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/output"
)
