odl_add_module(integrators
  SOURCES      src/integrators.cpp
  DEPENDS      odl::core
  TEST_SOURCES tests/integrator_tests.cpp)

if(BUILD_TESTING)
  target_include_directories(odl_integrators_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
endif()
