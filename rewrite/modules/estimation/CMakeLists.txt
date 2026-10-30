# estimation — L7 step 2 (SPEC-estimation.md v0.3): the batch estimator, the normal equations SCALED BY DEFAULT.  It depends on `core` alone (odl::Result),
# so that the NIST tests exercise it on plain rows; the force model, the integrators and the measurement models reach it through the fit that the exit gate
# needs, not through this module.
#
# FUSED MULTIPLY-ADD CONTRACTION IS OFF for the module and its tests: the order of operations and their roundings are part of the specification
# (EST-R-203, -204) and the registered figures replicate them (tools/estimation_sizing.cpp); the bounds hold with or without contraction, but the
# predictions and the bit-for-bit claims of EST-R-211 are registered for the plain operations.
odl_add_module(estimation
  SOURCES      src/dense.cpp src/normal_equations.cpp src/levenberg_marquardt.cpp
  DEPENDS      odl::core
  TEST_SOURCES tests/scale_tests.cpp tests/kernel_tests.cpp tests/nist_tests.cpp tests/decision_tests.cpp tests/hatch_tests.cpp tests/control_tests.cpp
               tests/lm_fixture_tests.cpp tests/lm_unit_tests.cpp tests/lm_dtu_tests.cpp tests/lm_nielsen_tests.cpp tests/lm_nist_tests.cpp)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(odl_estimation PRIVATE -ffp-contract=off)
endif()

if(BUILD_TESTING)
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(odl_estimation_tests PRIVATE -ffp-contract=off)
  endif()
  # The NIST StRD datasets are read from the pinned cache (the manifest's nist-strd-lls-<name> entries); nothing of NIST's is copied into the tree.
  foreach(_n norris pontius noint1 noint2 filip longley wampler1 wampler2 wampler3 wampler4 wampler5)
    odl_manifest_get(nist-strd-lls-${_n} _nist_${_n})
  endforeach()
  # step 3 (Levenberg-Marquardt): the 27 NIST nonlinear datasets, likewise read from the pinned cache
  foreach(_n bennett5 boxbod chwirut1 chwirut2 danwood eckerle4 enso gauss1 gauss2 gauss3 hahn1 kirby2 lanczos1 lanczos2 lanczos3 mgh09 mgh10 mgh17 misra1a misra1b misra1c misra1d nelson rat42 rat43 roszman1 thurber)
    odl_manifest_get(nist-strd-nls-${_n} _nist_nls_${_n})
  endforeach()
  target_compile_definitions(odl_estimation_tests PRIVATE ODL_MANIFEST_CACHE_ROOT="${ODL_CACHE_DIR}")
endif()
