# GoogleTest, fetched and built alongside the project rather than requiring
# a system package -- reproducible across machines/CI without an extra
# install step, the approach Google's own docs recommend for CMake.
# GIT_SHALLOW avoids pulling the repo's full history for a single pinned tag.
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)
# Windows-only knob (keeps gtest's runtime library choice in sync with the
# rest of the build); harmless to set unconditionally on other platforms.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# Only GTest::gtest_main is used below (no mocking) -- skip building gmock
# and skip generating install rules for a dependency this project doesn't
# ship or install itself.
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

# Each test file below became its own executable (rather than one big
# ndl_unit_tests binary) so ctest can discover and report on every
# individual TEST() case -- gtest_discover_tests() below queries each
# binary at build time for its test list, so `ctest` shows e.g.
# "ImageCore.ImageConstruction" as its own row instead of one line for the
# whole suite.
function(ndl_add_test_executable name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE ndl::ndl GTest::gtest_main)
    # Baked in at configure time so each binary finds its input files
    # regardless of where the build directory lives relative to the source
    # tree.
    target_compile_definitions(${name} PRIVATE
        NDL_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
        NDL_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/output"
    )
    gtest_discover_tests(${name})
endfunction()

ndl_add_test_executable(image_core_tests        image_core_tests.cpp)
ndl_add_test_executable(image_view_tests        image_view_tests.cpp)
ndl_add_test_executable(image_arithmetic_tests  image_arithmetic_tests.cpp)
ndl_add_test_executable(image_convolution_tests image_convolution_tests.cpp)
ndl_add_test_executable(image_morphology_tests  image_morphology_tests.cpp)
ndl_add_test_executable(owned_image_tests       owned_image_tests.cpp)
ndl_add_test_executable(packed_bit_image_tests  packed_bit_image_tests.cpp)
ndl_add_test_executable(image_io_tests          image_io_tests.cpp)
ndl_add_test_executable(composite_tests         composite_tests.cpp)
ndl_add_test_executable(matrix_tests            matrix_tests.cpp)
ndl_add_test_executable(fft_tests               fft_tests.cpp)
ndl_add_test_executable(type_coverage_tests     type_coverage_tests.cpp)

# Negative-compilation regression tests: confirm the static_assert-based
# type restrictions in image/core.h, morphology.h, and image/owned.h
# stay in place. A static_assert is a hard compile error, not a SFINAE
# substitution failure (confirmed empirically -- even just asking
# std::is_constructible_v<OwnedImage<bool,2>, ...> fails the whole
# translation unit, since checking constructibility requires instantiating
# the class, which is exactly where the static_assert lives), so this is
# the only way to actually verify these restrictions still fire, short of
# reading the source by eye. Each negative_compile/*.cpp is deliberately
# written to fail to compile; WILL_FAIL inverts ctest's usual pass/fail so
# the test PASSES exactly when the compiler invocation fails, and FAILS
# (flagging a real regression -- the restriction silently stopped working)
# if it ever unexpectedly succeeds.
function(ndl_add_negative_compile_test name)
    add_test(NAME negative_${name}
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++17 -I${CMAKE_CURRENT_SOURCE_DIR}/../..
                -c ${CMAKE_CURRENT_SOURCE_DIR}/negative_compile/${name}.cpp
                -o ${CMAKE_CURRENT_BINARY_DIR}/negative_compile_${name}.o
    )
    set_tests_properties(negative_${name} PROPERTIES WILL_FAIL TRUE LABELS "negative_compile")
endfunction()

ndl_add_negative_compile_test(owned_image_bool)
ndl_add_negative_compile_test(complex_ordering)
ndl_add_negative_compile_test(double_bitwise)
