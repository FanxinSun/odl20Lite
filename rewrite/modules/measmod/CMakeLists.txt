odl_add_module(measmod
  SOURCES      src/registry.cpp
  DEPENDS      odl::core odl::time odl::eop odl::frames odl::ephemerides odl::io
  TEST_SOURCES tests/registry_tests.cpp)

# ERFA is PRIVATE: a caller of odl::measmod must not have to know it exists (the geodetic conversions are its).
target_link_libraries(odl_measmod PRIVATE erfa)

if(BUILD_TESTING)
  # The registry's acceptance tests read the manifest's own pinned station files (SPEC-measmod.md §8.1) — the
  # manifest-supplied-path shape the other modules use, never a guessed path and never a second copy of a file.
  target_link_libraries(odl_measmod_tests PRIVATE erfa)
  odl_manifest_get(iers-leap-seconds _leap)
  odl_manifest_get(ilrs-slrf2020-20260205 _slrf)
  odl_manifest_get(ilrs-slrecc-une-20260527 _slrecc)
  odl_manifest_get(itrf2020-psd-slr _slrpsd)
  target_compile_definitions(odl_measmod_tests PRIVATE
    ODL_LEAP_SECOND_FILE="${_leap_CACHE_PATH}"
    ODL_SLRF2020_FILE="${_slrf_CACHE_PATH}"
    ODL_SLRF2020_SHA256="${_slrf_SHA256}"
    ODL_SLR_ECC_FILE="${_slrecc_CACHE_PATH}"
    ODL_SLR_ECC_SHA256="${_slrecc_SHA256}"
    ODL_SLR_PSD_FILE="${_slrpsd_CACHE_PATH}"
    ODL_SLR_PSD_SHA256="${_slrpsd_SHA256}")
endif()
