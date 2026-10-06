# --------------------------------------------------------------------------- #
# The development tools' support library, and the tools themselves (plan L0 step 8, §5 constraint 11: every tool of this tree is C++).
#
# odl_devkit is SHA-256, JSON, inflate/gzip/tar/zip, Python-compatible text helpers and process spawning, written for this tree.  It links
# NOTHING of the modules (odl::...), so that a generator can never share code with the thing it checks; tools/devkit/include is its whole surface.
#
# A tool is tools/<stem>.cpp, one function `run(argv, streams)` and a main.  It is compiled twice where something needs it before the build can
# build it (tools/fetch.cpp: cmake/OdlBuildHostTool.cmake, at configure time) and as an OBJECT library with its main left out
# (ODL_TOOL_NO_MAIN), which the tests link and call in-process on synthetic trees.

add_library(odl_devkit STATIC
  devkit/src/tool.cpp devkit/src/text.cpp devkit/src/pyfmt.cpp devkit/src/sha256.cpp devkit/src/json.cpp
  devkit/src/inflate.cpp devkit/src/archive.cpp devkit/src/fs.cpp devkit/src/process.cpp)
target_include_directories(odl_devkit PUBLIC devkit/include)
target_compile_features(odl_devkit PUBLIC cxx_std_20)
target_link_libraries(odl_devkit PRIVATE odl_warnings)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(odl_devkit PRIVATE -ffp-contract=off)
endif()

add_library(odl_tool_fetch OBJECT fetch.cpp)
target_compile_definitions(odl_tool_fetch PUBLIC ODL_TOOL_NO_MAIN ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
target_include_directories(odl_tool_fetch PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(odl_tool_fetch PUBLIC odl_devkit PRIVATE odl_warnings)

if(BUILD_TESTING)
  # One executable for the devkit's own tests and the tools' (the Python tests of tests/ ported): each ctest of the old names runs the tag of the
  # test it replaces (tests/CMakeLists.txt), and every devkit case is its own ctest.
  add_executable(odl_devtools_tests
    devkit/tests/sha256_tests.cpp devkit/tests/json_tests.cpp devkit/tests/inflate_tests.cpp devkit/tests/archive_tests.cpp
    devkit/tests/text_tests.cpp devkit/tests/process_tests.cpp
    ${CMAKE_SOURCE_DIR}/tests/devtools/fetch_tests.cpp)
  target_include_directories(odl_devtools_tests PRIVATE devkit/tests)
  target_compile_definitions(odl_devtools_tests PRIVATE
    ODL_MANIFEST_PATH="${ODL_MANIFEST_FILE}" ODL_MANIFEST_CACHE_ROOT="${ODL_CACHE_DIR}")
  target_link_libraries(odl_devtools_tests PRIVATE odl_tool_fetch odl_devkit Catch2::Catch2WithMain odl_warnings)
  catch_discover_tests(odl_devtools_tests TEST_SPEC "[devkit]" TEST_PREFIX "devkit: " EXTRA_ARGS --warn NoAssertions)
endif()

# --------------------------------------------------------------------------- #
# Compile-only targets for the standalone real-data verification programs
# (TYAW-A-009/Q-005/Q-006, PROVENANCE.md §30.11/§30.12). Built by the normal
# `cmake --build` gate (ci.sh's own "gate 4") so an API drift in the
# libraries they call is caught at build time -- they do NOT run in CI (no
# pinned data present there), and are not registered with `add_test`.

# L6 step 1: every tool below now reads SP3 through odl::io's own one SP3
# reader (SPEC-io-formats.md) instead of the ad hoc Sp3Row parser each used
# to carry independently -- odl::io added to each target's own link line.

add_executable(orbex_shape_e_check orbex_shape_e_check.cpp)
target_link_libraries(orbex_shape_e_check PRIVATE
  odl::attitude odl::frames odl::eop odl::ephemerides odl::time odl::io)

add_executable(orbex_noon_check orbex_noon_check.cpp)
target_link_libraries(orbex_noon_check PRIVATE
  odl::attitude odl::frames odl::eop odl::ephemerides odl::time odl::io)

add_executable(orbex_galileo_check orbex_galileo_check.cpp)
target_link_libraries(orbex_galileo_check PRIVATE
  odl::attitude odl::frames odl::eop odl::ephemerides odl::time odl::io)

add_executable(orbex_glonass_check orbex_glonass_check.cpp)
target_link_libraries(orbex_glonass_check PRIVATE
  odl::attitude odl::frames odl::eop odl::ephemerides odl::time odl::io)

add_executable(orbex_qzss_check orbex_qzss_check.cpp)
target_link_libraries(orbex_qzss_check PRIVATE
  odl::attitude odl::frames odl::eop odl::ephemerides odl::time odl::io)

add_executable(doris_jason_check doris_jason_check.cpp)
target_link_libraries(doris_jason_check PRIVATE
  odl::attitude odl::frames odl::eop odl::ephemerides odl::time odl::io)
