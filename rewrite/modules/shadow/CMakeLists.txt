odl_add_module(shadow
  SOURCES      src/conical.cpp src/perspective.cpp
  DEPENDS      odl::core odl::frames
  TEST_SOURCES tests/shadow_tests.cpp tests/perspective_tests.cpp)
