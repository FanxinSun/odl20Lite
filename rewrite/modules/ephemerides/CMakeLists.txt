odl_add_module(ephemerides
  SOURCES      src/ephemeris.cpp
  DEPENDS      odl::core odl::time odl::frames
  TEST_SOURCES tests/ephemerides_tests.cpp)

# CALCEPH is PRIVATE: a caller of odl::ephemerides must not have to know it
# exists, and must not be able to reach calceph_compute by accident.
target_link_libraries(odl_ephemerides PRIVATE calceph)

if(BUILD_TESTING)
  odl_manifest_get(iers-leap-seconds _leap)
  odl_manifest_get(de440s-spk _de440s)
  odl_manifest_get(de440-spk _de440)
  odl_manifest_get(de440-testpo _testpo)
  target_compile_definitions(odl_ephemerides_tests PRIVATE
    ODL_LEAP_SECOND_FILE="${_leap_CACHE_PATH}"
    ODL_DE440S_BSP="${_de440s_CACHE_PATH}"
    ODL_DE440_BSP="${_de440_CACHE_PATH}"
    ODL_TESTPO_440="${_testpo_CACHE_PATH}")
endif()
