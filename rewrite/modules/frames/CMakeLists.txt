odl_add_module(frames
  SOURCES      src/transform.cpp src/local.cpp
  DEPENDS      odl::core odl::time odl::eop
  TEST_SOURCES tests/frames_tests.cpp)

# ERFA is PRIVATE: a caller of odl::frames must not have to know it exists.
target_link_libraries(odl_frames PRIVATE erfa)

if(BUILD_TESTING)
  # FRAME-A-005 calls eraC2t06a directly as an independent check of the composed
  # chain, so the test target sees ERFA even though the module's surface does not.
  target_link_libraries(odl_frames_tests PRIVATE erfa)
  odl_manifest_get(iers-leap-seconds _leap)
  odl_manifest_get(eop-c04-20 _c04)
  target_compile_definitions(odl_frames_tests PRIVATE
    ODL_LEAP_SECOND_FILE="${_leap_CACHE_PATH}"
    ODL_C04_FILE="${_c04_CACHE_PATH}")
endif()
