odl_add_module(drag
  SOURCES      src/drag.cpp
  DEPENDS      odl::core odl::time odl::eop odl::frames odl::atmosphere odl::dynamics
  TEST_SOURCES tests/drag_tests.cpp)

if(BUILD_TESTING)
  odl_manifest_get(iers-leap-seconds _leap)
  target_compile_definitions(odl_drag_tests PRIVATE
    ODL_LEAP_SECOND_FILE="${_leap_CACHE_PATH}")
endif()
