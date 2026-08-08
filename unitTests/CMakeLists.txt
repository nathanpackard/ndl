add_executable(ndl_unit_tests unitTests.cpp)
target_link_libraries(ndl_unit_tests PRIVATE ndl::ndl)

# Bake the absolute path to the test data directory in at configure time so
# the test binary finds its input files regardless of where it's built
# (previously this was guessed at runtime from the executable's location,
# which only worked if the build directory happened to sit directly under
# the source tree).
target_compile_definitions(ndl_unit_tests PRIVATE
    NDL_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    NDL_TEST_OUTPUT_DIR="${CMAKE_CURRENT_BINARY_DIR}/output"
)

add_test(NAME ndl_unit_tests COMMAND ndl_unit_tests)
