odl_add_module(io
  SOURCES      src/sp3.cpp src/tle.cpp src/crd.cpp src/cpf.cpp src/iod.cpp src/sinex.cpp src/antex.cpp
               src/sp3_ephemeris.cpp src/horizons.cpp src/sgp4.cpp
  DEPENDS      odl::core odl::time odl::frames
  TEST_SOURCES tests/sp3_tests.cpp tests/tle_tests.cpp tests/crd_tests.cpp tests/cpf_tests.cpp
               tests/iod_tests.cpp tests/sinex_tests.cpp tests/antex_tests.cpp
               tests/sp3_ephemeris_tests.cpp tests/horizons_tests.cpp tests/sgp4_tests.cpp
               tests/sgp4_t01_tests.cpp)

# IOFM-A-030/031's own real-data holdout accuracy checks read two real SP3
# excerpts from disk rather than carrying them as string-literal fixtures --
# SP3 is fixed-column (SPEC-io-formats.md's own "verified placement, not
# hand transcription" discipline), and a real multi-epoch file is too large
# to place field-by-field the way the format's own worked examples are. The
# path is supplied by CMake, not guessed at runtime, the same shape
# ODL_SPEC_DIR/ODL_DE440S_BSP already use elsewhere in this tree.
if(BUILD_TESTING)
  target_compile_definitions(odl_io_tests PRIVATE
    ODL_IO_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")

  # IOHZ-A-008 reads the manifest's own pinned, real ACS3 Horizons capture
  # (FACTUAL-DATA-CITED, PROVENANCE.md §37.4) directly, the same
  # manifest-supplied-path shape ODL_DE440S_BSP/ODL_LEAP_SECOND_FILE already
  # use for l4_ranking_tests -- not a guessed path, and not a second copy of
  # the file placed under tests/data/.
  odl_manifest_get(horizons-acs3-vectors _horizons_acs3)
  target_compile_definitions(odl_io_tests PRIVATE
    ODL_HORIZONS_ACS3_TXT="${_horizons_acs3_CACHE_PATH}")

  # L6 step 3's own gate: the published SGP4 verification vectors
  # (SGP4-VER.TLE plus 32 .e files), extracted members of
  # vallado-sgp4-verification-vectors -- the same manifest-supplied-path
  # shape as ODL_HORIZONS_ACS3_TXT above, one directory rather than 33
  # separate definitions since the test reads every member by its own
  # already-known filename.
  target_compile_definitions(odl_io_tests PRIVATE
    ODL_SGP4_VECTORS_DIR="${ODL_CACHE_DIR}/vallado-sgp4-verification-vectors/extracted")

  # L6 step 3's T-01 gate (SPEC-io-sgp4.md IOSG-R-012, tests/sgp4_t01_tests.cpp): the vendored ACS3
  # element set, the EOP pin (finals2000A holds predictions for the capture's epochs), the
  # leap-second file and, for the comparison (IOSG-A-011/-A-012), the two vendored Horizons tables.
  # The tables were NOT wired in when the prediction (IOSG-A-010) was committed, on purpose: the build
  # gave that test no path to open one, so the numbers it froze cannot have seen them. The commit that
  # added the comparison added them here.
  odl_manifest_get(celestrak-acs3-tle-20261005 _t01_tle)
  odl_manifest_get(eop-finals2000a _t01_finals)
  odl_manifest_get(iers-leap-seconds _t01_leap)
  odl_manifest_get(horizons-acs3-t01-object-20261005 _t01_object)
  odl_manifest_get(horizons-acs3-t01-usertle-20261005 _t01_usertle)
  target_compile_definitions(odl_io_tests PRIVATE
    ODL_T01_TLE_FILE="${_t01_tle_CACHE_PATH}"
    ODL_FINALS2000A_FILE="${_t01_finals_CACHE_PATH}"
    ODL_LEAP_SECOND_FILE="${_t01_leap_CACHE_PATH}"
    ODL_T01_OBJECT_TXT="${_t01_object_CACHE_PATH}"
    ODL_T01_USERTLE_TXT="${_t01_usertle_CACHE_PATH}")
  # the legacy IAU-76/80 chain the prediction needs is ERFA's; PRIVATE, as in the frames tests
  target_link_libraries(odl_io_tests PRIVATE erfa)
endif()
