odl_add_module(dynamics
  SOURCES      src/dynamics.cpp src/variational.cpp
  DEPENDS      odl::core odl::time odl::frames odl::integrators
  TEST_SOURCES tests/dynamics_tests.cpp tests/stm_tests.cpp tests/sensitivity_tests.cpp)

if(BUILD_TESTING)
  # gravity is a TEST-ONLY dependency: DYN-A-018 asserts that SPEC-gravity §5's
  # entry points return values, so a gradient() entry point is additive
  # (GRAV-Q-006's structural half, DYN-R-028). The module itself must not depend
  # on any force — the surface is frozen before any force exists.
  target_link_libraries(odl_dynamics_tests PRIVATE odl::gravity)
endif()
