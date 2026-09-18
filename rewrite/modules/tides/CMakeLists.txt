odl_add_module(tides
  SOURCES      src/increments.cpp src/pole.cpp src/solid_earth.cpp src/ocean.cpp
  DEPENDS      odl::core odl::time odl::frames odl::gravity odl::eop
  TEST_SOURCES tests/tides_tests.cpp)
target_include_directories(odl_tides_tests PRIVATE src)

if(BUILD_TESTING)
  odl_manifest_get(desai-ocean-pole-tide _desai)
  odl_manifest_get(iers-leap-seconds _leap)
  target_compile_definitions(odl_tides_tests PRIVATE
    ODL_FES2004_CNM_SNM="${ODL_CACHE_DIR}/fes2004-ocean-tide/fes2004_Cnm-Snm.dat"
    ODL_DESAI_POLE_COEF="${ODL_CACHE_DIR}/desai-ocean-pole-tide/extracted/desaiscopolecoef.txt"
    ODL_MANIFEST_CACHE_ROOT="${ODL_CACHE_DIR}"
    ODL_LEAP_SECOND_FILE="${_leap_CACHE_PATH}")
endif()
