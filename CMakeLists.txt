cmake_minimum_required(VERSION 3.16)
project(ndl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# ndl is a header-only library. Headers are included as <ndl/...>, so
# consumers need the directory *containing* this checkout on their include
# path -- which is exactly what this INTERFACE target provides.
add_library(ndl INTERFACE)
add_library(ndl::ndl ALIAS ndl)
target_include_directories(ndl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

# Optional: fft.h's fftn()/ifftn() parallelize across an image's independent
# FFT "fibers" (rows, columns, etc.) using std::execution::par. libstdc++'s
# parallel algorithms need TBB linked in to actually run multi-threaded --
# without it they still compile and produce correct results, just silently
# fall back to running sequentially. So this is a nice-to-have, not a
# requirement: linked into the ndl INTERFACE target (and so propagated to
# every target that links ndl::ndl) whenever it's available, skipped
# otherwise.
find_package(TBB QUIET)
if(TBB_FOUND)
    target_link_libraries(ndl INTERFACE TBB::tbb)
    message(STATUS "ndl: found TBB -- std::execution::par (used by fft.h) will run multi-threaded")
else()
    message(STATUS "ndl: TBB not found -- std::execution::par (used by fft.h) will run single-threaded (still correct, just not parallel)")
endif()

option(NDL_BUILD_TESTS "Build the ndl unit test suite" ON)
option(NDL_BUILD_DEMOS "Build the ndl demo programs" ON)

if(NDL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(unitTests)
endif()

if(NDL_BUILD_DEMOS)
    add_subdirectory(demo/multiview)
    add_subdirectory(demo/convolution)
    add_subdirectory(demo/morphology)

    # Smoke tests: each demo is a real, multi-step pipeline (image I/O,
    # convolution, FFT, morphology) the unit tests don't exercise end-to-end
    # -- e.g. the OwnedImage/is_image_like overload-resolution bug found
    # earlier this project only surfaced when the convolution demo failed to
    # *compile*, not from any unit test. These don't check the demos'
    # output values -- that's the demos' own job, meant for a human to
    # read/view the printed explanations and saved PNGs -- just that each
    # one runs to completion without crashing or throwing (nonzero exit).
    #
    # Labeled "smoke" rather than left unlabeled alongside the unit tests:
    # each takes several seconds and writes real PNG files, so a plain
    # `ctest` (which runs every registered test regardless of label --
    # labels only ever *filter*, they don't opt a test out of the default
    # run) would still include them. Use `ctest -LE smoke` for the fast,
    # unit-tests-only loop, and `ctest -L smoke` to run just these.
    if(NDL_BUILD_TESTS)
        foreach(demo_target multiview convolution morphology)
            add_test(NAME demo_${demo_target} COMMAND ${demo_target} WORKING_DIRECTORY $<TARGET_FILE_DIR:${demo_target}>)
            set_tests_properties(demo_${demo_target} PROPERTIES LABELS "smoke")
        endforeach()
    endif()
endif()
