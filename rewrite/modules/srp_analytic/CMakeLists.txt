odl_add_module(srp_analytic
  SOURCES      src/srp_analytic.cpp
  DEPENDS      odl::core odl::macromodel
  TEST_SOURCES tests/srp_analytic_tests.cpp)
