odl_add_module(atmosphere
  SOURCES      src/atmosphere.cpp src/space_weather.cpp
  DEPENDS      odl::core odl::time
  TEST_SOURCES tests/atmosphere_tests.cpp tests/space_weather_tests.cpp)

if(BUILD_TESTING)
  odl_manifest_get(gfz-kp-ap-f107 _gfz)
  odl_manifest_get(drao-fluxtable _drao)
  target_include_directories(odl_atmosphere_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
  target_compile_definitions(odl_atmosphere_tests PRIVATE
    ODL_GFZ_KP_AP_F107="${_gfz_CACHE_PATH}"
    ODL_DRAO_FLUXTABLE="${_drao_CACHE_PATH}")
endif()
