#pragma once
// odl/gravity/legendre.hpp — the recursion of SPEC-gravity §4.4, in one place.
//
// PUBLIC BECAUSE GRAV-A-003 CHECKS IT.  That test compares this recursion, in
// double precision, against the definition evaluated in high-precision decimal
// arithmetic.  A test that reimplemented the recursion would be checking its own
// copy against the definition and would say nothing about the copy the field
// actually uses, so there is exactly one copy and both callers use it.
//
// THE RECURSION, derived from TN36-6 (6.2b) with DLMF 14.6.1 (its Condon-
// Shortley phase removed, GRAV-R-007) and DLMF 14.10.3, and verified against the
// definition in exact rational arithmetic to 60 significant digits before it was
// written into the specification.  With u = sin(phi) and P'_nm = Pbar_nm/cos^m(phi):
//
//     P'_00     = 1
//     P'_11     = sqrt(3)                        <- NOT the general sectorial step
//     P'_mm     = sqrt((2m+1)/(2m)) P'_{m-1,m-1}    m >= 2
//     P'_{m+1,m}= sqrt(2m+3) u P'_mm
//     P'_nm     = a_nm u P'_{n-1,m} - b_nm P'_{n-2,m}
//
// and, differentiating with respect to u (the sectorial function does not depend
// on u at all, which is what makes the derivative recursion start cleanly):
//
//     dP'_mm    = 0
//     dP'_{m+1,m} = sqrt(2m+3) P'_mm
//     dP'_nm    = a_nm (P'_{n-1,m} + u dP'_{n-1,m}) - b_nm dP'_{n-2,m}
//
// SCALE.  P'_nm is not representable at high degree: max over m of P'_{2190,m}(1)
// is 10^457.9, at m = 979.  Callers pass a scale; the synthesis passes 10^-280
// and unscales at the end, and GRAV-A-003 passes 1 because the values it checks
// are chosen to be representable unscaled.

#include <odl/gravity/field.hpp>

#include <cmath>
#include <cstddef>

namespace odl::gravity {

/// Fill P[m..n_max] and dP[m..n_max] for one order.  Entries below m are not
/// touched.  `scale` multiplies every value; the recursion is linear so scaling
/// the seed scales the column.
inline void legendre_column(const RecursionTable& t, int m, int n_max, double u, double scale,
                            double* P, double* dP) noexcept {
    const auto um = static_cast<std::size_t>(m);
    P[um] = t.sectorial(m) * scale;
    dP[um] = 0.0;
    if (m + 1 <= n_max) {
        const double f1 = std::sqrt(2.0 * static_cast<double>(m) + 3.0);
        P[um + 1] = f1 * u * P[um];
        dP[um + 1] = f1 * P[um];
    }
    for (int n = m + 2; n <= n_max; ++n) {
        const auto un = static_cast<std::size_t>(n);
        const double a = t.a(n, m);
        const double b = t.b(n, m);
        P[un] = a * u * P[un - 1] - b * P[un - 2];
        dP[un] = a * (P[un - 1] + u * dP[un - 1]) - b * dP[un - 2];
    }
}

}  // namespace odl::gravity
