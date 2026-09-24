odl_add_module(spacecraft
  SOURCES      src/spacecraft.cpp
  DEPENDS      odl::core odl::macromodel
  TEST_SOURCES tests/spacecraft_tests.cpp)
