# L0's own test suite.  These test the step-3 machinery itself — the fetcher's
# refusals and the module boundary — not any physics, of which there is none yet.

add_executable(odl_toolchain_tests toolchain_smoke.cpp)
target_link_libraries(odl_toolchain_tests PRIVATE Catch2::Catch2WithMain odl_warnings)
catch_discover_tests(odl_toolchain_tests EXTRA_ARGS --warn NoAssertions)

# The fetcher's own behaviour, including every refusal path (L0 step 8: tests/devtools/fetch_tests.cpp, the port of test_fetch.py; the cases are
# tagged [fetcher] and run in-process on synthetic manifests).
add_test(NAME fetcher.behaviour
  COMMAND odl_devtools_tests "[fetcher]")

# The derivative of GFZ's file that the tree vendors (L0 step 8 group C1b; tests/devtools/gfz_derive_tests.cpp, tagged [gfz]): the transformation, its
# refusals, the vendored file's invariants, and the injection of an unchanged sunspot number shown refused.
add_test(NAME gfz.derivation
  COMMAND odl_devtools_tests "[gfz]")

# NOTICE, generated from the manifest (L0 step 8 group C2; tests/devtools/notice_tests.cpp, tagged [notice]): the generator's text on synthetic manifests,
# the licence texts quoted out of archives, the two stated substitutions, the tool's outcomes and refusals, and the real tree.  The generator had no test
# but ci.sh's gate 6, which is `notice --check`: the second test is that gate, as a test.
add_test(NAME notice.behaviour
  COMMAND odl_devtools_tests "[notice]")
add_test(NAME notice.matches_manifest
  COMMAND notice --check)

# The manifest verifies offline.  This is the property CI depends on (step 4):
# green means the pinned bytes are the bytes, not that the network was up.
# ODL_VERIFY_ARGS is empty, so every entry, literature included, is verified -- except under
# `tools/ci.sh --skip-literature`, GitHub's workflow alone (the top-level CMakeLists.txt says why).
add_test(NAME manifest.verify_offline
  COMMAND "${ODL_FETCH_TOOL}" verify ${ODL_VERIFY_ARGS})

add_test(NAME manifest.licences_permissive
  COMMAND "${ODL_FETCH_TOOL}" check-licences)

# Plan §5 constraint 8.  D7's one-line migration holds only while the tree stays
# inside the interchangeable part of the expected API; this is what keeps it so.
# The budget rows are the only class of number in this tree with no test behind
# them, and four of twenty-three were wrong by 1000x or more on 2026-09-18.
# The budget checker (ci.sh's gate 8) is C++ since L0 step 8, group C4: tools/budgetcheck.cpp.  budget.arithmetic runs the built tool on the real specifications, as it always did; the
# test that was Python (the name is kept) is the Catch2 cases of tests/devtools/budgetcheck_tests.cpp tagged [historical] -- the same four wrong rows, the same corrected forms, the same
# refusals.  What came with the port: the printed text of a run, every kind of finding and the exit codes, each scanner against the pattern it replaces, the unit table, and the real tree
# row by row (each real row alone, and each with its stated result made wrong).
add_test(NAME budget.arithmetic
  COMMAND budgetcheck --quiet)
add_test(NAME budget.checker_catches_the_historical_errors
  COMMAND odl_devtools_tests "[budgetcheck][historical]")
add_test(NAME budgetcheck.behaviour
  COMMAND odl_devtools_tests "[budgetcheck][behaviour]")
add_test(NAME budgetcheck.real_tree
  COMMAND odl_devtools_tests "[budgetcheck][real_tree]")

# The constraint-8 checker (ci.sh's gate 9) is C++ since L0 step 8, group C5: tools/constraint8.cpp.  constraint8.result_only runs the built tool on the real tree, as it always did.  What came with
# the port, in the Catch2 cases of tests/devtools/constraint8_tests.cpp (the Python tool had no test of its own): each of its two scanners against the pattern it replaces, what it prints and
# refuses on synthetic trees, which files it reads, and the real tree recounted another way.
add_test(NAME constraint8.result_only
  COMMAND constraint8)
add_test(NAME constraint8.behaviour
  COMMAND odl_devtools_tests "[constraint8][behaviour]")
add_test(NAME constraint8.real_tree
  COMMAND odl_devtools_tests "[constraint8][real_tree]")

# The literature checker (ci.sh's gate 11, plan section 5 constraint 3) and the factor-of-a-thousand register (gate 12, SPEC-dynamics DYN-R-040) are C++ since the same group:
# tools/literaturecheck.cpp and tools/unitcheck.cpp.  They have no ctest that runs the built tool (the gates do); what they have is the Catch2 cases of their own, which are new -- the Python
# tools had none: what each prints and refuses, which files each reads and what it looks for, the scanners of unitcheck against the patterns they replace, and the real tree (recounted another
# way, and with defects injected into a copy of the real sources).
add_test(NAME literaturecheck.behaviour
  COMMAND odl_devtools_tests "[literaturecheck][behaviour]")
add_test(NAME literaturecheck.real_tree
  COMMAND odl_devtools_tests "[literaturecheck][real_tree]")
add_test(NAME unitcheck.behaviour
  COMMAND odl_devtools_tests "[unitcheck][behaviour]")
add_test(NAME unitcheck.real_tree
  COMMAND odl_devtools_tests "[unitcheck][real_tree]")

# The specification coverage checker (ci.sh's gate 7) is C++ since L0 step 8, group C3: tools/speccheck.cpp.  Its three tests, which were Python (the names are kept), are Catch2
# cases of tests/devtools/speccheck_tests.cpp in the one devtools executable, each ctest running the tag of the test it replaces, with the same synthetic inputs and the same
# injected defects; the cases that came with the port (what a run prints, each kind of finding, the scanners against the patterns they replace, the tree walk's scope) and the
# real tree run as `speccheck.behaviour` and `speccheck.real_tree`.

# L5 step 4's own review round: the checker's new duplicate-TEST_CASE check
# (gate 7) caught a real bug (two different modules' own tests claiming the
# same id) that nothing before it could see. This proves the checker itself
# catches that shape, the same "inject the historical error" discipline
# budget.checker_catches_the_historical_errors already uses for a different
# checker.
add_test(NAME speccheck.catches_duplicate_test_case_ids
  COMMAND odl_devtools_tests "[speccheck][test_case_dup]")

# L5 step 4's own THIRD review round: the TEST_CASE check above closes the
# .cpp side of this; this is its SPEC side -- does the checker also catch
# the same id defined in two DIFFERENT spec .md files, not just within one
# file? Same discipline again, a third checker.
add_test(NAME speccheck.catches_cross_file_duplicate_ids
  COMMAND odl_devtools_tests "[speccheck][cross_file]")

# L6 step 1's own review: a lettered-suffix ACCEPTANCE row (SPEC-io-formats.md
# IOFM-A-004b, the same amendment-lettering convention EPH-A-001b already
# uses) was invisible to the discharge parser's own row-finding regex, so its
# own real discharge column went unread and a properly-covered requirement
# showed UNCOVERED. Fixed in the checker itself; proven here the same way.
add_test(NAME speccheck.discharges_lettered_acceptance_rows
  COMMAND odl_devtools_tests "[speccheck][lettered]")

# What came with the port: the printed text of a run, every kind of finding, the refusals and their exit codes, the scanners (each against the semantics of the regular expression it
# replaces), universal newlines, and what the tree-wide scan reads and does not (the root's own build*/ and data/: plan L0-8's ruling D12 a).  Then the real tree, which is gate 7's
# own run as a test (the way notice.matches_manifest is gate 6's).
add_test(NAME speccheck.behaviour
  COMMAND odl_devtools_tests "[speccheck][behaviour],[speccheck][scan]")
add_test(NAME speccheck.real_tree
  COMMAND odl_devtools_tests "[speccheck][real_tree]")

# L6 step 4 (SPEC-measmod.md): the numbers the specification freezes and the tests compare against come from tools that do
# NOT use the code under test, and each is reproduced here so that the number cannot drift away from its generator:
# the registry's counts of the pinned station files (MEAS-A-004), the finite-difference sizing frozen before any run (§6.2) and its
# Amendment A1 of 2026-10-06 (the stencil bounds of the whole modelled range, by scans of about ten seconds), and the header of
# independent reference values the C++ tests include.
add_test(NAME measmod.registry_facts_reproduce
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/measmod_registry_facts.py" --check)
add_test(NAME measmod.fd_sizing_reproduces
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/measmod_fd_sizing.py" --scan --check)
add_test(NAME measmod.reference_header_matches_its_generator
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/measmod_reference.py" --check)
add_test(NAME measmod.height_sensitivity_prediction_reproduces
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/measmod_height_sensitivity.py" --check)
add_test(NAME measmod.g5_selection_reproduces
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/measmod_g5_select.py" --check)

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
# tides tests.  The check is C++ since L0 step 8, group C5 (tests/devtools/one_secular_pole.cpp, the
# port of test_one_secular_pole.py): tides.one_secular_pole runs the built program on the real tree, as it always did; secular_pole.behaviour and
# secular_pole.real_tree are the Catch2 cases of tests/devtools/one_secular_pole_tests.cpp, which came with the port -- the defect injected (a second copy within the window, a definition
# that lost a constant or let them drift apart), what is printed, which sources are read, and the real tree recounted another way and with defects injected into a copy of it.
add_test(NAME tides.one_secular_pole
         COMMAND one_secular_pole)
add_test(NAME secular_pole.behaviour
  COMMAND odl_devtools_tests "[secular_pole][behaviour]")
add_test(NAME secular_pole.real_tree
  COMMAND odl_devtools_tests "[secular_pole][real_tree]")

# A stale configure tests a subset and reports 100%. See the program's header
# (tests/devtools/configure_is_current.cpp, the C++ port of test_configure_is_current.py, L0 step 8 group C5) for why this is a test rather than a line in tools/ci.sh.
# build.configure_is_current runs the built program on THIS build directory; configure_is_current.behaviour and .real_tree are the Catch2 cases of
# tests/devtools/configure_is_current_tests.cpp (files of chosen ages, what is printed and refused, the real tree against a build system generated in the future and at the dawn of time).
add_test(NAME build.configure_is_current
         COMMAND configure_is_current ${CMAKE_BINARY_DIR})
add_test(NAME configure_is_current.behaviour
  COMMAND odl_devtools_tests "[configure_is_current][behaviour]")
add_test(NAME configure_is_current.real_tree
  COMMAND odl_devtools_tests "[configure_is_current][real_tree]")

# The Unicode facts two of those tools rest on (L0 step 8 group C6; tests/devtools/unicode_tables_tests.cpp, tagged [unicode]): the ten non-ASCII characters whose upper case is pure ASCII (literaturecheck,
# `terms.upper().startswith("NOT ESTABLISHED")`) and the non-ASCII characters re.IGNORECASE equates with an ASCII letter (unitcheck, `\bkm\b|kilomet`), DERIVED from the Unicode Character Database
# files the manifest pins (ucd-unicodedata, ucd-specialcasing), and each tool held to the derivation over every scalar value.  They replace a one-off witness that was not in the tree.
# `parser` needs no file; `derivation` reads the pinned bytes from the cache.
add_test(NAME unicode_tables.parser
  COMMAND odl_devtools_tests "[unicode][parser]")
add_test(NAME unicode_tables.derivation
  COMMAND odl_devtools_tests "[unicode][derivation]")

# RKF7(8)'s tableau and the proof about it (ci.sh's gate 10, SPEC-integrators INTG-A-001) are C++ since L0 step 8, group C6: tools/rk_coefficients.cpp, on the devkit's BigInt and Rational
# (tools/devkit/tests/bigint_tests.cpp and rational_tests.cpp, tagged [devkit], which are ctests of their own).  rk_coefficients.check runs the built tool on the tree, as gate 10 does; what came with the
# port, in the Catch2 cases of tests/devtools/rk_coefficients_tests.cpp (the Python tool had no test of its own), is the rooted trees and their order conditions against what is known in closed
# form, the proof and each of its failures on damaged copies of the table, the generated header, the command line and the file it writes, and the real tree.
add_test(NAME rk_coefficients.check
  COMMAND rk_coefficients --check)
add_test(NAME rk_coefficients.behaviour
  COMMAND odl_devtools_tests "[rk_coefficients][behaviour]")
add_test(NAME rk_coefficients.real_tree
  COMMAND odl_devtools_tests "[rk_coefficients][real_tree]")

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

# L6's exit gate (the plan's §3.7; SPEC-measmod.md MEAS-A-102, -102b): every format round-trips on the layer's real pinned files and published
# fixtures, and a measurement model returns residual and partials for all three observation types from real-format sources. Linked here for the
# reason l5_exit_gate.cpp states: the property spans the layer. It reuses the measmod tests' own test-side inputs (g5_inputs.hpp: the real pass,
# the real orbit as the model's target through the real chain), so it takes the measmod tests' own compile definitions — the manifest-supplied
# paths — as they stand rather than restating them, and adds the one input of its own (the real ACS3 element set).
add_executable(odl_l6_exit_gate_tests l6_exit_gate.cpp)
target_link_libraries(odl_l6_exit_gate_tests
  PRIVATE odl::core odl::time odl::eop odl::frames odl::ephemerides odl::io odl::measmod
          Catch2::Catch2WithMain odl_warnings)
target_include_directories(odl_l6_exit_gate_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../modules/measmod/tests")
if(BUILD_TESTING)
  get_target_property(_measmod_test_definitions odl_measmod_tests COMPILE_DEFINITIONS)
  target_compile_definitions(odl_l6_exit_gate_tests PRIVATE ${_measmod_test_definitions})
  odl_manifest_get(celestrak-acs3-tle-20261005 _gate_tle)
  target_compile_definitions(odl_l6_exit_gate_tests PRIVATE ODL_ACS3_TLE_FILE="${_gate_tle_CACHE_PATH}")
endif()
catch_discover_tests(odl_l6_exit_gate_tests EXTRA_ARGS --warn NoAssertions)

# L7 step 1 (SPEC-gravity.md v1.4, SPEC-forcemodel.md): the numbers the specifications freeze and the tests compare against come from
# tools that do NOT use the code under test, and each is reproduced here so that a number cannot drift away from its generator — the
# gravity gradient's 145 reference tensors from the definition, and the force plugins' finite-difference sizing frozen before any run (§6.2).
add_test(NAME gravity.gradient_reference_header_matches_its_generator
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/gradient_reference.py" --verify "${CMAKE_CURRENT_SOURCE_DIR}/../modules/gravity/tests/gradient_reference.hpp")
add_test(NAME forcemodel.fd_sizing_reproduces
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/forcemodel_fd_sizing.py" --check)
add_test(NAME forcemodel.comparator_header_matches_its_generator
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/forcemodel_comparator_header.py" --verify "${CMAKE_CURRENT_SOURCE_DIR}/../modules/forcemodel/tests/comparator_polynomials.hpp")

# L7 step 2 (SPEC-estimation.md v0.3): the estimator's frozen figures come from a tool that does NOT use the code under test (exact rational arithmetic on the
# PINNED NIST files, 90-digit eigenvalues), and each is reproduced here so that a number in the specification or in the tests cannot drift from its generator.
add_test(NAME estimation.sizing_header_matches_generator
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/estimation_sizing.py" --cache "${ODL_CACHE_DIR}" --verify "${CMAKE_CURRENT_SOURCE_DIR}/../modules/estimation/tests/registered_figures.hpp")
add_test(NAME estimation.spec_table_matches_generator
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/estimation_sizing.py" --cache "${ODL_CACHE_DIR}" --verify-spec "${CMAKE_CURRENT_SOURCE_DIR}/../spec/SPEC-estimation.md")
