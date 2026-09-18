#pragma once
// msis_variation.hpp — G(L), the spherical-harmonic variation functions.
//
// SPEC-atmosphere.md.  This is GLOBE7 (upper thermosphere) and GLOB7S (lower
// atmosphere) ported from the hash-pinned reference.
//
// THE SWITCHES ARE GONE, and that is a simplification rather than a loss.
// TSELEC maps a caller's SV array to SW (main terms) and SWC (cross terms) by
// SW(i) = mod(SV(i), 2) and SWC(i) = 1 where |SV(i)| is 1 or 2.  This interface
// exposes no switches (ATMO-R-009), so SV is all ones except SV(9), which
// selects the geomagnetic convention.  Therefore SW is all +1 except SW(9) =
// ±1, and SWC IS ALL 1.  Every `IF(SW(k).EQ.0) GOTO` in the reference is a
// branch never taken here, and every `ABS(SW(i))` is 1.  Those are exactly the
// two configurations the 17 published cases exercise: cases 1-15 with the
// default, cases 16-17 after TSELEC sets SW(9) = -1.
//
// THE SHARED LEGENDRE STATE IS MADE EXPLICIT.  In the reference GLOB7S reads
// PLG, CTLOC, STLOC, DAY, DFA, APDF and APT out of COMMON/LPOLY/ and never
// writes them: its result depends on GLOBE7 having been called first, with the
// same latitude, and nothing in its signature says so.  Here that state is a
// parameter (`Shared`), so the dependency is visible and cannot be got wrong by
// call order (ATMO-A-015).
//
// ONE MUTATION IN THE REFERENCE IS DELIBERATELY NOT REPRODUCED.  Line 1107 is
// `IF(P(25).LT.1.E-4) P(25)=1.E-4`, which WRITES TO THE COEFFICIENT ARRAY and so
// changes every later call.  IT IS DEAD FOR ALL ELEVEN ARRAYS GLOBE7 IS CALLED
// WITH, measured rather than assumed:
//
//   PT, PS and seven PD columns   8.667840e-02   867x the clamp, 2.938 decades
//   PD column 1                   8.310900e-02   831x            2.920 decades
//   PD column 2                   5.382050e-02   538x            2.731 decades  <- BINDING
//
// The coefficients are const here because of the LAST line, not the first: a
// margin is the worst column's, never the typical one.  SPEC-gravity §3.6a is
// the same rule — the scale's bottom margin is set by the smallest intermediate
// the recursion forms, not by a representative value.

#include "msis_coefficients.hpp"
#include "msis_detail.hpp"

#include <array>
#include <cmath>

namespace odl::atmosphere::detail {

constexpr double kDegToRad = 1.74533e-2;    // as the reference rounds it
constexpr double kDayToRad = 1.72142e-2;    // DR
constexpr double kHourToRad = 0.2618;       // HR
constexpr double kSecToRad = 7.2722e-5;     // SR

/// What COMMON/LPOLY carries between GLOBE7 and GLOB7S, made a parameter.
struct Shared {
    double plg[4][9]{};       ///< plg[j][i] is the reference's PLG(i+1, j+1)
    double ctloc = 0, stloc = 0, c2tloc = 0, s2tloc = 0, c3tloc = 0, s3tloc = 0;
    double day = 0, df = 0, dfa = 0, apd = 0, apdf = 0;
    double apt1 = 0;
    double longitude = 0;
};

/// The associated Legendre polynomials the model uses, exactly the subset the
/// reference fills — the commented-out rows there are not computed here either.
inline void fill_legendre(double lat_deg, Shared& sh) noexcept {
    const double c = std::sin(lat_deg * kDegToRad);
    const double s = std::cos(lat_deg * kDegToRad);
    const double c2 = c * c, c4 = c2 * c2, s2 = s * s;
    auto& g = sh.plg;
    g[0][1] = c;
    g[0][2] = 0.5 * (3.0 * c2 - 1.0);
    g[0][3] = 0.5 * (5.0 * c * c2 - 3.0 * c);
    g[0][4] = (35.0 * c4 - 30.0 * c2 + 3.0) / 8.0;
    g[0][5] = (63.0 * c2 * c2 * c - 70.0 * c2 * c + 15.0 * c) / 8.0;
    g[0][6] = (11.0 * c * g[0][5] - 5.0 * g[0][4]) / 6.0;
    g[1][1] = s;
    g[1][2] = 3.0 * c * s;
    g[1][3] = 1.5 * (5.0 * c2 - 1.0) * s;
    g[1][4] = 2.5 * (7.0 * c2 * c - 3.0 * c) * s;
    g[1][5] = 1.875 * (21.0 * c4 - 14.0 * c2 + 1.0) * s;
    g[1][6] = (11.0 * c * g[1][5] - 6.0 * g[1][4]) / 5.0;
    g[2][2] = 3.0 * s2;
    g[2][3] = 15.0 * s2 * c;
    g[2][4] = 7.5 * (7.0 * c2 - 1.0) * s2;
    g[2][5] = 3.0 * c * g[2][4] - 2.0 * g[2][3];
    g[2][6] = (11.0 * c * g[2][5] - 7.0 * g[2][4]) / 4.0;
    g[2][7] = (13.0 * c * g[2][6] - 8.0 * g[2][5]) / 5.0;
    g[3][3] = 15.0 * s2 * s;
    g[3][4] = 105.0 * s2 * s * c;
    g[3][5] = (9.0 * c * g[3][4] - 7.0 * g[3][3]) / 2.0;
    g[3][6] = (11.0 * c * g[3][5] - 8.0 * g[3][4]) / 3.0;
}

inline void fill_local_time(double tloc_hours, Shared& sh) noexcept {
    sh.stloc  = std::sin(kHourToRad * tloc_hours);
    sh.ctloc  = std::cos(kHourToRad * tloc_hours);
    sh.s2tloc = std::sin(2.0 * kHourToRad * tloc_hours);
    sh.c2tloc = std::cos(2.0 * kHourToRad * tloc_hours);
    sh.s3tloc = std::sin(3.0 * kHourToRad * tloc_hours);
    sh.c3tloc = std::cos(3.0 * kHourToRad * tloc_hours);
}

/// Eq. A24d.  A statement function in the reference; a lambda would hide that
/// it reads P(25) and P(26) from the caller's coefficient array.
inline double g0(double a, const double* p) noexcept {
    const double p25 = std::abs(p[24]);
    return a - 4.0 + (p[25] - 1.0) * (a - 4.0 + (std::exp(-p25 * (a - 4.0)) - 1.0) / p25);
}

/// Eq. A24c.
inline double sumex(double ex) noexcept {
    return 1.0 + (1.0 - std::pow(ex, 19.0)) / (1.0 - ex) * std::sqrt(ex);
}

/// Eq. A24a — the three-hourly magnetic activity sum.
inline double sg0(double ex, const std::array<double, 7>& ap, const double* p) noexcept {
    return (g0(ap[1], p)
            + (g0(ap[2], p) * ex + g0(ap[3], p) * ex * ex + g0(ap[4], p) * std::pow(ex, 3.0)
               + (g0(ap[5], p) * std::pow(ex, 4.0) + g0(ap[6], p) * std::pow(ex, 12.0))
                 * (1.0 - std::pow(ex, 8.0)) / (1.0 - ex)))
           / sumex(ex);
}

/// GLOBE7 — the upper-thermosphere variation function.  `sw9` is +1 for daily Ap
/// and -1 for the seven-element form; every other switch is 1 (see the header).
inline double globe7(double day, double sec, double lat_deg, double long_deg, double tloc,
                     double f107a, double f107, const DailyAp& daily,
                     const ThreeHourlyAp& hourly, double sw9,
                     const double* p, Shared& sh) noexcept {
    double t[15] = {};
    sh.day = day;
    sh.longitude = long_deg;
    fill_legendre(lat_deg, sh);
    fill_local_time(tloc, sh);
    const auto& g = sh.plg;

    const double cd14 = std::cos(kDayToRad * (day - p[13]));
    const double cd18 = std::cos(2.0 * kDayToRad * (day - p[17]));
    const double cd32 = std::cos(kDayToRad * (day - p[31]));
    const double cd39 = std::cos(2.0 * kDayToRad * (day - p[38]));

    // F10.7 effect
    sh.df = f107 - f107a;
    sh.dfa = f107a - 150.0;
    const double df = sh.df, dfa = sh.dfa;
    t[0] = p[19] * df * (1.0 + p[59] * dfa) + p[20] * df * df + p[21] * dfa + p[29] * dfa * dfa;
    const double f1 = 1.0 + (p[47] * dfa + p[19] * df + p[20] * df * df);
    const double f2 = 1.0 + (p[49] * dfa + p[19] * df + p[20] * df * df);

    t[1] = (p[1] * g[0][2] + p[2] * g[0][4] + p[22] * g[0][6])
         + (p[14] * g[0][2]) * dfa + p[26] * g[0][1];
    t[2] = p[18] * cd32;                                   // symmetrical annual
    t[3] = (p[15] + p[16] * g[0][2]) * cd18;               // symmetrical semiannual
    t[4] = f1 * (p[9] * g[0][1] + p[10] * g[0][3]) * cd14; // asymmetrical annual
    t[5] = p[37] * g[0][1] * cd39;                         // asymmetrical semiannual

    // diurnal
    const double t71 = (p[11] * g[1][2]) * cd14;
    const double t72 = (p[12] * g[1][2]) * cd14;
    t[6] = f2 * ((p[3] * g[1][1] + p[4] * g[1][3] + p[27] * g[1][5] + t71) * sh.ctloc
               + (p[6] * g[1][1] + p[7] * g[1][3] + p[28] * g[1][5] + t72) * sh.stloc);

    // semidiurnal
    const double t81 = (p[23] * g[2][3] + p[35] * g[2][5]) * cd14;
    const double t82 = (p[33] * g[2][3] + p[36] * g[2][5]) * cd14;
    t[7] = f2 * ((p[5] * g[2][2] + p[41] * g[2][4] + t81) * sh.c2tloc
               + (p[8] * g[2][2] + p[42] * g[2][4] + t82) * sh.s2tloc);

    // terdiurnal
    t[13] = f2 * ((p[39] * g[3][3] + (p[93] * g[3][4] + p[46] * g[3][6]) * cd14) * sh.s3tloc
                + (p[40] * g[3][3] + (p[94] * g[3][4] + p[48] * g[3][6]) * cd14) * sh.c3tloc);

    // magnetic activity
    if (sw9 != -1.0) {
        sh.apd = daily.ap - 4.0;
        double p44 = p[43];
        const double p45 = p[44];
        if (p44 < 0.0) p44 = 1.0e-5;
        sh.apdf = sh.apd + (p45 - 1.0) * (sh.apd + (std::exp(-p44 * sh.apd) - 1.0) / p44);
        t[8] = sh.apdf * (p[32] + p[45] * g[0][2] + p[34] * g[0][4]
               + (p[100] * g[0][1] + p[101] * g[0][3] + p[102] * g[0][5]) * cd14
               + (p[121] * g[1][1] + p[122] * g[1][3] + p[123] * g[1][5])
                 * std::cos(kHourToRad * (tloc - p[124])));
    } else if (p[51] != 0.0) {
        double exp1 = std::exp(-10800.0 * std::abs(p[51])
                               / (1.0 + p[138] * (45.0 - std::abs(lat_deg))));
        if (exp1 > 0.99999) exp1 = 0.99999;
        sh.apt1 = sg0(exp1, hourly.ap, p);
        t[8] = sh.apt1 * (p[50] + p[96] * g[0][2] + p[54] * g[0][4]
               + (p[125] * g[0][1] + p[126] * g[0][3] + p[127] * g[0][5]) * cd14
               + (p[128] * g[1][1] + p[129] * g[1][3] + p[130] * g[1][5])
                 * std::cos(kHourToRad * (tloc - p[131])));
    }

    if (long_deg > -1000.0) {
        // longitudinal
        t[10] = (1.0 + p[80] * dfa)
              * ((p[64] * g[1][2] + p[65] * g[1][4] + p[66] * g[1][6]
                  + p[103] * g[1][1] + p[104] * g[1][3] + p[105] * g[1][5]
                  + (p[109] * g[1][1] + p[110] * g[1][3] + p[111] * g[1][5]) * cd14)
                 * std::cos(kDegToRad * long_deg)
               + (p[90] * g[1][2] + p[91] * g[1][4] + p[92] * g[1][6]
                  + p[106] * g[1][1] + p[107] * g[1][3] + p[108] * g[1][5]
                  + (p[112] * g[1][1] + p[113] * g[1][3] + p[114] * g[1][5]) * cd14)
                 * std::sin(kDegToRad * long_deg));

        // UT and mixed UT/longitude
        t[11] = (1.0 + p[95] * g[0][1]) * (1.0 + p[81] * dfa) * (1.0 + p[119] * g[0][1] * cd14)
              * ((p[68] * g[0][1] + p[69] * g[0][3] + p[70] * g[0][5])
                 * std::cos(kSecToRad * (sec - p[71])));
        t[11] += (p[76] * g[2][3] + p[77] * g[2][5] + p[78] * g[2][7])
               * std::cos(kSecToRad * (sec - p[79]) + 2.0 * kDegToRad * long_deg)
               * (1.0 + p[137] * dfa);

        // UT/longitude magnetic activity
        if (sw9 != -1.0) {
            t[12] = sh.apdf * (1.0 + p[120] * g[0][1])
                  * ((p[60] * g[1][2] + p[61] * g[1][4] + p[62] * g[1][6])
                     * std::cos(kDegToRad * (long_deg - p[63])))
                  + sh.apdf * (p[115] * g[1][1] + p[116] * g[1][3] + p[117] * g[1][5])
                    * cd14 * std::cos(kDegToRad * (long_deg - p[118]))
                  + sh.apdf * (p[83] * g[0][1] + p[84] * g[0][3] + p[85] * g[0][5])
                    * std::cos(kSecToRad * (sec - p[75]));
        } else if (p[51] != 0.0) {
            t[12] = sh.apt1 * (1.0 + p[132] * g[0][1])
                  * ((p[52] * g[1][2] + p[98] * g[1][4] + p[67] * g[1][6])
                     * std::cos(kDegToRad * (long_deg - p[97])))
                  + sh.apt1 * (p[133] * g[1][1] + p[134] * g[1][3] + p[135] * g[1][5])
                    * cd14 * std::cos(kDegToRad * (long_deg - p[136]))
                  + sh.apt1 * (p[55] * g[0][1] + p[56] * g[0][3] + p[57] * g[0][5])
                    * std::cos(kSecToRad * (sec - p[58]));
        }
    }

    double tinf = p[30];
    for (int i = 0; i < 14; ++i) tinf += t[i];   // |SW(i)| == 1 for every i
    return tinf;
}

/// GLOB7S — the lower-atmosphere variation function.  Reads `sh`, which
/// `globe7` fills; in the reference that dependency is invisible.
inline double glob7s(const double* p, const Shared& sh) noexcept {
    double t[15] = {};
    const auto& g = sh.plg;
    const double day = sh.day, dfa = sh.dfa;
    const double cd32 = std::cos(kDayToRad * (day - p[31]));
    const double cd18 = std::cos(2.0 * kDayToRad * (day - p[17]));
    const double cd14 = std::cos(kDayToRad * (day - p[13]));
    const double cd39 = std::cos(2.0 * kDayToRad * (day - p[38]));

    t[0] = p[21] * dfa;
    t[1] = p[1] * g[0][2] + p[2] * g[0][4] + p[22] * g[0][6]
         + p[26] * g[0][1] + p[14] * g[0][3] + p[59] * g[0][5];
    t[2] = (p[18] + p[47] * g[0][2] + p[29] * g[0][4]) * cd32;
    t[3] = (p[15] + p[16] * g[0][2] + p[30] * g[0][4]) * cd18;
    t[4] = (p[9] * g[0][1] + p[10] * g[0][3] + p[20] * g[0][5]) * cd14;
    t[5] = p[37] * g[0][1] * cd39;

    const double t71 = p[11] * g[1][2] * cd14;
    const double t72 = p[12] * g[1][2] * cd14;
    t[6] = (p[3] * g[1][1] + p[4] * g[1][3] + t71) * sh.ctloc
         + (p[6] * g[1][1] + p[7] * g[1][3] + t72) * sh.stloc;

    const double t81 = (p[23] * g[2][3] + p[35] * g[2][5]) * cd14;
    const double t82 = (p[33] * g[2][3] + p[36] * g[2][5]) * cd14;
    t[7] = (p[5] * g[2][2] + p[41] * g[2][4] + t81) * sh.c2tloc
         + (p[8] * g[2][2] + p[42] * g[2][4] + t82) * sh.s2tloc;

    t[13] = p[39] * g[3][3] * sh.s3tloc + p[40] * g[3][3] * sh.c3tloc;

    // magnetic activity: SW(9) is +1 or -1 here, never 0
    if (sh.apt1 != 0.0) t[8] = p[50] * sh.apt1 + p[96] * g[0][2] * sh.apt1;
    else                t[8] = sh.apdf * (p[32] + p[45] * g[0][2]);

    if (sh.longitude > -1000.0) {
        t[10] = (1.0 + g[0][1] * (p[80] * std::cos(kDayToRad * (day - p[81]))
                                + p[85] * std::cos(2.0 * kDayToRad * (day - p[86])))
                 + p[83] * std::cos(kDayToRad * (day - p[84]))
                 + p[87] * std::cos(2.0 * kDayToRad * (day - p[88])))
              * ((p[64] * g[1][2] + p[65] * g[1][4] + p[66] * g[1][6]
                  + p[74] * g[1][1] + p[75] * g[1][3] + p[76] * g[1][5])
                 * std::cos(kDegToRad * sh.longitude)
               + (p[90] * g[1][2] + p[91] * g[1][4] + p[92] * g[1][6]
                  + p[77] * g[1][1] + p[78] * g[1][3] + p[79] * g[1][5])
                 * std::sin(kDegToRad * sh.longitude));
    }

    double tt = 0.0;
    for (int i = 0; i < 14; ++i) tt += t[i];
    return tt;
}

}  // namespace odl::atmosphere::detail
