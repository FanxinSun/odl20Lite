#pragma once
// odl/eop/fundamental_arguments.hpp — the six arguments every tidal table in
// this tree is written in, defined ONCE.
//
// Promoted from this module's private header when L2 step 3 needed them.  The
// alternative was for `tides` to compute the same six from ERFA itself, and the
// manager's condition on GRAV-R-029 settled that shape of question already:
// *two definitions is how one ends up secular and the other mean.*  Tables 5.1,
// 8.2, 8.3 and 6.5a/b/c are all indexed by these, so there is one of them.
//
// SPEC-perturbations PERT-R-015 says they come from `frames`, which owns GMST
// and the nutation arguments.  They are here instead because that is where the
// tree already had them and where they are already tested; moving them again
// would be a refactor with no test behind it.  The principle the requirement
// was protecting — one definition, consumed rather than restated — holds either
// way, and the discrepancy is recorded rather than quietly resolved.

#include <odl/time/epoch.hpp>

namespace odl::eop::tides {

/// The six arguments of TN36 Tables 5.1, 8.2/8.3 and 6.5a/b/c, in radians.
struct Arguments {
    double gamma;   ///< GMST + pi
    double l;       ///< Delaunay: mean anomaly of the Moon
    double lp;      ///< mean anomaly of the Sun
    double F;       ///< L - Omega
    double D;       ///< mean elongation of the Moon from the Sun
    double Om;      ///< mean longitude of the ascending node of the Moon
};

/// From a TT Julian date and a UT1 Julian date, each as a two-part sum.  The
/// Delaunay arguments are ERFA's eraFa*03, the IERS 2003/2010 expressions of
/// TN36 (5.43); gamma is eraGmst06 + pi, GMST being ERA plus precession in
/// right ascension as the tables' own captions specify.
[[nodiscard]] Arguments arguments_at(double tt1, double tt2, double ut1_1, double ut1_2) noexcept;

/// A convenience for the published test cases, which are specified by MJD alone.
[[nodiscard]] Arguments arguments_at_mjd(double mjd) noexcept;

}  // namespace odl::eop::tides
