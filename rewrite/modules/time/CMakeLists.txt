odl_add_module(time
  SOURCES      src/leap_table.cpp src/epoch.cpp
  DEPENDS      odl::core
  TEST_SOURCES tests/time_tests.cpp)
# ERFA is PRIVATE: it is how TDB−TT and the calendar conversions are computed,
# not part of this module's surface.  A caller of odl::time must not have to know
# that ERFA exists, and must not be able to reach it by accident.
target_link_libraries(odl_time PRIVATE erfa)

# The acceptance suite reads the real IERS table, from the manifest cache, so it
# tests the pinned bytes and not a copy that has drifted.
if(BUILD_TESTING)
  odl_manifest_get(iers-leap-seconds _leap)
  target_compile_definitions(odl_time_tests PRIVATE
    ODL_LEAP_SECOND_FILE="${_leap_CACHE_PATH}")
endif()
