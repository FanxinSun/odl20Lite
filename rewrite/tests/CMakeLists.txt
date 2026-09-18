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
