odl_add_module(spacecraft
  SOURCES      src/spacecraft.cpp src/galileo.cpp src/glonass.cpp src/qzss.cpp src/beidou.cpp
  DEPENDS      odl::core odl::macromodel
  TEST_SOURCES tests/spacecraft_tests.cpp tests/galileo_tests.cpp tests/glonass_tests.cpp
               tests/qzss_tests.cpp tests/beidou_tests.cpp)
