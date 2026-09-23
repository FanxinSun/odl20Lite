odl_add_module(antenna_thrust
  SOURCES      src/antenna_thrust.cpp
  DEPENDS      odl::core odl::time odl::frames odl::dynamics
  TEST_SOURCES tests/antenna_thrust_tests.cpp)
