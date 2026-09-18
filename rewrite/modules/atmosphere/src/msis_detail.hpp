#pragma once
// msis_detail.hpp — the numerical primitives NRLMSISE-00 is built from.
//
// SPEC-atmosphere.md.  This is this tree's own C++, written against the
// hash-pinned NRL reference implementation (plan D2, ATMO-R-002), which is the
// model's only definition (ATMO-R-001, plan §4 rule 6).  The coefficients are
// extracted rather than transcribed; see msis_coefficients.hpp.
//
// TWO DELIBERATE DEPARTURES FROM THE REFERENCE, both specified:
//
//   * DOUBLE PRECISION (ATMO-R-003).  The reference is single throughout, so
//     this is not bit-identical to it and must not claim to be.  What the gap
//     is was measured, not assumed: msis_reference_values.hpp carries it.
//   * THE PRINT-AND-CONTINUE CONDITIONS BECOME REFUSALS (ATMO-R-030).  Three of
//     the reference's six WRITE statements print a diagnostic and then carry on,
//     so a caller receives a number computed after a condition the model itself
//     called an error and learns of it only if standard output was watched.  A
//     Result interface has no such option.  Two of the three are here.
//
// There is no global state: no METERS flag, no TSELEC array, no saved ALAST.
// The reference caches across calls through SAVEd locals; reproducing that
// would reproduce a data race (ATMO-A-015).

#include <odl/atmosphere/indices.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace odl::atmosphere::detail {

/// Which of the reference's print-and-continue conditions fired, if any.
/// `none` is the overwhelmingly common case: these are reachable through
/// internal inconsistency, not through caller input, which is exactly why they
/// must not be writes to standard output (ATMO-R-030).
enum class Fault { none, coincident_spline_node, nonpositive_density_ratio, pressure_no_convergence };

/// Carried by reference through the computation.  The FIRST fault wins and is
/// kept with its operands, because the second is usually a consequence.
struct FaultRecord {
    Fault what = Fault::none;
    double a = 0.0, b = 0.0, c = 0.0;   ///< the condition's own operands

    void raise(Fault f, double x = 0.0, double y = 0.0, double z = 0.0) noexcept {
        if (what == Fault::none) { what = f; a = x; b = y; c = z; }
    }
    [[nodiscard]] bool ok() const noexcept { return what == Fault::none; }
};

/// Second derivatives of a natural-or-clamped cubic spline.
///
/// `yp1`/`ypn` ≥ 1e30 mean "second derivative zero at that end", which is the
/// reference's own signalling convention and is kept because the model's callers
/// use both forms.
template <std::size_t N>
void spline(const std::array<double, N>& x, const std::array<double, N>& y, std::size_t n,
            double yp1, double ypn, std::array<double, N>& y2) noexcept {
    std::array<double, N> u{};
    if (yp1 > 0.99e30) {
        y2[0] = 0.0; u[0] = 0.0;
    } else {
        y2[0] = -0.5;
        u[0] = (3.0 / (x[1] - x[0])) * ((y[1] - y[0]) / (x[1] - x[0]) - yp1);
    }
    for (std::size_t i = 1; i + 1 < n; ++i) {
        const double sig = (x[i] - x[i - 1]) / (x[i + 1] - x[i - 1]);
        const double p = sig * y2[i - 1] + 2.0;
        y2[i] = (sig - 1.0) / p;
        u[i] = (6.0 * ((y[i + 1] - y[i]) / (x[i + 1] - x[i])
                       - (y[i] - y[i - 1]) / (x[i] - x[i - 1]))
                / (x[i + 1] - x[i - 1]) - sig * u[i - 1]) / p;
    }
    double qn, un;
    if (ypn > 0.99e30) {
        qn = 0.0; un = 0.0;
    } else {
        qn = 0.5;
        un = (3.0 / (x[n - 1] - x[n - 2])) * (ypn - (y[n - 1] - y[n - 2]) / (x[n - 1] - x[n - 2]));
    }
    y2[n - 1] = (un - qn * u[n - 2]) / (qn * y2[n - 2] + 1.0);
    for (std::size_t k = n - 1; k-- > 0;) y2[k] = y2[k] * y2[k + 1] + u[k];
}

/// Cubic spline interpolation.
///
/// THE REFERENCE PRINTS 'BAD XA INPUT TO SPLINT' HERE AND DIVIDES BY ZERO
/// ANYWAY (line 1539).  Here it is a refusal (ATMO-F-012): two coincident knots
/// mean the caller's node table is malformed, and the value that would come back
/// is an infinity or a NaN dressed as a density.
template <std::size_t N>
double splint(const std::array<double, N>& xa, const std::array<double, N>& ya,
              const std::array<double, N>& y2a, std::size_t n, double x,
              FaultRecord& fault) noexcept {
    std::size_t klo = 0, khi = n - 1;
    while (khi - klo > 1) {
        const std::size_t k = (khi + klo) / 2;
        if (xa[k] > x) khi = k; else klo = k;
    }
    const double h = xa[khi] - xa[klo];
    if (h == 0.0) { fault.raise(Fault::coincident_spline_node, xa[klo], xa[khi], x); return 0.0; }
    const double a = (xa[khi] - x) / h;
    const double b = (x - xa[klo]) / h;
    return a * ya[klo] + b * ya[khi]
         + ((a * a * a - a) * y2a[klo] + (b * b * b - b) * y2a[khi]) * h * h / 6.0;
}

/// Integral of the spline from xa[0] to x.
template <std::size_t N>
double splini(const std::array<double, N>& xa, const std::array<double, N>& ya,
              const std::array<double, N>& y2a, std::size_t n, double x) noexcept {
    double yi = 0.0;
    std::size_t klo = 0, khi = 1;
    while (x > xa[klo] && khi <= n - 1) {
        double xx = x;
        if (khi < n - 1) xx = std::min(x, xa[khi]);
        const double h = xa[khi] - xa[klo];
        const double a = (xa[khi] - xx) / h;
        const double b = (xx - xa[klo]) / h;
        const double a2 = a * a, b2 = b * b;
        yi += ((1.0 - a2) * ya[klo] / 2.0 + b2 * ya[khi] / 2.0
               + ((-(1.0 + a2 * a2) / 4.0 + a2 / 2.0) * y2a[klo]
                  + (b2 * b2 / 4.0 - b2 / 2.0) * y2a[khi]) * h * h / 6.0) * h;
        ++klo; ++khi;
    }
    return yi;
}

/// Turbopause correction: the root-mean blend of a diffusive and a mixed density.
///
/// THE REFERENCE PRINTS 'DNET LOG ERROR' HERE AND CONTINUES (line 1591), having
/// first patched a zero input to 1.0.  A non-positive density before a logarithm
/// is a refusal here (ATMO-F-012).
inline double dnet(double dd, double dm, double zhm, double xmm, double xm,
                   FaultRecord& fault) noexcept {
    const double a = zhm / (xmm - xm);
    if (!(dm > 0.0 && dd > 0.0)) {
        fault.raise(Fault::nonpositive_density_ratio, dm, dd, xm);
        return 0.0;
    }
    const double ylog = a * std::log(dm / dd);
    if (ylog < -10.0) return dd;
    if (ylog > 10.0)  return dm;
    return dd * std::pow(1.0 + std::exp(ylog), 1.0 / a);
}

/// Chemistry/dissociation correction.  Returns exp(correction), as the reference
/// does — the name says "correction" and the value is a multiplier.
inline double ccor(double alt, double r, double h1, double zh) noexcept {
    const double e = (alt - zh) / h1;
    if (e > 70.0)  return std::exp(0.0);
    if (e < -70.0) return std::exp(r);
    return std::exp(r / (1.0 + std::exp(e)));
}

/// O and O2 chemistry/dissociation correction, two scale heights.
inline double ccor2(double alt, double r, double h1, double zh, double h2) noexcept {
    const double e1 = (alt - zh) / h1;
    const double e2 = (alt - zh) / h2;
    if (e1 > 70.0 || e2 > 70.0)    return std::exp(0.0);
    if (e1 < -70.0 && e2 < -70.0)  return std::exp(r);
    return std::exp(r / (1.0 + 0.5 * (std::exp(e1) + std::exp(e2))));
}

/// Latitude-dependent surface gravity (cm s^-2) and effective Earth radius (km).
///
/// These are the model's OWN constants and deliberately not this tree's: the fit
/// was made with them, so substituting `SPEC-gravity`'s more accurate values
/// would evaluate the coefficients at a geometry they were not fitted to.
struct LatitudeGravity { double gsurf, re; };
inline LatitudeGravity glatf(double lat_deg) noexcept {
    constexpr double kDegToRad = 1.74533e-2;          // as the reference rounds it
    const double c2 = std::cos(2.0 * kDegToRad * lat_deg);
    const double gv = 980.616 * (1.0 - 0.0026373 * c2);
    return {gv, 2.0 * gv / (3.085462e-6 + 2.27e-9 * c2) * 1.0e-5};
}

/// Scale height, km.
inline double scalh(double alt, double xm, double temp, double gsurf, double re) noexcept {
    constexpr double kRgas = 831.4;
    const double g = gsurf / ((1.0 + alt / re) * (1.0 + alt / re));
    return kRgas * temp / (g * xm);
}

/// The reference's ZETA: geopotential-like altitude relative to zll.
inline double zeta(double zz, double zl, double re) noexcept {
    return (zz - zl) * (re + zl) / (re + zz);
}

}  // namespace odl::atmosphere::detail
