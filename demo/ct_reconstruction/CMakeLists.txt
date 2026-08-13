if(NOT TARGET ndl::ndl)
    # Being configured standalone (not pulled in via the root ndl
    # CMakeLists.txt through add_subdirectory), so set up our own project
    # and a minimal stand-in for the ndl::ndl interface target.
    cmake_minimum_required(VERSION 3.16)
    project(ndl_ct_reconstruction_demo LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)

    add_library(ndl INTERFACE)
    add_library(ndl::ndl ALIAS ndl)
    target_include_directories(ndl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
endif()

add_executable(ct_reconstruction ct_reconstruction.cpp)
target_link_libraries(ct_reconstruction PRIVATE ndl::ndl)

# Writes its own PNG outputs into its own build output folder -- same
# NDL_*_OUTPUT_DIR pattern the other demo targets use, baked in at
# configure time so the binary finds its files regardless of where the
# build directory lives relative to the source tree. Unlike most other
# demos, this one needs no NDL_TEST_DATA_DIR -- the Shepp-Logan phantom is
# generated analytically, not loaded from a file.
target_compile_definitions(ct_reconstruction PRIVATE
    NDL_CT_RECONSTRUCTION_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/output"
)
