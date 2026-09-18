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

#include <odl/time/epoch.hpp>

namespace odl::eop::tides {

/// The six arguments of TN36 Tables 5.1 and 8.2/8.3, in radians.
struct Arguments {
    double gamma;   ///< GMST + pi
    double l;       ///< Delaunay: mean anomaly of the Moon
    double lp;      ///< mean anomaly of the Sun
    double F;       ///< L − Omega
    double D;       ///< mean elongation of the Moon from the Sun
    double Om;      ///< mean longitude of the ascending node of the Moon
};

/// From a TT Julian date and a UT1 Julian date.  The Delaunay arguments are
/// ERFA's eraFa*03, which are the IERS 2003/2010 expressions of TN36 eq. (5.43);
/// gamma is eraGmst06 + pi, GMST being ERA plus precession in right ascension as
/// the tables' own caption specifies.
[[nodiscard]] Arguments arguments_at(double tt1, double tt2, double ut1_1, double ut1_2) noexcept;

/// A convenience for the published test cases, which are specified by MJD alone.
/// The choice of scale for that MJD moves the result by well under the tables'
/// own rounding: the terms have periods near a day, so 40 s of scale ambiguity
/// shifts a 25 µas term by under 0.1 µas.
[[nodiscard]] Arguments arguments_at_mjd(double mjd) noexcept;

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
