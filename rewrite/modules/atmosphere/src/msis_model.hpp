#pragma once
// msis_model.hpp — GTD7 and GTD7D: the model's two entry points.
//
// SPEC-atmosphere.md.  GTD7 joins the thermospheric model (GTS7) to the lower
// atmosphere below 72.5 km, blending across the mixing height at 62.5 km.
//
// THESE TAKE THE REFERENCE'S OWN ARGUMENTS, deliberately.  The 17 published
// input cases are specified in exactly these terms (day-of-year, seconds of UT,
// altitude, geodetic latitude and longitude, local solar time, F10.7A, F10.7,
// Ap), so the gate compares at this level and the epoch handling above it cannot
// silently change what is being compared.  The public interface in
// odl/atmosphere/atmosphere.hpp is a layer on top.
//
// ONE OPTIMISATION IN THE REFERENCE IS NOT REPRODUCED, and it is safe.  Below
// the mixing height the reference sets MSS = 28 and GTS7 then computes N2 only,
// leaving He, O2 and Ar zero.  Those zeros never reach a result: the blend
// factor DMC is zero at and below the mixing height, and DMC multiplies the only
// term the skipped species enter.  Computing them anyway gives the same answer
// and removes a branch whose correctness depends on that cancellation.

#include "msis_thermosphere.hpp"

namespace odl::atmosphere::detail {

/// Densities in the reference's CGS: cm^-3, and d[5] in g cm^-3.
struct RawResult {
    std::array<double, 9> d{};
    std::array<double, 2> t{};
    FaultRecord fault{};
};

constexpr double kZmix = 62.5;

/// GTD7.  `d[5]` EXCLUDES anomalous oxygen.
inline RawResult gtd7(double iyd, double sec, double alt, double glat, double glong, double stl,
                      double f107a, double f107, const DailyAp& daily,
                      const ThreeHourlyAp& hourly, bool use_three_hourly) noexcept {
    using namespace coeff;
    RawResult out;
    MesoNodes m;
    ThermoState st;
    Shared sh;
    const double sw9 = use_three_hourly ? -1.0 : 1.0;

    const auto lg = glatf(glat);
    const double gsurf = lg.gsurf, re = lg.re;
    const double xmm = kPDM[2][4];
    const double altt = std::max(alt, m.zn2[0]);

    std::array<double, 9> ds{};
    std::array<double, 2> ts{};
    gts7(iyd, sec, altt, glat, glong, stl, f107a, f107, daily, hourly, sw9,
         gsurf, re, ds, ts, m, st, sh, out.fault);

    // st.dm28 was captured inside gts7 where the reference sets COMMON/DMIX.
    const double dm28m = st.dm28;

    out.t = ts;
    if (alt >= m.zn2[0]) { out.d = ds; return out; }

    // lower-atmosphere temperature nodes
    m.tgn2[0] = m.tgn1[1];
    m.tn2[0] = m.tn1[4];
    m.tn2[1] = kPMA[0][0] * kPAVGM[0] / (1.0 - glob7s(kPMA[0].data(), sh));
    m.tn2[2] = kPMA[1][0] * kPAVGM[1] / (1.0 - glob7s(kPMA[1].data(), sh));
    m.tn2[3] = kPMA[2][0] * kPAVGM[2] / (1.0 - glob7s(kPMA[2].data(), sh));
    m.tgn2[1] = kPAVGM[8] * kPMA[9][0] * (1.0 + glob7s(kPMA[9].data(), sh))
              * m.tn2[3] * m.tn2[3] / ((kPMA[2][0] * kPAVGM[2]) * (kPMA[2][0] * kPAVGM[2]));
    m.tn3[0] = m.tn2[3];

    if (alt < m.zn3[0]) {
        m.tgn3[0] = m.tgn2[1];
        m.tn3[1] = kPMA[3][0] * kPAVGM[3] / (1.0 - glob7s(kPMA[3].data(), sh));
        m.tn3[2] = kPMA[4][0] * kPAVGM[4] / (1.0 - glob7s(kPMA[4].data(), sh));
        m.tn3[3] = kPMA[5][0] * kPAVGM[5] / (1.0 - glob7s(kPMA[5].data(), sh));
        m.tn3[4] = kPMA[6][0] * kPAVGM[6] / (1.0 - glob7s(kPMA[6].data(), sh));
        m.tgn3[1] = kPMA[7][0] * kPAVGM[7] * (1.0 + glob7s(kPMA[7].data(), sh))
                  * m.tn3[4] * m.tn3[4] / ((kPMA[6][0] * kPAVGM[6]) * (kPMA[6][0] * kPAVGM[6]));
    }

    double tz = 0.0;
    const double dmc = (alt > kZmix) ? 1.0 - (m.zn2[0] - alt) / (m.zn2[0] - kZmix) : 0.0;
    const double dz28 = ds[2];

    double dmr = ds[2] / dm28m - 1.0;
    out.d[2] = densm(alt, dm28m, xmm, tz, 5, m.zn3, m.tn3, m.tgn3,
                     4, m.zn2, m.tn2, m.tgn2, gsurf, re, out.fault) * (1.0 + dmr * dmc);

    dmr = ds[0] / (dz28 * kPDM[0][1]) - 1.0;
    out.d[0] = out.d[2] * kPDM[0][1] * (1.0 + dmr * dmc);
    out.d[1] = 0.0;                                  // O, H, N and anomalous O are
    out.d[8] = 0.0;                                  // zero below 72.5 km (ATMO-R-007)
    dmr = ds[3] / (dz28 * kPDM[3][1]) - 1.0;
    out.d[3] = out.d[2] * kPDM[3][1] * (1.0 + dmr * dmc);
    dmr = ds[4] / (dz28 * kPDM[4][1]) - 1.0;
    out.d[4] = out.d[2] * kPDM[4][1] * (1.0 + dmr * dmc);
    out.d[6] = 0.0;
    out.d[7] = 0.0;
    out.d[5] = 1.66e-24 * (4.0 * out.d[0] + 16.0 * out.d[1] + 28.0 * out.d[2]
                         + 32.0 * out.d[3] + 40.0 * out.d[4] + out.d[6] + 14.0 * out.d[7]);
    out.t[1] = tz;
    return out;
}

/// GTD7D.  `d[5]` INCLUDES anomalous oxygen — the effective density for drag.
inline RawResult gtd7d(double iyd, double sec, double alt, double glat, double glong, double stl,
                       double f107a, double f107, const DailyAp& daily,
                       const ThreeHourlyAp& hourly, bool use_three_hourly) noexcept {
    RawResult r = gtd7(iyd, sec, alt, glat, glong, stl, f107a, f107, daily, hourly,
                       use_three_hourly);
    r.d[5] = 1.66e-24 * (4.0 * r.d[0] + 16.0 * r.d[1] + 28.0 * r.d[2] + 32.0 * r.d[3]
                       + 40.0 * r.d[4] + r.d[6] + 14.0 * r.d[7] + 16.0 * r.d[8]);
    return r;
}

}  // namespace odl::atmosphere::detail
