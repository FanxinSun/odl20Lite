odl_add_module(io
  SOURCES      src/sp3.cpp src/tle.cpp src/crd.cpp src/cpf.cpp src/iod.cpp src/sinex.cpp src/antex.cpp
  DEPENDS      odl::core odl::time
  TEST_SOURCES tests/sp3_tests.cpp tests/tle_tests.cpp tests/crd_tests.cpp tests/cpf_tests.cpp
               tests/iod_tests.cpp tests/sinex_tests.cpp tests/antex_tests.cpp)
