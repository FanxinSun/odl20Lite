# L0's own test suite.  These test the step-3 machinery itself — the fetcher's
# refusals and the module boundary — not any physics, of which there is none yet.

add_executable(odl_toolchain_tests toolchain_smoke.cpp)
target_link_libraries(odl_toolchain_tests PRIVATE Catch2::Catch2WithMain odl_warnings)
catch_discover_tests(odl_toolchain_tests EXTRA_ARGS --warn NoAssertions)

# The fetcher's own behaviour, including every refusal path.
add_test(NAME fetcher.behaviour
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_fetch.py")

# The manifest verifies offline.  This is the property CI depends on (step 4):
# green means the pinned bytes are the bytes, not that the network was up.
add_test(NAME manifest.verify_offline
  COMMAND "${Python3_EXECUTABLE}" "${ODL_FETCH_TOOL}" verify)

add_test(NAME manifest.licences_permissive
  COMMAND "${Python3_EXECUTABLE}" "${ODL_FETCH_TOOL}" check-licences)

# Plan §5 constraint 8.  D7's one-line migration holds only while the tree stays
# inside the interchangeable part of the expected API; this is what keeps it so.
# The budget rows are the only class of number in this tree with no test behind
# them, and four of twenty-three were wrong by 1000x or more on 2026-09-18.
add_test(NAME budget.arithmetic
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/budgetcheck.py" --quiet)
add_test(NAME budget.checker_catches_the_historical_errors
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_budgetcheck.py")

add_test(NAME constraint8.result_only
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/constraint8.py")

# L5 step 4's own review round: speccheck.py's new duplicate-TEST_CASE check
# (gate 7) caught a real bug (two different modules' own tests claiming the
# same id) that nothing before it could see. This proves the checker itself
# catches that shape, the same "inject the historical error" discipline
# budget.checker_catches_the_historical_errors already uses for a different
# checker.
add_test(NAME speccheck.catches_duplicate_test_case_ids
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_speccheck_duplicate.py")

# L5 step 4's own THIRD review round: the TEST_CASE check above closes the
# .cpp side of this; this is its SPEC side -- does speccheck.py also catch
# the same id defined in two DIFFERENT spec .md files, not just within one
# file? Same discipline again, a third checker.
add_test(NAME speccheck.catches_cross_file_duplicate_ids
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_speccheck_cross_file_duplicate.py")

# --------------------------------------------------------------------------- #
# The module boundary is a link boundary — proved both ways.

add_subdirectory(boundary/alpha)
add_subdirectory(boundary/gamma)

# Positive: a module that declares its dependency can see it.
add_test(NAME boundary.declared_dependency_compiles
  COMMAND "${CMAKE_CXX_COMPILER}" -std=c++20 -fsyntax-only
          -I "${CMAKE_CURRENT_SOURCE_DIR}/boundary/alpha/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/boundary/violation.cpp")

# Negative: a module that does NOT declare it cannot.  gamma's include path is
# its own surface only, so the same source fails to compile.  A test that
# asserts a failure is the right shape here for the same reason FRAME-A-009 is:
# the property under test is that something is IMPOSSIBLE, and only a failing
# compile demonstrates that.
add_test(NAME boundary.undeclared_dependency_refused
  COMMAND "${CMAKE_CXX_COMPILER}" -std=c++20 -fsyntax-only
          -I "${CMAKE_CURRENT_SOURCE_DIR}/boundary/gamma/include"
          "${CMAKE_CURRENT_SOURCE_DIR}/boundary/violation.cpp")
set_tests_properties(boundary.undeclared_dependency_refused PROPERTIES WILL_FAIL TRUE)

# SPEC-perturbations PERT-A-009, the source half: the secular pole is DEFINED
# once, in gravity, and consumed by the pole tide.  The runtime half is in the
# tides tests.
add_test(NAME tides.one_secular_pole
         COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_one_secular_pole.py)

# A stale configure tests a subset and reports 100%. See the script's header for
# why this is a test rather than a line in tools/ci.sh.
add_test(NAME build.configure_is_current
         COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_configure_is_current.py
                 ${CMAKE_BINARY_DIR})

# The three L2 floors in one place with one reference point.  It links three
# modules on purpose: the numbers it compares live in three specifications, and
# stating each against its own reference point is what let two of them disagree
# by three orders of magnitude.
add_executable(odl_l2_floor_tests l2_floors.cpp)
target_link_libraries(odl_l2_floor_tests
  PRIVATE odl::core odl::relativity odl::time odl::frames odl::atmosphere
          Catch2::Catch2WithMain odl_warnings)
catch_discover_tests(odl_l2_floor_tests EXTRA_ARGS --warn NoAssertions)

# L4's exit gate (PLAN.md §3.5, amended 2026-09-24): every L4 force measured in
# one place, through DYN-R-023's registry, at reference points each row
# states. This file's own header explains why it needs a real ephemeris and
# leap table (srp/CMakeLists.txt's own pattern, below) where l2_floors.cpp did
# not.
add_executable(odl_l4_ranking_tests l4_ranking.cpp)
target_link_libraries(odl_l4_ranking_tests
  PRIVATE odl::core odl::time odl::frames odl::eop odl::atmosphere
          odl::ephemerides odl::dynamics odl::macromodel odl::srp_analytic
          odl::drag odl::srp odl::erp odl::antenna_thrust odl::ecom
          odl::gravity odl::thirdbody odl::relativity
          Catch2::Catch2WithMain odl_warnings)
if(BUILD_TESTING)
  odl_manifest_get(de440s-spk _de440s)
  odl_manifest_get(iers-leap-seconds _leap)
  odl_manifest_get(naif-gm-de440 _gm)
  target_compile_definitions(odl_l4_ranking_tests PRIVATE
    ODL_DE440S_BSP="${_de440s_CACHE_PATH}"
    ODL_LEAP_SECOND_FILE="${_leap_CACHE_PATH}"
    ODL_EGM2008_COEFFICIENTS="${ODL_CACHE_DIR}/egm2008-coefficients/extracted/EGM2008_to2190_TideFree"
    ODL_GM_DE440_TPC="${_gm_CACHE_PATH}"
    ODL_MANIFEST_CACHE_ROOT="${ODL_CACHE_DIR}")
endif()
catch_discover_tests(odl_l4_ranking_tests EXTRA_ARGS --warn NoAssertions)

# L5 step 4's own rule-2 check (the manager's own ruling, plan/subplan_L5/
# L5-4.md): reproducing a published, source-provided numerical SRP example
# through this tree's own real photon-pressure kernel, and running
# Sentinel-6's own REAL macromodel through that same real kernel. Links
# both odl::spacecraft (the data) and odl::srp_analytic (the kernel) on
# purpose, the same "linked here because the numbers being compared live in
# separate modules" reasoning l2_floors.cpp's own header states -- neither
# module's own DEPENDS gains the other for one cross-cutting test file.
add_executable(odl_spot5_appendix_tests spot5_appendix_tests.cpp)
target_link_libraries(odl_spot5_appendix_tests
  PRIVATE odl::core odl::macromodel odl::spacecraft odl::srp_analytic
          Catch2::Catch2WithMain odl_warnings)
catch_discover_tests(odl_spot5_appendix_tests EXTRA_ARGS --warn NoAssertions)

# L5's own exit gate (PLAN.md §3.6): every value modules/spacecraft can
# construct resolves to a citation, checked once, tree-wide, across every
# block and constellation -- not only within each block's own per-block
# suite. odl::macromodel linked directly (this file's own #include of
# cited.hpp/macromodel.hpp), the same "link what you directly use" choice
# odl_spot5_appendix_tests already makes for this exact pair of modules.
# ODL_SPEC_DIR: RESOLVES, not just EXISTS (the manager's own fourth-review
# correction) needs the registered source-key list read from
# SPEC-spacecraft.md itself at test time, not copied into the test by hand --
# the same "path supplied by CMake, not guessed at runtime" shape
# ODL_LEAP_SECOND_FILE/ODL_DE440S_BSP already use for odl_l4_ranking_tests,
# above.
add_executable(odl_l5_exit_gate_tests l5_exit_gate.cpp)
target_link_libraries(odl_l5_exit_gate_tests
  PRIVATE odl::core odl::macromodel odl::spacecraft
          Catch2::Catch2WithMain odl_warnings)
target_compile_definitions(odl_l5_exit_gate_tests PRIVATE
  ODL_SPEC_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../spec")
catch_discover_tests(odl_l5_exit_gate_tests EXTRA_ARGS --warn NoAssertions)
