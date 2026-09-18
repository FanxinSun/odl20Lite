odl_add_module(eop
  SOURCES      src/tides.cpp src/series.cpp
  DEPENDS      odl::core odl::time
  TEST_SOURCES tests/eop_tests.cpp)

# ERFA is PRIVATE: it supplies the Delaunay arguments and GMST, and is not part
# of this module's surface.
target_link_libraries(odl_eop PRIVATE erfa)

if(BUILD_TESTING)
  # A module's OWN tests may see its internals.  The tidal models are an
  # implementation detail of `at()`, but EOP-R-008 requires each coefficient
  # table to be checked against the IERS routine's published test case, and a
  # test that could only reach them through the public surface would be testing
  # the interpolation at the same time.  The exception is one line, local to this
  # module, and does not widen anything for anyone else.
  target_include_directories(odl_eop_tests PRIVATE src)
  target_link_libraries(odl_eop_tests PRIVATE erfa)
  odl_manifest_get(iers-leap-seconds _leap)
  odl_manifest_get(eop-c04-20 _c04)
  odl_manifest_get(eop-finals2000a _finals)
  target_compile_definitions(odl_eop_tests PRIVATE
    ODL_LEAP_SECOND_FILE="${_leap_CACHE_PATH}"
    ODL_C04_FILE="${_c04_CACHE_PATH}"
    ODL_FINALS_FILE="${_finals_CACHE_PATH}")
endif()
