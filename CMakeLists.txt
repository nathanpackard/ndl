cmake_minimum_required(VERSION 3.16)
project(ndl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type (Debug, Release, RelWithDebInfo, MinSizeRel, Checked)" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel Checked)
endif()

# Checked: a custom build type for local dev/test iteration -- optimized
# like RelWithDebInfo, but (unlike every stock CMake build type except
# Debug) deliberately does NOT define NDEBUG, so every assert() in the
# codebase (Image::at()'s bounds check, the extent-matching checks on
# convolve()/add()/etc., ...) stays live. A plain Debug build (-O0) also
# keeps asserts live, but disables optimization entirely -- for a
# template-heavy, iterator/accessor-call-heavy library like this one, most
# of the resulting slowdown vs. Release is -O0 itself, not the asserts
# (measured directly: removing Image::at()'s bounds check from an otherwise
# unchanged Debug build only recovered about a quarter of its runtime on
# the convolution/morphology demos). -O2 -g here recovers the rest while
# keeping every check active, so `cmake -B build -DCMAKE_BUILD_TYPE=Checked`
# is the one to reach for when you want assert-level checking without
# Debug's full -O0 cost.
set(CMAKE_CXX_FLAGS_CHECKED "-O2 -g" CACHE STRING "Flags used by the CXX compiler for Checked builds (optimized, asserts still live)." FORCE)
set(CMAKE_C_FLAGS_CHECKED "-O2 -g" CACHE STRING "Flags used by the C compiler for Checked builds." FORCE)
set(CMAKE_EXE_LINKER_FLAGS_CHECKED "" CACHE STRING "Flags used for linking binaries for Checked builds." FORCE)
mark_as_advanced(CMAKE_CXX_FLAGS_CHECKED CMAKE_C_FLAGS_CHECKED CMAKE_EXE_LINKER_FLAGS_CHECKED)

# ndl is a header-only library. Headers are included as <ndl/...>, so
# consumers need the directory *containing* this checkout on their include
# path -- which is exactly what this INTERFACE target provides.
add_library(ndl INTERFACE)
add_library(ndl::ndl ALIAS ndl)
target_include_directories(ndl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

# The two vendored codec libraries imageIO/video_io.h wraps (save_video()/
# load_video()) -- see that header's own top comment, and each *_impl.cpp's
# own, for exactly why these need to be their own compiled translation
# units rather than header-only like the rest of ndl (name AND macro
# collisions between the two libraries if they ever shared a TU). Only
# whatever links video_io.h's declarations against actual code (i.e. links
# this target) pays for it; nothing else in ndl depends on it.
add_library(ndl_video_codec STATIC
    imageIO/mp4/minimp4_impl.cpp
    imageIO/mp4/minih264e_impl.cpp
)
target_link_libraries(ndl_video_codec PUBLIC ndl::ndl)
if(NOT MSVC)
    target_link_libraries(ndl_video_codec PUBLIC m)
endif()

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
option(NDL_BUILD_APPS "Build the ndl apps (interactive client/server programs, e.g. live_video_stream -- unlike demo/, these can't be captured into a static tutorial page)" ON)
option(NDL_BUILD_DOCS "Add a 'docs' target that generates Doxygen documentation (requires Doxygen; off by default since it's not part of the normal build/test loop)" OFF)

if(NDL_BUILD_TESTS)
    enable_testing()
    add_subdirectory(unitTests)
endif()

if(NDL_BUILD_DEMOS)
    add_subdirectory(demo/multiview)
    add_subdirectory(demo/convolution)
    add_subdirectory(demo/morphology)
    add_subdirectory(demo/histogram)
    add_subdirectory(demo/distance_transform)
    add_subdirectory(demo/summed_area_table)
    add_subdirectory(demo/motion)
    add_subdirectory(demo/ct_reconstruction)
    add_subdirectory(demo/ct_reconstruction_3d)
    add_subdirectory(demo/nd_viewer)
    add_subdirectory(demo/color_video)

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
        foreach(demo_target multiview convolution morphology histogram distance_transform summed_area_table motion ct_reconstruction nd_viewer color_video)
            add_test(NAME demo_${demo_target} COMMAND ${demo_target} WORKING_DIRECTORY $<TARGET_FILE_DIR:${demo_target}>)
            # TIMEOUT is a safety net, not a tuned budget: convolution (the
            # slowest) takes ~10s on a throttled CPU, so 120s leaves ample
            # headroom while still failing a future hang (e.g. an infinite
            # loop regression) instead of blocking ctest indefinitely.
            set_tests_properties(demo_${demo_target} PROPERTIES LABELS "smoke" TIMEOUT 120)
        endforeach()

        # ct_reconstruction_3d gets its own, much longer TIMEOUT rather
        # than joining the loop above: a 128^3 volume x 180 cone-beam
        # views is a genuinely bigger workload than any other demo here
        # (see that demo's own top comment), routinely taking past a
        # minute even where the rest finish in single-digit seconds.
        add_test(NAME demo_ct_reconstruction_3d COMMAND ct_reconstruction_3d WORKING_DIRECTORY $<TARGET_FILE_DIR:ct_reconstruction_3d>)
        set_tests_properties(demo_ct_reconstruction_3d PROPERTIES LABELS "smoke" TIMEOUT 600)
    endif()
endif()

if(NDL_BUILD_APPS)
    add_subdirectory(apps/live_video_stream)
    add_subdirectory(apps/bouncing_donut)

    if(NDL_BUILD_TESTS)
        # apps/ programs are interactive client/server processes, not
        # run-to-completion pipelines -- live_video_stream is SUPPOSED to
        # run forever until stopped, so it can't be smoke-tested the way
        # demo/'s own "run it, check the exit code" loop above tests those.
        # Its own self-test mode (NDL_LIVE_VIDEO_STREAM_SELFTEST, see that
        # app's own comment) makes it behave like an ordinary demo for
        # exactly this purpose: still does everything a real run does
        # (opens the file, builds the ring buffer, starts the WebSocket
        # server, streams a few frames), then stops itself -- so this is
        # still just "run it, expect exit 0," the same pattern every demo
        # smoke test above already uses.
        add_test(NAME app_live_video_stream COMMAND live_video_stream WORKING_DIRECTORY $<TARGET_FILE_DIR:live_video_stream>)
        set_tests_properties(app_live_video_stream PROPERTIES
            LABELS "smoke" TIMEOUT 30
            ENVIRONMENT "NDL_LIVE_VIDEO_STREAM_SELFTEST=1")

        # Same self-test-mode smoke-test pattern as live_video_stream above
        # (NDL_BOUNCING_DONUT_SELFTEST -- see that app's own comment).
        add_test(NAME app_bouncing_donut COMMAND bouncing_donut WORKING_DIRECTORY $<TARGET_FILE_DIR:bouncing_donut>)
        set_tests_properties(app_bouncing_donut PROPERTIES
            LABELS "smoke" TIMEOUT 30
            ENVIRONMENT "NDL_BOUNCING_DONUT_SELFTEST=1")
    endif()
endif()

if(NDL_BUILD_DOCS)
    find_package(Doxygen QUIET OPTIONAL_COMPONENTS dot)
    if(DOXYGEN_FOUND)
        # Doxyfile.in is the checked-in, hand-maintained template;
        # configure_file() resolves @CMAKE_CURRENT_SOURCE_DIR@/@CMAKE_CURRENT_BINARY_DIR@
        # (ndl's own location, not necessarily the overall build's root --
        # see Doxyfile.in's own top comment) into a real Doxyfile in the
        # build directory so OUTPUT_DIRECTORY is an absolute path
        # regardless of where `doxygen` (or this target) is actually
        # invoked from.
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
                set(_ndl_tutorial_targets multiview convolution morphology histogram distance_transform summed_area_table motion ct_reconstruction ct_reconstruction_3d nd_viewer color_video)
                set(_ndl_tutorial_labels multiview_tutorial convolution_tutorial morphology_tutorial histogram_tutorial distance_transform_tutorial summed_area_table_tutorial motion_tutorial ct_reconstruction_tutorial ct_reconstruction_3d_tutorial nd_viewer_tutorial color_video_tutorial)
                set(_ndl_tutorial_titles "Multiview Tutorial" "Convolution Tutorial" "Morphology Tutorial" "Histogram Tutorial" "Distance Transform Tutorial" "Summed-Area Table Tutorial" "Motion (Optical Flow / SIFT) Tutorial" "CT Reconstruction Tutorial" "3D Cone-Beam CT Reconstruction Tutorial" "N-D Viewer Tutorial" "Color Video Tutorial")
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
                                ${NDL_GENERATED_DOCS_DIR} 0
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

        # Apps (apps/) get their own, much simpler doc-generation step --
        # see docs/generate_app_doc.py's own top comment for why this
        # can't reuse generate_tutorial.py's "run it, capture stdout"
        # pipeline: an app is a live client/server program with no
        # "completed output" to capture. Still depends on the app's own
        # TARGET (so at least a compile failure is caught before docs
        # claims success) even though the generated page never runs it.
        if(NDL_BUILD_APPS)
            find_package(Python3 QUIET COMPONENTS Interpreter)
            if(Python3_FOUND)
                set(NDL_GENERATED_DOCS_DIR ${CMAKE_CURRENT_BINARY_DIR}/docs/generated)
                set(_app_page_file ${NDL_GENERATED_DOCS_DIR}/live_video_stream_app.md)
                add_custom_command(
                    OUTPUT ${_app_page_file}
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/generate_app_doc.py
                            ${CMAKE_CURRENT_SOURCE_DIR}/apps/live_video_stream/live_video_stream.cpp
                            live_video_stream_app "Live Video Stream App"
                            "./live_video_stream <videoPath> [port]"
                            live_video_stream
                            ${NDL_GENERATED_DOCS_DIR}
                    DEPENDS live_video_stream ${CMAKE_CURRENT_SOURCE_DIR}/apps/live_video_stream/live_video_stream.cpp ${CMAKE_CURRENT_SOURCE_DIR}/docs/generate_app_doc.py
                    COMMENT "Generating live_video_stream's app doc page (source only -- the app itself is never run)"
                    VERBATIM
                )
                set(_donut_page_file ${NDL_GENERATED_DOCS_DIR}/bouncing_donut_app.md)
                add_custom_command(
                    OUTPUT ${_donut_page_file}
                    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/docs/generate_app_doc.py
                            ${CMAKE_CURRENT_SOURCE_DIR}/apps/bouncing_donut/bouncing_donut.cpp
                            bouncing_donut_app "Bouncing Donut App"
                            "./bouncing_donut [port]"
                            bouncing_donut
                            ${NDL_GENERATED_DOCS_DIR}
                    DEPENDS bouncing_donut ${CMAKE_CURRENT_SOURCE_DIR}/apps/bouncing_donut/bouncing_donut.cpp ${CMAKE_CURRENT_SOURCE_DIR}/docs/generate_app_doc.py
                    COMMENT "Generating bouncing_donut's app doc page (source only -- the app itself is never run)"
                    VERBATIM
                )
                add_custom_target(app_docs DEPENDS ${_app_page_file} ${_donut_page_file})
                add_dependencies(docs app_docs)
            else()
                message(STATUS "ndl: Python3 interpreter not found -- 'docs' will skip regenerating the app doc pages (any previously generated ones are still used)")
            endif()
        endif()
    else()
        message(STATUS "ndl: NDL_BUILD_DOCS is ON but Doxygen was not found -- no 'docs' target added")
    endif()
endif()
