odl_add_module(relativity
  SOURCES      src/correction.cpp
  DEPENDS      odl::core odl::frames
  TEST_SOURCES tests/relativity_tests.cpp)
