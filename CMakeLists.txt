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
option(NDL_BUILD_DOCS "Add a 'docs' target that generates Doxygen documentation (requires Doxygen; off by default since it's not part of the normal build/test loop)" OFF)

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
            # TIMEOUT is a safety net, not a tuned budget: convolution (the
            # slowest) takes ~10s on a throttled CPU, so 120s leaves ample
            # headroom while still failing a future hang (e.g. an infinite
            # loop regression) instead of blocking ctest indefinitely.
            set_tests_properties(demo_${demo_target} PROPERTIES LABELS "smoke" TIMEOUT 120)
        endforeach()
    endif()
endif()

if(NDL_BUILD_DOCS)
    find_package(Doxygen QUIET OPTIONAL_COMPONENTS dot)
    if(DOXYGEN_FOUND)
        # Doxyfile.in is the checked-in, hand-maintained template;
        # configure_file() resolves @CMAKE_SOURCE_DIR@/@CMAKE_BINARY_DIR@
        # into a real Doxyfile in the build directory so OUTPUT_DIRECTORY
        # is an absolute path regardless of where `doxygen` (or this
        # target) is actually invoked from.
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating HTML documentation with Doxygen -> ${CMAKE_CURRENT_BINARY_DIR}/docs/html/index.html"
            VERBATIM
        )
        # Doxygen_dot_FOUND (not the legacy DOXYGEN_DOT_FOUND) is the
        # variable actually populated when dot is requested via
        # OPTIONAL_COMPONENTS above -- FindDoxygen.cmake only maintains the
        # legacy name in the no-explicit-components code path.
        if(NOT Doxygen_dot_FOUND)
            message(STATUS "ndl: Doxygen found but graphviz's 'dot' was not -- the 'docs' target will still work, just without class/collaboration/include graphs")
        endif()

        # Tutorials: each demo's *actual* runtime output (the step()/
        # explain() walkthrough it prints, plus whatever PNGs it saves)
        # becomes a generated Doxygen markdown page -- see
        # docs/generate_tutorial.py's own comment for why this is a small,
        # format-specific parser rather than a general one. Requires the
        # demo targets to exist (NDL_BUILD_DEMOS) and a Python
        # interpreter to run the generator.
        if(NDL_BUILD_DEMOS)
            find_package(Python3 QUIET COMPONENTS Interpreter)
            if(Python3_FOUND)
                set(NDL_GENERATED_DOCS_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs/generated)
                set(NDL_TUTORIAL_PAGES "")
                set(_ndl_tutorial_targets multiview convolution morphology)
                set(_ndl_tutorial_labels multiview_tutorial convolution_tutorial morphology_tutorial)
                set(_ndl_tutorial_titles "Multiview Tutorial" "Convolution Tutorial" "Morphology Tutorial")
                list(LENGTH _ndl_tutorial_targets _ndl_tutorial_count)
                math(EXPR _ndl_tutorial_last "${_ndl_tutorial_count} - 1")
                foreach(_i RANGE ${_ndl_tutorial_last})
                    list(GET _ndl_tutorial_targets ${_i} _demo_target)
                    list(GET _ndl_tutorial_labels ${_i} _page_label)
                    list(GET _ndl_tutorial_titles ${_i} _page_title)
                    set(_capture_file ${CMAKE_CURRENT_BINARY_DIR}/docs/captured/${_demo_target}.txt)
                    set(_page_file ${NDL_GENERATED_DOCS_DIR}/${_page_label}.md)
                    add_custom_command(
                        OUTPUT ${_page_file}
                        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/docs/captured
                        COMMAND ${_demo_target} > ${_capture_file}
                        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/generate_tutorial.py
                                ${_capture_file} ${_page_label} ${_page_title}
                                $<TARGET_FILE_DIR:${_demo_target}>/output
                                ${NDL_GENERATED_DOCS_DIR}
                        WORKING_DIRECTORY $<TARGET_FILE_DIR:${_demo_target}>
                        DEPENDS ${_demo_target} ${CMAKE_CURRENT_SOURCE_DIR}/docs/generate_tutorial.py
                        COMMENT "Running ${_demo_target} and generating its tutorial page"
                        VERBATIM
                    )
                    list(APPEND NDL_TUTORIAL_PAGES ${_page_file})
                endforeach()
                add_custom_target(tutorials DEPENDS ${NDL_TUTORIAL_PAGES})
                add_dependencies(docs tutorials)
            else()
                message(STATUS "ndl: Python3 interpreter not found -- 'docs' will skip regenerating the tutorial pages (any previously generated ones are still used)")
            endif()
        endif()
    else()
        message(STATUS "ndl: NDL_BUILD_DOCS is ON but Doxygen was not found -- no 'docs' target added")
    endif()
endif()
