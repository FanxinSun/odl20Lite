odl_add_module(ecom
  SOURCES      src/ecom.cpp
  DEPENDS      odl::core odl::dynamics odl::ephemerides odl::frames odl::time
  TEST_SOURCES tests/ecom_tests.cpp)
