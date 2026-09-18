#pragma once
// tides.hpp — the sub-daily signals the published EOP series do not contain.
//
// SPEC-eop.md §4.4.  The IERS series are REGULARISED: the diurnal and
// semi-diurnal variations caused by ocean tides and by tidal gravitation
// ("libration") have been removed, and TN36 eq. (5.11) says plainly that they
// must be added back after interpolation to obtain the instantaneous
// orientation.  Four corrections, all four required:
//
//   1  ocean tides, xp and yp      Tables 8.2a + 8.2b, 71 constituents   ≈ 6 mm
//   2  ocean tides, UT1 and LOD    Tables 8.3a + 8.3b, 71 constituents   ≈ 12 mm
//   3  libration, xp and yp        Table 5.1a, the 10 near-diurnal rows  ≈ 1 mm
//   4  libration, UT1 and LOD      Table 5.1b, 11 terms                  ≈ 1 mm
//
// The fourth is required ALTHOUGH THE IERS REFERENCE ROUTINE OMITS IT.
// `interp.f` calls PMUT1_OCEANS and PM_GRAVI and never UTLIBR, while TN36
// §5.5.3.1 says the UT1/LOD libration terms "should be included in that
// routine".  At about a millimetre of satellite position that is not negligible
// against ILRS normal points, so this tree follows the Conventions rather than
// the routine, and says so (EOP-R-043).

#include <odl/eop/fundamental_arguments.hpp>
#include <odl/time/epoch.hpp>

namespace odl::eop::tides {

// Arguments and arguments_at now live in the module's PUBLIC header, because
// L2 step 3's solid Earth tide is indexed by the same six and there is one
// definition of them.  See odl/eop/fundamental_arguments.hpp.

struct Correction {
    double dxp = 0.0;    ///< radians
    double dyp = 0.0;    ///< radians
    double dut1 = 0.0;   ///< seconds
    double dlod = 0.0;   ///< seconds
};

/// Tables 8.2a/8.2b and 8.3a/8.3b: 71 ocean-tide constituents.
[[nodiscard]] Correction ocean_tides(const Arguments& a) noexcept;
/// Tables 5.1a (near-diurnal rows only) and 5.1b: libration.
[[nodiscard]] Correction libration(const Arguments& a) noexcept;

[[nodiscard]] inline Correction subdaily(const Arguments& a) noexcept {
    const Correction o = ocean_tides(a);
    const Correction l = libration(a);
    return Correction{o.dxp + l.dxp, o.dyp + l.dyp, o.dut1 + l.dut1, o.dlod + l.dlod};
}

}  // namespace odl::eop::tides
