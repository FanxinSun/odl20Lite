odl_add_module(thirdbody
  SOURCES      src/attraction.cpp
  DEPENDS      odl::core odl::frames odl::time odl::ephemerides
  TEST_SOURCES tests/thirdbody_tests.cpp)

if(BUILD_TESTING)
  odl_manifest_get(naif-gm-de440 _gm)
  odl_manifest_get(de440s-spk _de440s)
  odl_manifest_get(iers-leap-seconds _leap)
  target_compile_definitions(odl_thirdbody_tests PRIVATE
    ODL_GM_DE440_TPC="${_gm_CACHE_PATH}"
    ODL_MANIFEST_CACHE_ROOT="${ODL_CACHE_DIR}"
    ODL_DE440S_BSP="${_de440s_CACHE_PATH}"
    ODL_LEAP_SECOND_FILE="${_leap_CACHE_PATH}")
endif()
