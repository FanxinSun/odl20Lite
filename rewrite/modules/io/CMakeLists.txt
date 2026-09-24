odl_add_module(io
  SOURCES      src/sp3.cpp src/tle.cpp src/crd.cpp src/cpf.cpp src/iod.cpp src/sinex.cpp src/antex.cpp
               src/sp3_ephemeris.cpp
  DEPENDS      odl::core odl::time
  TEST_SOURCES tests/sp3_tests.cpp tests/tle_tests.cpp tests/crd_tests.cpp tests/cpf_tests.cpp
               tests/iod_tests.cpp tests/sinex_tests.cpp tests/antex_tests.cpp
               tests/sp3_ephemeris_tests.cpp)

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
endif()
