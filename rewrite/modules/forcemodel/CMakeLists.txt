# forcemodel — L7 step 1 (SPEC-forcemodel.md): L2's models as dyn::Force plugins.  A module of its own, APART FROM THE ESTIMATOR
# (the manager's ruling R1): steps 2-5 depend on dynamics' Force interface and on none of these.
odl_add_module(forcemodel
  SOURCES      src/common.cpp src/earth_orientation.cpp src/gravity_force.cpp src/third_body_force.cpp
               src/relativity_force.cpp src/tides_force.cpp src/truncation.cpp
  DEPENDS      odl::core odl::time odl::eop odl::frames odl::ephemerides odl::gravity odl::tides odl::thirdbody
               odl::relativity odl::dynamics
  TEST_SOURCES tests/registry_tests.cpp tests/fd_gate_tests.cpp tests/truncation_tests.cpp)

if(BUILD_TESTING)
  odl_manifest_get(iers-leap-seconds _leap)
  odl_manifest_get(de440s-spk _de440s)
  odl_manifest_get(naif-gm-de440 _gm)
  odl_manifest_get(eop-c04-20 _c04)
  odl_manifest_get(egm2008-coefficients _egm)
  odl_manifest_get(fes2004-ocean-tide _fes)
  odl_manifest_get(desai-ocean-pole-tide _desai)
  # ERFA is the INDEPENDENT composition the wrapper's rotation is checked against (SPEC-forcemodel FMOD-A-002): the tests link it.
  target_link_libraries(odl_forcemodel_tests PRIVATE erfa)
  target_compile_definitions(odl_forcemodel_tests PRIVATE
    ODL_LEAP_SECOND_FILE="${_leap_CACHE_PATH}"
    ODL_DE440S_BSP="${_de440s_CACHE_PATH}"
    ODL_GM_DE440_TPC="${_gm_CACHE_PATH}"
    ODL_C04_FILE="${_c04_CACHE_PATH}"
    ODL_EGM2008_COEFFICIENTS="${ODL_CACHE_DIR}/egm2008-coefficients/extracted/EGM2008_to2190_TideFree"
    ODL_FES2004_CNM_SNM="${ODL_CACHE_DIR}/fes2004-ocean-tide/fes2004_Cnm-Snm.dat"
    ODL_DESAI_POLE_COEF="${ODL_CACHE_DIR}/desai-ocean-pole-tide/extracted/desaiscopolecoef.txt"
    ODL_MANIFEST_CACHE_ROOT="${ODL_CACHE_DIR}"
    ODL_EGM2008_SHA256="${_egm_SHA256}"
    ODL_GM_DE440_SHA256="${_gm_SHA256}"
    ODL_DE440S_SHA256="${_de440s_SHA256}"
    ODL_C04_SHA256="${_c04_SHA256}"
    ODL_FES2004_SHA256="${_fes_SHA256}"
    ODL_DESAI_SHA256="${_desai_SHA256}")
endif()
