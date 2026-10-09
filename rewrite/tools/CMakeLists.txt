# --------------------------------------------------------------------------- #
# The development tools' support library, and the tools themselves (plan L0 step 8, §5 constraint 11: every tool of this tree is C++).
#
# odl_devkit is SHA-256, JSON, inflate/gzip/tar/zip, Python-compatible text helpers, big integers and exact rationals (group C6) and process spawning, written for this tree.  It links
# NOTHING of the modules (odl::...), so that a generator can never share code with the thing it checks; tools/devkit/include is its whole surface.
#
# A tool is tools/<stem>.cpp, one function `run(argv, streams)` and a main.  It is compiled twice where something needs it before the build can
# build it (tools/fetch.cpp: cmake/OdlBuildHostTool.cmake, at configure time) and as an OBJECT library with its main left out
# (ODL_TOOL_NO_MAIN), which the tests link and call in-process on synthetic trees.

add_library(odl_devkit STATIC
  devkit/src/tool.cpp devkit/src/text.cpp devkit/src/pyfmt.cpp devkit/src/sha256.cpp devkit/src/json.cpp
  devkit/src/inflate.cpp devkit/src/archive.cpp devkit/src/fs.cpp devkit/src/process.cpp devkit/src/pytext.cpp devkit/src/bigint.cpp devkit/src/rational.cpp devkit/src/decimal.cpp devkit/src/pymath.cpp)
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

# The derivative of GFZ's Kp/ap/F10.7 file that the tree vendors (plan L0 step 8, group C1b; tools/gfz_derive.hpp says why): `gfz_derive <the pinned
# upstream file> <output>` reproduces it, and the object library is what the tests call in-process.
add_library(odl_tool_gfz_derive OBJECT gfz_derive.cpp)
target_compile_definitions(odl_tool_gfz_derive PUBLIC ODL_TOOL_NO_MAIN)
target_include_directories(odl_tool_gfz_derive PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(odl_tool_gfz_derive PUBLIC odl_devkit PRIVATE odl_warnings)

add_executable(gfz_derive gfz_derive.cpp)
target_link_libraries(gfz_derive PRIVATE odl_devkit odl_warnings)

# NOTICE, generated from the manifest (plan L0 step 8, group C2; tools/notice.cpp): `notice` writes it, `notice --check` is ci.sh's gate 6, the target
# `regenerate_notice` is the one command that rewrites the committed file, and the object library is what the tests call in-process.
add_library(odl_tool_notice OBJECT notice.cpp)
target_compile_definitions(odl_tool_notice PUBLIC ODL_TOOL_NO_MAIN ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
target_include_directories(odl_tool_notice PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(odl_tool_notice PUBLIC odl_devkit PRIVATE odl_warnings)

add_executable(notice notice.cpp)
target_compile_definitions(notice PRIVATE ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(notice PRIVATE odl_devkit odl_warnings)

add_custom_target(regenerate_notice
  COMMAND notice --root "${CMAKE_SOURCE_DIR}"
  COMMENT "NOTICE: written from the manifest and the pinned archives"
  VERBATIM)

# The specification coverage checker (plan L0 step 8, group C3; tools/speccheck.cpp): `speccheck` is ci.sh's gate 7, and the object library is what the tests call in-process
# (the three ctests that were Python, tests/CMakeLists.txt, and the real tree).
add_library(odl_tool_speccheck OBJECT speccheck.cpp)
target_compile_definitions(odl_tool_speccheck PUBLIC ODL_TOOL_NO_MAIN ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
target_include_directories(odl_tool_speccheck PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(odl_tool_speccheck PUBLIC odl_devkit PRIVATE odl_warnings)

add_executable(speccheck speccheck.cpp)
target_compile_definitions(speccheck PRIVATE ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(speccheck PRIVATE odl_devkit odl_warnings)

# The budget-row arithmetic checker (plan L0 step 8, group C4; tools/budgetcheck.cpp): `budgetcheck` is ci.sh's gate 8 and the ctest budget.arithmetic, and the object library is what the
# tests call in-process.  Its arithmetic is the Python's operation for operation (the same libm, nothing fused), so no multiplication is contracted into a fused one.
add_library(odl_tool_budgetcheck OBJECT budgetcheck.cpp)
target_compile_definitions(odl_tool_budgetcheck PUBLIC ODL_TOOL_NO_MAIN ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
target_include_directories(odl_tool_budgetcheck PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(odl_tool_budgetcheck PUBLIC odl_devkit PRIVATE odl_warnings)

add_executable(budgetcheck budgetcheck.cpp)
target_compile_definitions(budgetcheck PRIVATE ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(budgetcheck PRIVATE odl_devkit odl_warnings)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(odl_tool_budgetcheck PRIVATE -ffp-contract=off)
  target_compile_options(budgetcheck PRIVATE -ffp-contract=off)
endif()

# The checkers of groups C5 and C6 (plan L0 step 8; tools/constraint8.cpp, literaturecheck.cpp, unitcheck.cpp, rk_coefficients.cpp) and the two small checks that were Python tests (tests/devtools/one_secular_pole.cpp,
# configure_is_current.cpp): each is an executable that ci.sh's gates 9, 10, 11 and 12 or a ctest runs (constraint8.result_only, rk_coefficients.check, tides.one_secular_pole, build.configure_is_current), and an object library with
# its main left out, which the tests call in-process on synthetic trees.
foreach(checker constraint8 literaturecheck unitcheck rk_coefficients)
  add_library(odl_tool_${checker} OBJECT ${checker}.cpp)
  target_compile_definitions(odl_tool_${checker} PUBLIC ODL_TOOL_NO_MAIN ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
  target_include_directories(odl_tool_${checker} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(odl_tool_${checker} PUBLIC odl_devkit PRIVATE odl_warnings)

  add_executable(${checker} ${checker}.cpp)
  target_compile_definitions(${checker} PRIVATE ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
  target_link_libraries(${checker} PRIVATE odl_devkit odl_warnings)
endforeach()
# The generators of group C7 (plan L0 step 8; tools/gradient_reference.cpp, forcemodel_fd_sizing.cpp, forcemodel_comparator_header.cpp, legendre_reference.cpp): the numbers the specifications freeze and
# the tests compare against, computed from the definitions by tools that do not use the code under test.  Each is an executable (the ctests gravity.gradient_reference_header_matches_its_generator,
# forcemodel.fd_sizing_reproduces, forcemodel.comparator_header_matches_its_generator and gravity.legendre_reference_header_matches_its_generator run it) and an object library with its main left out,
# which the tests call in-process.  The comparator's header and the sizing use the gradient tool's exact polynomials, and the comparator the sizing's Phi, as the Python tools imported them: those executables
# take the objects of the tools they use.  No multiplication is contracted into a fused one: the sizing's floating point is the Python's, operation for operation.
foreach(gen gradient_reference forcemodel_fd_sizing forcemodel_comparator_header legendre_reference)
  add_library(odl_tool_${gen} OBJECT ${gen}.cpp)
  target_compile_definitions(odl_tool_${gen} PUBLIC ODL_TOOL_NO_MAIN ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
  target_include_directories(odl_tool_${gen} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(odl_tool_${gen} PUBLIC odl_devkit PRIVATE odl_warnings)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(odl_tool_${gen} PRIVATE -ffp-contract=off)
  endif()
endforeach()
add_executable(gradient_reference gradient_reference.cpp)
add_executable(forcemodel_fd_sizing forcemodel_fd_sizing.cpp $<TARGET_OBJECTS:odl_tool_gradient_reference>)
add_executable(forcemodel_comparator_header forcemodel_comparator_header.cpp $<TARGET_OBJECTS:odl_tool_gradient_reference> $<TARGET_OBJECTS:odl_tool_forcemodel_fd_sizing>)
add_executable(legendre_reference legendre_reference.cpp)
foreach(gen gradient_reference forcemodel_fd_sizing forcemodel_comparator_header legendre_reference)
  target_compile_definitions(${gen} PRIVATE ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
  target_link_libraries(${gen} PRIVATE odl_devkit odl_warnings)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${gen} PRIVATE -ffp-contract=off)
  endif()
endforeach()

# The generators and sizing tools of group C8 (plan L0 step 8; tools/measmod_reference.cpp, measmod_fd_sizing.cpp, measmod_registry_facts.cpp, measmod_g5_select.cpp, measmod_height_sensitivity.cpp): the numbers
# SPEC-measmod.md freezes and the tests compare against, from tools that do not use the code under test.  Each is an executable (the ctests measmod.reference_header_matches_its_generator,
# measmod.fd_sizing_reproduces, measmod.registry_facts_reproduce, measmod.g5_selection_reproduces and measmod.height_sensitivity_prediction_reproduces run it) and an object library with its main left out,
# which the tests call in-process.  The sizing imports the reference generator's functions, as the Python did: its executable takes that tool's objects.  No multiplication is contracted into a fused one: the
# sizing's floating point is the Python's, operation for operation.
foreach(gen measmod_reference measmod_registry_facts measmod_g5_select measmod_height_sensitivity measmod_fd_sizing)
  add_library(odl_tool_${gen} OBJECT ${gen}.cpp)
  target_compile_definitions(odl_tool_${gen} PUBLIC ODL_TOOL_NO_MAIN ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
  target_include_directories(odl_tool_${gen} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(odl_tool_${gen} PUBLIC odl_devkit PRIVATE odl_warnings)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(odl_tool_${gen} PRIVATE -ffp-contract=off)
  endif()
endforeach()
add_executable(measmod_reference measmod_reference.cpp)
add_executable(measmod_registry_facts measmod_registry_facts.cpp)
add_executable(measmod_g5_select measmod_g5_select.cpp)
add_executable(measmod_height_sensitivity measmod_height_sensitivity.cpp)
add_executable(measmod_fd_sizing measmod_fd_sizing.cpp $<TARGET_OBJECTS:odl_tool_measmod_reference>)
foreach(gen measmod_reference measmod_registry_facts measmod_g5_select measmod_height_sensitivity measmod_fd_sizing)
  target_compile_definitions(${gen} PRIVATE ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
  target_link_libraries(${gen} PRIVATE odl_devkit odl_warnings)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${gen} PRIVATE -ffp-contract=off)
  endif()
endforeach()

# The estimator's sizing generator of group C9 (plan L0 step 8; tools/estimation_sizing.cpp): the frozen figures of SPEC-estimation section 6 -- the committed modules/estimation/tests/registered_figures.hpp and the
# specification's generated table -- from exact arithmetic on the pinned NIST files, by a tool that does not use the code under test.  An executable (the ctests estimation.sizing_header_matches_generator and
# estimation.spec_table_matches_generator run it) and an object library with its main left out, which the tests call in-process.  No multiplication is contracted into a fused one: the double-precision recipe
# of the estimator (EST-R-203, -204) is replicated operation for operation.
add_library(odl_tool_estimation_sizing OBJECT estimation_sizing.cpp)
target_compile_definitions(odl_tool_estimation_sizing PUBLIC ODL_TOOL_NO_MAIN ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
target_include_directories(odl_tool_estimation_sizing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(odl_tool_estimation_sizing PUBLIC odl_devkit PRIVATE odl_warnings)
add_executable(estimation_sizing estimation_sizing.cpp)
target_compile_definitions(estimation_sizing PRIVATE ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(estimation_sizing PRIVATE odl_devkit odl_warnings)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(odl_tool_estimation_sizing PRIVATE -ffp-contract=off)
  target_compile_options(estimation_sizing PRIVATE -ffp-contract=off)
endif()

foreach(check one_secular_pole configure_is_current)
  add_library(odl_tool_${check} OBJECT ${CMAKE_SOURCE_DIR}/tests/devtools/${check}.cpp)
  target_compile_definitions(odl_tool_${check} PUBLIC ODL_TOOL_NO_MAIN ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
  target_include_directories(odl_tool_${check} PUBLIC ${CMAKE_SOURCE_DIR}/tests/devtools)
  target_link_libraries(odl_tool_${check} PUBLIC odl_devkit PRIVATE odl_warnings)

  add_executable(${check} ${CMAKE_SOURCE_DIR}/tests/devtools/${check}.cpp)
  target_compile_definitions(${check} PRIVATE ODL_TREE_ROOT="${CMAKE_SOURCE_DIR}")
  target_include_directories(${check} PRIVATE ${CMAKE_SOURCE_DIR}/tests/devtools)
  target_link_libraries(${check} PRIVATE odl_devkit odl_warnings)
endforeach()

if(BUILD_TESTING)
  # One executable for the devkit's own tests and the tools' (the Python tests of tests/ ported): each ctest of the old names runs the tag of the
  # test it replaces (tests/CMakeLists.txt), and every devkit case is its own ctest.
  add_executable(odl_devtools_tests
    devkit/tests/sha256_tests.cpp devkit/tests/json_tests.cpp devkit/tests/inflate_tests.cpp devkit/tests/archive_tests.cpp
    devkit/tests/text_tests.cpp devkit/tests/process_tests.cpp devkit/tests/pytext_tests.cpp devkit/tests/fs_tests.cpp devkit/tests/bigint_tests.cpp devkit/tests/rational_tests.cpp devkit/tests/decimal_tests.cpp devkit/tests/pymath_tests.cpp
    ${CMAKE_SOURCE_DIR}/tests/devtools/fetch_tests.cpp ${CMAKE_SOURCE_DIR}/tests/devtools/gfz_derive_tests.cpp
    ${CMAKE_SOURCE_DIR}/tests/devtools/notice_tests.cpp ${CMAKE_SOURCE_DIR}/tests/devtools/speccheck_tests.cpp
    ${CMAKE_SOURCE_DIR}/tests/devtools/budgetcheck_tests.cpp ${CMAKE_SOURCE_DIR}/tests/devtools/constraint8_tests.cpp ${CMAKE_SOURCE_DIR}/tests/devtools/literaturecheck_tests.cpp
    ${CMAKE_SOURCE_DIR}/tests/devtools/unitcheck_tests.cpp ${CMAKE_SOURCE_DIR}/tests/devtools/one_secular_pole_tests.cpp ${CMAKE_SOURCE_DIR}/tests/devtools/configure_is_current_tests.cpp
    ${CMAKE_SOURCE_DIR}/tests/devtools/unicode_tables_tests.cpp ${CMAKE_SOURCE_DIR}/tests/devtools/rk_coefficients_tests.cpp
    ${CMAKE_SOURCE_DIR}/tests/devtools/gradient_reference_tests.cpp ${CMAKE_SOURCE_DIR}/tests/devtools/forcemodel_fd_sizing_tests.cpp
    ${CMAKE_SOURCE_DIR}/tests/devtools/forcemodel_comparator_header_tests.cpp ${CMAKE_SOURCE_DIR}/tests/devtools/legendre_reference_tests.cpp
    ${CMAKE_SOURCE_DIR}/tests/devtools/measmod_height_sensitivity_tests.cpp ${CMAKE_SOURCE_DIR}/tests/devtools/measmod_g5_select_tests.cpp ${CMAKE_SOURCE_DIR}/tests/devtools/measmod_registry_facts_tests.cpp
    ${CMAKE_SOURCE_DIR}/tests/devtools/measmod_reference_tests.cpp ${CMAKE_SOURCE_DIR}/tests/devtools/measmod_fd_sizing_tests.cpp
    ${CMAKE_SOURCE_DIR}/tests/devtools/estimation_sizing_tests.cpp)
  target_include_directories(odl_devtools_tests PRIVATE devkit/tests)
  # measmod_fd_sizing_tests.cpp writes the Python's scans out again and requires the SAME DOUBLES as the tool (compiled without fused multiply-add above): no multiplication contracted there either
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(${CMAKE_SOURCE_DIR}/tests/devtools/measmod_fd_sizing_tests.cpp PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
    # estimation_sizing_tests.cpp writes the Python's double-precision recipe out again (the normal equations, the Cholesky, the bounds' chain) and requires the SAME DOUBLES as the tool: no multiplication contracted there either
    set_source_files_properties(${CMAKE_SOURCE_DIR}/tests/devtools/estimation_sizing_tests.cpp PROPERTIES COMPILE_OPTIONS -ffp-contract=off)
  endif()
  target_compile_definitions(odl_devtools_tests PRIVATE
    ODL_MANIFEST_PATH="${ODL_MANIFEST_FILE}" ODL_MANIFEST_CACHE_ROOT="${ODL_CACHE_DIR}" ODL_MANIFEST_VENDORED_ROOT="${ODL_VENDORED_DIR}"
    ODL_VENDORED_GFZ="${CMAKE_SOURCE_DIR}/data/vendored/gfz-kp-ap-f107/Kp_ap_Ap_SN_F107_since_1932.txt")
  target_link_libraries(odl_devtools_tests PRIVATE odl_tool_fetch odl_tool_gfz_derive odl_tool_notice odl_tool_speccheck odl_tool_budgetcheck odl_tool_constraint8 odl_tool_literaturecheck odl_tool_unitcheck odl_tool_rk_coefficients
    odl_tool_gradient_reference odl_tool_forcemodel_fd_sizing odl_tool_forcemodel_comparator_header odl_tool_legendre_reference
    odl_tool_measmod_reference odl_tool_measmod_fd_sizing odl_tool_measmod_registry_facts odl_tool_measmod_g5_select odl_tool_measmod_height_sensitivity odl_tool_estimation_sizing
    odl_tool_one_secular_pole odl_tool_configure_is_current odl_devkit Catch2::Catch2WithMain odl_warnings)

  # The fetcher's download path is tested against a stand-in `curl` (tests/devtools/fake_curl.cpp), built under that name into `fakebin/` beside the
  # test executable, which the tests find at run time and put first on PATH: no test of this tree touches the network.  (Its place is NOT a compile
  # definition: a build directory's path in an executable would make two builds differ, and tools/reprocheck.py compares them.)
  add_executable(odl_fake_curl ${CMAKE_SOURCE_DIR}/tests/devtools/fake_curl.cpp)
  set_target_properties(odl_fake_curl PROPERTIES OUTPUT_NAME curl RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fakebin)
  target_link_libraries(odl_fake_curl PRIVATE odl_warnings)
  add_dependencies(odl_devtools_tests odl_fake_curl)
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
