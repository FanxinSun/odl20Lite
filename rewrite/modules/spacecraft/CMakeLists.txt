odl_add_module(spacecraft
  SOURCES      src/spacecraft.cpp src/galileo.cpp
  DEPENDS      odl::core odl::macromodel
  TEST_SOURCES tests/spacecraft_tests.cpp tests/galileo_tests.cpp)
