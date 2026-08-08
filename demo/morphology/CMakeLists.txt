if(NOT TARGET ndl::ndl)
    # Being configured standalone (not pulled in via the root ndl
    # CMakeLists.txt through add_subdirectory), so set up our own project
    # and a minimal stand-in for the ndl::ndl interface target.
    cmake_minimum_required(VERSION 3.16)
    project(ndl_morphology_demo LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    add_library(ndl INTERFACE)
    add_library(ndl::ndl ALIAS ndl)
    target_include_directories(ndl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
endif()

add_executable(morphology morphology.cpp)
target_link_libraries(morphology PRIVATE ndl::ndl)

# Reuses the same source images as the unit tests and the convolution demo,
# and writes its own PNG outputs into its own build output folder -- same
# NDL_TEST_DATA_DIR / NDL_MORPHOLOGY_OUTPUT_DIR pattern the other targets use,
# baked in at configure time so the binary finds its files regardless of
# where the build directory lives relative to the source tree.
target_compile_definitions(morphology PRIVATE
    NDL_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../unitTests/data"
    NDL_MORPHOLOGY_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/output"
)
