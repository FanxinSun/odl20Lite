# Compile-only targets for the standalone real-data verification programs
# (TYAW-A-009/Q-005/Q-006, PROVENANCE.md §30.11/§30.12). Built by the normal
# `cmake --build` gate (ci.sh's own "gate 4") so an API drift in the
# libraries they call is caught at build time -- they do NOT run in CI (no
# pinned data present there), and are not registered with `add_test`.

add_executable(orbex_shape_e_check orbex_shape_e_check.cpp)
target_link_libraries(orbex_shape_e_check PRIVATE
  odl::attitude odl::frames odl::eop odl::ephemerides odl::time)

add_executable(orbex_noon_check orbex_noon_check.cpp)
target_link_libraries(orbex_noon_check PRIVATE
  odl::attitude odl::frames odl::eop odl::ephemerides odl::time)

add_executable(orbex_galileo_check orbex_galileo_check.cpp)
target_link_libraries(orbex_galileo_check PRIVATE
  odl::attitude odl::frames odl::eop odl::ephemerides odl::time)

add_executable(orbex_glonass_check orbex_glonass_check.cpp)
target_link_libraries(orbex_glonass_check PRIVATE
  odl::attitude odl::frames odl::eop odl::ephemerides odl::time)

add_executable(orbex_qzss_check orbex_qzss_check.cpp)
target_link_libraries(orbex_qzss_check PRIVATE
  odl::attitude odl::frames odl::eop odl::ephemerides odl::time)

add_executable(doris_jason_check doris_jason_check.cpp)
target_link_libraries(doris_jason_check PRIVATE
  odl::attitude odl::frames odl::eop odl::ephemerides odl::time)
