odl_add_module(gravity
  SOURCES      src/scaling.cpp src/degree.cpp src/coefficients.cpp src/secular_pole.cpp src/field.cpp
  DEPENDS      odl::core odl::time odl::frames
  TEST_SOURCES tests/gravity_tests.cpp)

if(BUILD_TESTING)
  odl_manifest_get(egm2008-coefficients _egm)
  odl_manifest_get(iers-leap-seconds _leap)
  target_compile_definitions(odl_gravity_tests PRIVATE
    ODL_EGM2008_COEFFICIENTS="${ODL_CACHE_DIR}/egm2008-coefficients/extracted/EGM2008_to2190_TideFree"
    ODL_MANIFEST_CACHE_ROOT="${ODL_CACHE_DIR}"
    ODL_LEAP_SECOND_FILE="${_leap_CACHE_PATH}")
endif()
