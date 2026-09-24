odl_add_module(attitude
  SOURCES      src/attitude.cpp
  DEPENDS      odl::core
  TEST_SOURCES tests/attitude_tests.cpp tests/galileo_tests.cpp tests/glonass_tests.cpp
               tests/qzss_tests.cpp)
