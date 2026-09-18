#pragma once
// msis_thermosphere.hpp — GTS7, the thermospheric model.
//
// SPEC-atmosphere.md.  Ported from the hash-pinned reference.
//
// ONLY THE ALL-SPECIES PATH EXISTS HERE.  The reference selects among eleven
// mass numbers through a computed GOTO; this interface returns every species
// (ATMO-R-005), so `MASS = 48` is the only path and every `IF(MASS.NE.48) GO TO
// 90` is a fall-through.  Selector 49 is refused (ATMO-R-012, ATMO-F-010) and
// the single-species selectors are not offered, so `DD` — which exists only to
// accumulate for 49 — is not carried.
//
// EVERY SWITCH IS 1 (see msis_variation.hpp), so `SW(15)`, `SW(21)`, `SW(16)`
// through `SW(20)`, `SW(5)` and `SW(1)` guards are all taken.
//
// TWO NAMES THAT LOOK ALIKE AND ARE NOT.  In the anomalous-oxygen block the
// reference passes `T2` to DENSU, not `T(2)`: an undeclared scalar, distinct
// from the output temperature array.  A port that reads it as `T(2)` would
// overwrite the temperature at altitude with the anomalous-oxygen profile's.
// And the mixed-density calls pass `T(2)` for most species but a scratch `TZ`
// for N2 — reproduced call for call, because the LAST write wins.
//
// Densities here are the reference's CGS (cm^-3, g cm^-3).  The SI crossing is
// made once, at the module boundary (ATMO-R-006).

#include "msis_coefficients.hpp"
#include "msis_detail.hpp"
#include "msis_profile.hpp"
#include "msis_variation.hpp"

#include <array>
#include <cmath>

namespace odl::atmosphere::detail {

/// The reference's MESO7 temperature nodes, which DENSU writes into.
struct MesoNodes {
    std::array<double, 5> zn1{120.0, 110.0, 100.0, 90.0, 72.5};
    std::array<double, 4> zn2{72.5, 55.0, 45.0, 32.5};
    std::array<double, 5> zn3{32.5, 20.0, 15.0, 10.0, 0.0};
    std::array<double, 5> tn1{};
    std::array<double, 4> tn2{};
    std::array<double, 5> tn3{};
    std::array<double, 2> tgn1{}, tgn2{}, tgn3{};
};

struct ThermoState {
    double tlb = 0, s = 0, za = 0;
    double db04 = 0, db16 = 0, db28 = 0, db32 = 0, db40 = 0, db01 = 0, db14 = 0;
    double xmm = 0;
    /// The N2 MIXED density at altitude, which GTD7 needs for the lower
    /// atmosphere.  The reference passes it through COMMON/DMIX, and it must be
    /// captured HERE rather than recomputed afterwards: DENSU writes back into
    /// the temperature-node arrays (see msis_profile.hpp), so by the time the
    /// later species are done, tn1 and tgn1 are not what this call saw.
    double dm28 = 0;
};

/// GTS7 with MASS = 48.  `d` is the reference's D(1..9) in CGS; `t` is T(1..2).
inline void gts7(double yrd, double sec, double alt, double glat, double glong, double stl,
                 double f107a, double f107, const DailyAp& daily, const ThreeHourlyAp& hourly,
                 double sw9, double gsurf, double re,
                 std::array<double, 9>& d, std::array<double, 2>& t,
                 MesoNodes& m, ThermoState& st, Shared& sh, FaultRecord& fault) noexcept {
    using namespace coeff;
    constexpr std::array<double, 9> alpha{-0.38, 0.0, 0.0, 0.0, 0.17, 0.0, -0.38, 0.0, 0.0};
    constexpr std::array<double, 8> altl{200.0, 300.0, 160.0, 250.0, 240.0, 450.0, 320.0, 450.0};
    constexpr std::size_t mn1 = 5;

    // NOT-A-UNIT-CROSSING: extracts DDD from the reference's packed YYDDD date.
    const double day = std::fmod(yrd, 1000.0);   // NOT-A-UNIT-CROSSING: YYDDD radix
    st.za = kPDL[1][15];            // PDL(16,2): the joining altitude is a COEFFICIENT, not 120
    m.zn1[0] = st.za;
    d.fill(0.0);

    auto g7 = [&](const double* p) {
        return globe7(day, sec, glat, glong, stl, f107a, f107, daily, hourly, sw9, p, sh);
    };

    // exospheric temperature
    double tinf = (alt > m.zn1[0]) ? kPTM[0] * kPT[0] * (1.0 + g7(kPT.data()))
                                   : kPTM[0] * kPT[0];
    t[0] = tinf;
    // temperature gradient at the lowest node
    const double g0v = (alt > m.zn1[4]) ? kPTM[3] * kPS[0] * (1.0 + g7(kPS.data()))
                                        : kPTM[3] * kPS[0];
    st.tlb = kPTM[1] * (1.0 + g7(kPD[3].data())) * kPD[3][0];
    st.s = g0v / (tinf - st.tlb);

    if (alt < 300.0) {
        m.tn1[1] = kPTM[6] * kPTL[0][0] / (1.0 - glob7s(kPTL[0].data(), sh));
        m.tn1[2] = kPTM[2] * kPTL[1][0] / (1.0 - glob7s(kPTL[1].data(), sh));
        m.tn1[3] = kPTM[7] * kPTL[2][0] / (1.0 - glob7s(kPTL[2].data(), sh));
        m.tn1[4] = kPTM[4] * kPTL[3][0] / (1.0 - glob7s(kPTL[3].data(), sh));
        m.tgn1[1] = kPTM[8] * kPMA[8][0] * (1.0 + glob7s(kPMA[8].data(), sh))
                  * m.tn1[4] * m.tn1[4] / ((kPTM[4] * kPTL[3][0]) * (kPTM[4] * kPTL[3][0]));
    } else {
        m.tn1[1] = kPTM[6] * kPTL[0][0];
        m.tn1[2] = kPTM[2] * kPTL[1][0];
        m.tn1[3] = kPTM[7] * kPTL[2][0];
        m.tn1[4] = kPTM[4] * kPTL[3][0];
        m.tgn1[1] = kPTM[8] * kPMA[8][0]
                  * m.tn1[4] * m.tn1[4] / ((kPTM[4] * kPTL[3][0]) * (kPTM[4] * kPTL[3][0]));
    }

    const double g28 = g7(kPD[2].data());
    const double zhf = kPDL[1][24] * (1.0 + kPDL[0][24] * std::sin(kDegToRad * glat)
                                            * std::cos(kDayToRad * (day - kPT[13])));
    t[0] = tinf;
    st.xmm = kPDM[2][4];            // PDM(5,3)
    const double z = alt;
    double scratch = 0.0;

    auto du = [&](double zz, double dlb, double xm, double al, double& tz) {
        return densu(zz, dlb, tinf, st.tlb, xm, al, tz, kPTM[5], st.s, mn1,
                     m.zn1, m.tn1, m.tgn1, gsurf, re, fault);
    };

    // ---- N2 ------------------------------------------------------------
    st.db28 = kPDM[2][0] * std::exp(g28) * kPD[2][0];
    d[2] = du(z, st.db28, 28.0, alpha[2], t[1]);
    const double zh28 = kPDM[2][2] * zhf;
    const double zhm28 = kPDM[2][3] * kPDL[1][5];
    const double b28 = du(zh28, st.db28, 28.0 - st.xmm, alpha[2] - 1.0, scratch);
    if (z <= altl[2]) {
        st.dm28 = du(z, b28, st.xmm, alpha[2], scratch);
        d[2] = dnet(d[2], st.dm28, zhm28, st.xmm, 28.0, fault);
    }

    // ---- He ------------------------------------------------------------
    st.db04 = kPDM[0][0] * std::exp(g7(kPD[0].data())) * kPD[0][0];
    d[0] = du(z, st.db04, 4.0, alpha[0], t[1]);
    if (z <= altl[0]) {
        const double b04 = du(kPDM[0][2], st.db04, 4.0 - st.xmm, alpha[0] - 1.0, t[1]);
        const double dm04 = du(z, b04, st.xmm, 0.0, t[1]);
        d[0] = dnet(d[0], dm04, zhm28, st.xmm, 4.0, fault);
        const double rl = std::log(b28 * kPDM[0][1] / b04);
        d[0] *= ccor(z, rl, kPDM[0][5] * kPDL[1][1], kPDM[0][4] * kPDL[1][0]);
    }

    // ---- O -------------------------------------------------------------
    st.db16 = kPDM[1][0] * std::exp(g7(kPD[1].data())) * kPD[1][0];
    d[1] = du(z, st.db16, 16.0, alpha[1], t[1]);
    if (z <= altl[1]) {
        const double b16 = du(kPDM[1][2], st.db16, 16.0 - st.xmm, alpha[1] - 1.0, t[1]);
        const double dm16 = du(z, b16, st.xmm, 0.0, t[1]);
        d[1] = dnet(d[1], dm16, zhm28, st.xmm, 16.0, fault);
        const double rl = kPDM[1][1] * kPDL[1][16] * (1.0 + kPDL[0][23] * (f107a - 150.0));
        d[1] *= ccor2(z, rl, kPDM[1][5] * kPDL[1][3], kPDM[1][4] * kPDL[1][2],
                      kPDM[1][5] * kPDL[1][4]);
        d[1] *= ccor(z, kPDM[1][3] * kPDL[1][14], kPDM[1][7] * kPDL[1][13],
                     kPDM[1][6] * kPDL[1][12]);
    }

    // ---- O2 ------------------------------------------------------------
    st.db32 = kPDM[3][0] * std::exp(g7(kPD[4].data())) * kPD[4][0];
    d[3] = du(z, st.db32, 32.0, alpha[3], t[1]);
    if (z <= altl[3]) {
        const double b32 = du(kPDM[3][2], st.db32, 32.0 - st.xmm, alpha[3] - 1.0, t[1]);
        const double dm32 = du(z, b32, st.xmm, 0.0, t[1]);
        d[3] = dnet(d[3], dm32, zhm28, st.xmm, 32.0, fault);
        const double rl = std::log(b28 * kPDM[3][1] / b32);
        d[3] *= ccor(z, rl, kPDM[3][5] * kPDL[1][7], kPDM[3][4] * kPDL[1][6]);
    }
    {
        const double rc32 = kPDM[3][3] * kPDL[1][23] * (1.0 + kPDL[0][23] * (f107a - 150.0));
        d[3] *= ccor2(z, rc32, kPDM[3][7] * kPDL[1][22], kPDM[3][6] * kPDL[1][21],
                      kPDM[3][7] * kPDL[0][22]);
    }

    // ---- Ar ------------------------------------------------------------
    st.db40 = kPDM[4][0] * std::exp(g7(kPD[5].data())) * kPD[5][0];
    d[4] = du(z, st.db40, 40.0, alpha[4], t[1]);
    if (z <= altl[4]) {
        const double b40 = du(kPDM[4][2], st.db40, 40.0 - st.xmm, alpha[4] - 1.0, t[1]);
        const double dm40 = du(z, b40, st.xmm, 0.0, t[1]);
        d[4] = dnet(d[4], dm40, zhm28, st.xmm, 40.0, fault);
        const double rl = std::log(b28 * kPDM[4][1] / b40);
        d[4] *= ccor(z, rl, kPDM[4][5] * kPDL[1][9], kPDM[4][4] * kPDL[1][8]);
    }

    // ---- H -------------------------------------------------------------
    st.db01 = kPDM[5][0] * std::exp(g7(kPD[6].data())) * kPD[6][0];
    d[6] = du(z, st.db01, 1.0, alpha[6], t[1]);
    if (z <= altl[6]) {
        const double b01 = du(kPDM[5][2], st.db01, 1.0 - st.xmm, alpha[6] - 1.0, t[1]);
        const double dm01 = du(z, b01, st.xmm, 0.0, t[1]);
        d[6] = dnet(d[6], dm01, zhm28, st.xmm, 1.0, fault);
        const double rl = std::log(b28 * kPDM[5][1] * std::abs(kPDL[1][17]) / b01);
        d[6] *= ccor(z, rl, kPDM[5][5] * kPDL[1][11], kPDM[5][4] * kPDL[1][10]);
        d[6] *= ccor(z, kPDM[5][3] * kPDL[1][20], kPDM[5][7] * kPDL[1][19],
                     kPDM[5][6] * kPDL[1][18]);
    }

    // ---- N -------------------------------------------------------------
    st.db14 = kPDM[6][0] * std::exp(g7(kPD[7].data())) * kPD[7][0];
    d[7] = du(z, st.db14, 14.0, alpha[7], t[1]);
    if (z <= altl[7]) {
        const double b14 = du(kPDM[6][2], st.db14, 14.0 - st.xmm, alpha[7] - 1.0, t[1]);
        const double dm14 = du(z, b14, st.xmm, 0.0, t[1]);
        d[7] = dnet(d[7], dm14, zhm28, st.xmm, 14.0, fault);
        const double rl = std::log(b28 * kPDM[6][1] * std::abs(kPDL[0][2]) / b14);
        d[7] *= ccor(z, rl, kPDM[6][5] * kPDL[0][1], kPDM[6][4] * kPDL[0][0]);
        d[7] *= ccor(z, kPDM[6][3] * kPDL[0][5], kPDM[6][7] * kPDL[0][4],
                     kPDM[6][6] * kPDL[0][3]);
    }

    // ---- anomalous oxygen ----------------------------------------------
    {
        const double db16h = kPDM[7][0] * std::exp(g7(kPD[8].data())) * kPD[8][0];
        const double tho = kPDM[7][9] * kPDL[0][6];
        double t2_scratch = 0.0;      // the reference's T2, NOT T(2)
        const double dd = densu(z, db16h, tho, tho, 16.0, alpha[8], t2_scratch, kPTM[5],
                                st.s, mn1, m.zn1, m.tn1, m.tgn1, gsurf, re, fault);
        const double zsht = kPDM[7][5];
        const double zmho = kPDM[7][4];
        const double zsho = scalh(zmho, 16.0, tho, gsurf, re);
        d[8] = dd * std::exp(-zsht / zsho * (std::exp(-(z - zmho) / zsht) - 1.0));
    }

    // ---- total mass density (GTD7's sense: anomalous oxygen EXCLUDED) ---
    d[5] = 1.66e-24 * (4.0 * d[0] + 16.0 * d[1] + 28.0 * d[2] + 32.0 * d[3]
                     + 40.0 * d[4] + d[6] + 14.0 * d[7]);
}

}  // namespace odl::atmosphere::detail
