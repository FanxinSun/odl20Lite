#pragma once
// msis_profile.hpp — DENSU and DENSM: temperature and density profiles.
//
// SPEC-atmosphere.md.  Ported from the hash-pinned reference.
//
// DENSU HAS A SIDE EFFECT THE CALLER DEPENDS ON, and it is easy to lose in a
// port.  At lines 1333-1334 it writes `TGN1(1)=DTA` and `TN1(1)=TA` back into
// the caller's node arrays — the Bates temperature and its gradient at the
// joining altitude.  GTS7 then reuses those same arrays for later species, so
// the FIRST call's write is read by every later one.  Here the arrays are
// explicit in/out parameters rather than shared COMMON, so the dependency is
// visible; the values written are identical.
//
// `EXPL` MEANS TWO DIFFERENT THINGS under one name in the reference, and both
// clamps are reproduced as written: above the joining altitude it holds the
// exponential itself and is clamped to 50 when it EXCEEDS 50; below, it holds an
// exponent and is clamped before being negated and exponentiated.

#include "msis_detail.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace odl::atmosphere::detail {

constexpr double kRgas = 831.4;

/// Temperature and density above the mesosphere.  `tz` and the node arrays are
/// outputs as well as inputs; see the header.
template <std::size_t N1>
double densu(double alt, double dlb, double tinf, double tlb, double xm, double alpha,
             double& tz, double zlb, double s2, std::size_t mn1,
             const std::array<double, N1>& zn1, std::array<double, N1>& tn1,
             std::array<double, 2>& tgn1, double gsurf, double re,
             FaultRecord& fault) noexcept {
    double result = 1.0;
    const double za = zn1[0];                 // joining altitude of Bates and spline
    double z = std::max(alt, za);
    const double zg2 = zeta(z, zlb, re);
    const double tt = tinf - (tinf - tlb) * std::exp(-s2 * zg2);
    const double ta = tt;
    tz = tt;
    result = tz;

    std::array<double, N1> xs{}, ys{}, y2out{};
    double x = 0.0, zgdif = 0.0, t1 = 0.0;
    const bool below = alt < za;

    if (below) {
        // temperature gradient at za from the Bates profile — WRITTEN BACK
        const double dta = (tinf - ta) * s2 * ((re + zlb) / (re + za)) * ((re + zlb) / (re + za));
        tgn1[0] = dta;
        tn1[0] = ta;
        z = std::max(alt, zn1[mn1 - 1]);
        const double z1 = zn1[0], z2 = zn1[mn1 - 1];
        t1 = tn1[0];
        const double t2 = tn1[mn1 - 1];
        const double zg = zeta(z, z1, re);
        zgdif = zeta(z2, z1, re);
        for (std::size_t k = 0; k < mn1; ++k) {
            xs[k] = zeta(zn1[k], z1, re) / zgdif;
            ys[k] = 1.0 / tn1[k];
        }
        const double yd1 = -tgn1[0] / (t1 * t1) * zgdif;
        const double yd2 = -tgn1[1] / (t2 * t2) * zgdif * ((re + z2) / (re + z1)) * ((re + z2) / (re + z1));
        spline(xs, ys, mn1, yd1, yd2, y2out);
        x = zg / zgdif;
        const double y = splint(xs, ys, y2out, mn1, x, fault);
        tz = 1.0 / y;
        result = tz;
    }

    if (xm == 0.0) return result;

    // density above za
    double glb = gsurf / ((1.0 + zlb / re) * (1.0 + zlb / re));
    const double gamma = xm * glb / (s2 * kRgas * tinf);
    double expl = std::exp(-s2 * gamma * zg2);
    if (expl > 50.0 || tt <= 0.0) expl = 50.0;      // clamps the VALUE, as written
    result = dlb * std::pow(tlb / tt, 1.0 + alpha + gamma) * expl;
    if (!below) return result;

    // density below za
    const double z1 = zn1[0];
    glb = gsurf / ((1.0 + z1 / re) * (1.0 + z1 / re));
    const double gamm = xm * glb * zgdif / kRgas;
    const double yi = splini(xs, ys, y2out, mn1, x);
    double expl2 = gamm * yi;
    if (expl2 > 50.0 || tz <= 0.0) expl2 = 50.0;    // clamps the EXPONENT
    return result * std::pow(t1 / tz, 1.0 + alpha) * std::exp(-expl2);
}

/// Temperature and density in the stratosphere, mesosphere and troposphere.
template <std::size_t N3, std::size_t N2>
double densm(double alt, double d0, double xm, double& tz,
             std::size_t mn3, const std::array<double, N3>& zn3,
             const std::array<double, N3>& tn3, const std::array<double, 2>& tgn3,
             std::size_t mn2, const std::array<double, N2>& zn2,
             const std::array<double, N2>& tn2, const std::array<double, 2>& tgn2,
             double gsurf, double re, FaultRecord& fault) noexcept {
    double result = d0;
    std::array<double, 10> xs{}, ys{}, y2out{};

    auto layer = [&](std::size_t mn, const double* zn, const double* tn, const double* tgn,
                     double z) {
        const double z1 = zn[0], z2 = zn[mn - 1];
        const double t1 = tn[0], t2 = tn[mn - 1];
        const double zg = zeta(z, z1, re);
        const double zgdif = zeta(z2, z1, re);
        for (std::size_t k = 0; k < mn; ++k) {
            xs[k] = zeta(zn[k], z1, re) / zgdif;
            ys[k] = 1.0 / tn[k];
        }
        const double yd1 = -tgn[0] / (t1 * t1) * zgdif;
        const double yd2 = -tgn[1] / (t2 * t2) * zgdif * ((re + z2) / (re + z1)) * ((re + z2) / (re + z1));
        spline(xs, ys, mn, yd1, yd2, y2out);
        const double x = zg / zgdif;
        tz = 1.0 / splint(xs, ys, y2out, mn, x, fault);
        if (xm == 0.0) return;
        const double glb = gsurf / ((1.0 + z1 / re) * (1.0 + z1 / re));
        const double gamm = xm * glb * zgdif / kRgas;
        double expl = gamm * splini(xs, ys, y2out, mn, x);
        if (expl > 50.0) expl = 50.0;
        result = result * (t1 / tz) * std::exp(-expl);
    };

    if (alt <= zn2[0]) {
        layer(mn2, zn2.data(), tn2.data(), tgn2.data(), std::max(alt, zn2[mn2 - 1]));
        if (alt <= zn3[0]) layer(mn3, zn3.data(), tn3.data(), tgn3.data(), alt);
    }
    return (xm == 0.0) ? tz : result;
}

}  // namespace odl::atmosphere::detail
