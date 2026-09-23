odl_add_module(erp
  SOURCES      src/erp.cpp
  DEPENDS      odl::core odl::time odl::frames odl::ephemerides
               odl::macromodel odl::srp_analytic odl::attitude odl::dynamics
  TEST_SOURCES tests/erp_tests.cpp)

if(BUILD_TESTING)
  odl_manifest_get(de440s-spk _de440s)
  odl_manifest_get(iers-leap-seconds _leap)
  target_compile_definitions(odl_erp_tests PRIVATE
    ODL_DE440S_BSP="${_de440s_CACHE_PATH}"
    ODL_LEAP_SECOND_FILE="${_leap_CACHE_PATH}")
endif()
