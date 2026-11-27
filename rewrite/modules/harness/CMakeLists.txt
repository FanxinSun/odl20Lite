# harness — L8 step 1 (plan/subplan_L8/L8-1.md, registered 2026-11-27; no SPEC: plan §3.11 exempts L8 from points 1 and 2): GNSS against IGS precise orbits.
# The statistics (each naming its formula), the IGS satellite-metadata SINEX resolver, and the seven- and fifteen-parameter orbit fits of the L7 exit gate's model, made a library.
# NO PATH IS EMBEDDED in the library: every file arrives as an argument (reprocheck, gate 13); the pinned paths are compile definitions of the TEST target only.
odl_add_module(harness
  SOURCES      src/statistics.cpp src/igs_metadata.cpp src/gnss_campaign.cpp
  DEPENDS      odl::core odl::time odl::eop odl::frames odl::ephemerides odl::gravity odl::tides odl::thirdbody odl::relativity
               odl::integrators odl::dynamics odl::forcemodel odl::estimation odl::macromodel odl::shadow odl::spacecraft odl::srp odl::ecom odl::io
  TEST_SOURCES tests/harness_unit_tests.cpp tests/gnss_baselines_tests.cpp)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(odl_harness PRIVATE -ffp-contract=off)
endif()

if(BUILD_TESTING)
  odl_manifest_get(iers-leap-seconds _h_leap)
  odl_manifest_get(de440s-spk _h_de440s)
  odl_manifest_get(naif-gm-de440 _h_gm)
  odl_manifest_get(eop-c04-20 _h_c04)
  odl_manifest_get(igs-satellite-metadata _h_snx)
  target_compile_options(odl_harness_tests PRIVATE -ffp-contract=off)
  target_compile_definitions(odl_harness_tests PRIVATE
    ODL_H_LEAP_SECOND_FILE="${_h_leap_CACHE_PATH}"
    ODL_H_DE440S_BSP="${_h_de440s_CACHE_PATH}"
    ODL_H_GM_DE440_TPC="${_h_gm_CACHE_PATH}"
    ODL_H_C04_FILE="${_h_c04_CACHE_PATH}"
    ODL_H_EGM2008_COEFFICIENTS="${ODL_CACHE_DIR}/egm2008-coefficients/extracted/EGM2008_to2190_TideFree"
    ODL_H_FES2004_CNM_SNM="${ODL_CACHE_DIR}/fes2004-ocean-tide/fes2004_Cnm-Snm.dat"
    ODL_H_MANIFEST_CACHE_ROOT="${ODL_CACHE_DIR}"
    ODL_H_SP3_022="${ODL_CACHE_DIR}/igs-final-sp3-2023-022/extracted/IGS0OPSFIN_20230220000_01D_15M_ORB.SP3"
    ODL_H_SP3_023="${ODL_CACHE_DIR}/igs-final-sp3-2023-023/extracted/IGS0OPSFIN_20230230000_01D_15M_ORB.SP3"
    ODL_H_SINEX="${_h_snx_CACHE_PATH}")
endif()
