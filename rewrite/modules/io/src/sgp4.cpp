#include <odl/io/sgp4.hpp>

#include <odl/io/sp3_ephemeris.hpp>  // calendar_elapsed_seconds -- see sgp4.hpp's own note

#include <cmath>

// PROVENANCE.md §38.4 has the full account: which STR3 section (Hoots &
// Roehrich 1980, §6/§7/§10/§11) each block below is ported from, and where
// in VAL06 (Vallado, Crawford, Hujsak & Kelso 2006) each correction is
// described. Every correction is marked CORRECTION below with its own
// citation, so this file can be checked against STR3 section by section,
// with the specific, named divergences visible rather than silent.

namespace odl::io {

namespace {

// -- WGS-72. VAL06 Table 2 gives J2/J3/J4/QO/SO/XKMPER at the SAME
//    precision STR3 §11's own DATA statement already states -- nothing to
//    correct there. XKE and THDT (VAL06 calls the same quantity RPTIM) are
//    DERIVED constants, and VAL06 gives them fuller precision -- CORRECTION,
//    VAL06 §III ("the move to double-precision code... increase in accuracy
//    for certain astrodynamic constants") and Table 2/§VI.B. --
constexpr double kAe = 1.0;
constexpr double kXkmper = 6378.135;
constexpr double kXj2 = 1.082616e-3;
constexpr double kXj3 = -0.253881e-5;
constexpr double kXj4 = -1.65597e-6;
constexpr double kQo = 120.0;
constexpr double kSo = 78.0;
constexpr double kXmnpda = 1440.0;
// CORRECTION, VAL06 §III second bullet ("the move to double-precision code
// throughout... corresponding increase in accuracy for certain astrodynamic
// constants"): STR3's own TOTHRD=.66666667 is a rounding of 2/3, carried
// here at full double precision. Its effect is invisible in a low orbit and
// ~0.4 m at geostationary radius (a ~ (xke/n)^TOTHRD, a 3.3e-9 exponent
// error times ln(xke/n) ~ 2.8): every near-GEO case in the verification
// battery carried exactly that 0.4 m constant offset until this changed
// (PROVENANCE.md §38.6's own table).
constexpr double kTothrd = 2.0 / 3.0;
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwopi = 2.0 * kPi;
constexpr double kDe2ra = kPi / 180.0;
// CORRECTION, VAL06 §III third bullet: "Spacetrack Report Number 6 changed
// the tolerance to 10-12 (commensurate with double-precision work)". STR3's
// own E6A=1.E-6 stopped Newton's iteration one step before its answer was
// good (the FORTRAN exits to the sines/cosines of the PREVIOUS iterate), an
// up-to-1e-6-rad error in E -- metres in a low orbit, tens of metres at
// GEO.
constexpr double kKeplerTol = 1.0e-12;
// CORRECTION, VAL06 §VI.E: "an even simpler option fixes a limit of 0.9 -
// 1.0 for the maximum correction" to each Newton step (Crawford 1995's own
// +/-e bound is the other option VAL06 names). 0.95 here; 0.9 and 1.0 give
// bit-identical results over the whole verification battery. The +/-e
// option was tried first and is WRONG as written here: SGP4's `e` at that
// point is the drag-decayed eccentricity, but the quantity Crawford's bound
// belongs to is the EFFECTIVE eccentricity sqrt(axn^2+ayn^2), which the
// long-period term `aynl` can push above `e` -- near-circular decaying
// satellites (28350, 22312, 28057) then got a clamp tighter than their own
// true Newton step.
constexpr double kKeplerStepLimit = 0.95;
constexpr double kXke = 0.0743669161331734;   // STR3: 0.743669161E-1. VAL06 Table 2: fuller.
constexpr double kThdt = 0.00437526908802;    // STR3 THDT=4.3752691E-3. VAL06 "RPTIM": fuller.
constexpr double kXpdotp = kXmnpda / kTwopi;  // VAL06 §VI.B: rev/day -> rad/min

constexpr double kCk2 = 0.5 * kXj2 * kAe * kAe;
constexpr double kCk4 = -0.375 * kXj4 * kAe * kAe * kAe * kAe;

// IOSG-F-001: VAL06 §VI.A, "inclination values near 180.0 degrees can cause
// divide-by-zero problems in the initialization and the routine operation...
// fixed by setting a tolerance in both routines." Checked at init on the
// TLE's own inclination and again in operation on the perturbed one.
constexpr double kInclTolRad = 1.5e-3;  // ~0.086 deg

// Eccentricity limits. VAL06 Table 1 states the error trap for satellites
// 28350 and 22312 ("modified eccentricity too low"); the published .e files
// for both end one step before the drag-modified eccentricity reaches
// -0.001 (28350: -9.2e-4 at the last row, 1440 min, crossing -1e-3 near 1470;
// Table 1: "approximately 1460 minutes"), which fixes the trap at -0.001.
// Between 0 and the trap the vectors fix the eccentricity at a floor of
// 1.0e-6: 22312's last row (e = -2.6e-5 unfloored) and 33335 (TLE e =
// 4.0e-7) agree with the published rows only at 1.0e-6, and the agreement
// is V-shaped and sharp -- 5% either side is 4 m off at 33335 (PROVENANCE.md
// §38.6, §38.8). No text states the floor's value; it is determined by the
// vectors. 28350, whose own residual was a test-reader artefact until
// 2026-10-06 (§38.7), is a third satellite on the same V (0.64 m at 9.5e-7
// and at 1.05e-6, 6 micrometres at 1.0e-6), and none of the three was needed to
// fix the others.
constexpr double kEccTrap = -0.001;  // NOT-A-UNIT-CROSSING: a dimensionless eccentricity bound, VAL06 Table 1's error trap
constexpr double kEccFloor = 1.0e-6;
// VAL06 Table 1, satellite 28057 ("certain drag terms are set to zero to
// avoid math errors / loss of precision"), and the verification file's own
// comment on that case ("ecc = 8.84E-5 (< 1.0e-4) / drop certain normal drag
// terms").
constexpr double kLowEccTol = 1.0e-4;

double qoms2t_const() { return std::pow((kQo - kSo) * kAe / kXkmper, 4); }
double s_const() { return kAe * (1.0 + kSo / kXkmper); }

// STR3 §11's own ACTAN: atan2(sinx,cosx), wrapped to [0, 2*pi) instead of
// (-pi, pi]. Implemented from its own branch structure, not std::atan2 plus
// a wrap, so a COSX==0.0 / SINX==0.0 edge case matches the FORTRAN's own
// value-equality branches exactly (including -0.0, which std::atan2's own
// signed-zero handling treats differently from a bare `== 0.0` comparison).
double actan(double sinx, double cosx) {
    if (cosx == 0.0) {
        if (sinx == 0.0) return 0.0;
        return sinx > 0.0 ? kPi / 2.0 : 3.0 * kPi / 2.0;
    }
    if (cosx > 0.0) {
        if (sinx == 0.0) return 0.0;
        return sinx > 0.0 ? std::atan(sinx / cosx) : kTwopi + std::atan(sinx / cosx);
    }
    return kPi + std::atan(sinx / cosx);
}

// STR3 §11's own FMOD2P: wraps to [0, 2*pi), truncating toward zero first
// (the FORTRAN's own real-to-INTEGER assignment), then correcting a
// still-negative result -- not std::fmod, whose truncation direction for a
// negative argument differs.
double fmod2p(double x) {
    double result = x - std::trunc(x / kTwopi) * kTwopi;
    if (result < 0.0) result += kTwopi;
    return result;
}

}  // namespace

time::Calendar tle_epoch_calendar(const Tle& tle) {
    // Pivot at 57 -- see sgp4.hpp's own note; not part of SGP4's own
    // mathematical definition (VAL06 §II.E).
    const int year = tle.epoch_year < 57 ? 2000 + tle.epoch_year : 1900 + tle.epoch_year;
    const int day_of_year = static_cast<int>(tle.epoch_day);
    const double frac_day = tle.epoch_day - static_cast<double>(day_of_year);

    static constexpr int kDaysInMonth[2][12] = {
        {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
        {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    };
    const bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    int remaining_days = day_of_year - 1;  // day_of_year is 1-indexed (Jan 1 = 1)
    int month = 1;
    while (month <= 12 && remaining_days >= kDaysInMonth[leap ? 1 : 0][month - 1]) {
        remaining_days -= kDaysInMonth[leap ? 1 : 0][month - 1];
        ++month;
    }
    const int day = remaining_days + 1;
    const double day_seconds = frac_day * 86400.0;
    const int hour = static_cast<int>(day_seconds / 3600.0);
    const int minute = static_cast<int>((day_seconds - hour * 3600.0) / 60.0);
    const double second = day_seconds - hour * 3600.0 - minute * 60.0;
    return time::Calendar{year, month, day, hour, minute, second};
}

namespace {

/// STR3 §10's own repeated block (labels 20-40 in DPINIT): the lunar-solar
/// term computation, run once with solar parameters and once with lunar --
/// same formulas, different inputs, so factored here rather than printed
/// twice. Field names match STR3's own "X..." (raw, reused-for-both-calls)
/// variable names.
struct LuniSolarTerms {
    double se = 0, si = 0, sl = 0, sgh = 0, sh = 0;
    double ee2 = 0, e3 = 0, xi2 = 0, xi3 = 0, xl2 = 0, xl3 = 0, xl4 = 0;
    double xgh2 = 0, xgh3 = 0, xgh4 = 0, xh2 = 0, xh3 = 0;
};

LuniSolarTerms lunisolar_term_block(double zcosg, double zsing, double zcosi, double zsini,
                                    double zcosh, double zsinh, double cc, double zn, double ze,
                                    double cosiq, double siniq, double cosomo, double sinomo,
                                    double eosq, double bsq, double eq, double xnq, double xqncl) {
    const double a1v = zcosg * zcosh + zsing * zcosi * zsinh;
    const double a3v = -zsing * zcosh + zcosg * zcosi * zsinh;
    const double a7v = -zcosg * zsinh + zsing * zcosi * zcosh;
    const double a8v = zsing * zsini;
    const double a9v = zsing * zsinh + zcosg * zcosi * zcosh;
    const double a10v = zcosg * zsini;
    const double a2v = cosiq * a7v + siniq * a8v;
    const double a4v = cosiq * a9v + siniq * a10v;
    const double a5v = -siniq * a7v + cosiq * a8v;
    const double a6v = -siniq * a9v + cosiq * a10v;
    const double x1 = a1v * cosomo + a2v * sinomo;
    const double x2 = a3v * cosomo + a4v * sinomo;
    const double x3 = -a1v * sinomo + a2v * cosomo;
    const double x4 = -a3v * sinomo + a4v * cosomo;
    const double x5 = a5v * sinomo, x6 = a6v * sinomo, x7 = a5v * cosomo, x8 = a6v * cosomo;
    const double z31 = 12.0 * x1 * x1 - 3.0 * x3 * x3;
    const double z32 = 24.0 * x1 * x2 - 6.0 * x3 * x4;
    const double z33 = 12.0 * x2 * x2 - 3.0 * x4 * x4;
    double z1 = 3.0 * (a1v * a1v + a2v * a2v) + z31 * eosq;
    double z2 = 6.0 * (a1v * a3v + a2v * a4v) + z32 * eosq;
    double z3 = 3.0 * (a3v * a3v + a4v * a4v) + z33 * eosq;
    const double z11 = -6.0 * a1v * a5v + eosq * (-24.0 * x1 * x7 - 6.0 * x3 * x5);
    const double z12 = -6.0 * (a1v * a6v + a3v * a5v) +
        eosq * (-24.0 * (x2 * x7 + x1 * x8) - 6.0 * (x3 * x6 + x4 * x5));
    const double z13 = -6.0 * a3v * a6v + eosq * (-24.0 * x2 * x8 - 6.0 * x4 * x6);
    const double z21 = 6.0 * a2v * a5v + eosq * (24.0 * x1 * x5 - 6.0 * x3 * x7);
    const double z22 = 6.0 * (a4v * a5v + a2v * a6v) +
        eosq * (24.0 * (x2 * x5 + x1 * x6) - 6.0 * (x4 * x7 + x3 * x8));
    const double z23 = 6.0 * a4v * a6v + eosq * (24.0 * x2 * x6 - 6.0 * x4 * x8);
    z1 = z1 + z1 + bsq * z31;
    z2 = z2 + z2 + bsq * z32;
    z3 = z3 + z3 + bsq * z33;
    const double s3 = cc / xnq;
    const double s2 = -0.5 * s3 / std::sqrt(1.0 - eosq);  // RTEQSQ == BETAO == sqrt(1-e^2)
    const double s4v = s3 * std::sqrt(1.0 - eosq);
    const double s1 = -15.0 * eq * s4v;
    const double s5 = x1 * x3 + x2 * x4;
    const double s6 = x2 * x3 + x1 * x4;
    const double s7 = x2 * x4 - x1 * x3;
    LuniSolarTerms t;
    t.se = s1 * zn * s5;
    t.si = s2 * zn * (z11 + z13);
    t.sl = -zn * s3 * (z1 + z3 - 14.0 - 6.0 * eosq);
    t.sgh = s4v * zn * (z31 + z33 - 6.0);
    t.sh = xqncl < 5.2359877e-2 ? 0.0 : -zn * s2 * (z21 + z23);
    t.ee2 = 2.0 * s1 * s6;
    t.e3 = 2.0 * s1 * s7;
    t.xi2 = 2.0 * s2 * z12;
    t.xi3 = 2.0 * s2 * (z13 - z11);
    t.xl2 = -2.0 * s3 * z2;
    t.xl3 = -2.0 * s3 * (z3 - z1);
    t.xl4 = -2.0 * s3 * (-21.0 - 9.0 * eosq) * ze;
    t.xgh2 = 2.0 * s4v * z32;
    t.xgh3 = 2.0 * s4v * (z33 - z31);
    t.xgh4 = -18.0 * s4v * ze;
    t.xh2 = -2.0 * s2 * z22;
    t.xh3 = -2.0 * s2 * (z23 - z21);
    return t;
}

}  // namespace

odl::Result<Sgp4InitialState, Sgp4Error> sgp4_init(const Tle& tle) {
    Sgp4InitialState st;
    const double xincl = tle.inclination_deg * kDe2ra;

    if (std::abs(kPi - xincl) < kInclTolRad) {
        return odl::err(Sgp4Error{"IOSG-F-001", "inclination within 0.086 deg of 180 deg"});
    }

    const double xnodeo = tle.raan_deg * kDe2ra;
    const double omegao = tle.arg_perigee_deg * kDe2ra;
    const double xmo = tle.mean_anomaly_deg * kDe2ra;
    const double eo = tle.eccentricity;
    const double xno_radpm = tle.mean_motion_rev_per_day * kTwopi / kXmnpda;  // STR3 §11's own conversion

    // Saved for BOTH paths -- `sgp4_propagate` takes only `Sgp4InitialState`,
    // never the original `Tle` again, so its own secular update (near-earth
    // or deep-space alike) needs these back from here, not re-parsed.
    st.xnodeo_saved = xnodeo;
    st.omegao_saved = omegao;
    st.xmo_saved = xmo;
    st.xincl_saved = xincl;
    st.bstar = tle.bstar;

    // -- STR3 §6/§7's SHARED first step (also §11's own driver copy;
    //    computed once here, not this tree's own three-fold repeat of the
    //    same formula) --
    const double a1 = std::pow(kXke / xno_radpm, kTothrd);
    const double cosio = std::cos(xincl);
    const double theta2 = cosio * cosio;
    const double x3thm1 = 3.0 * theta2 - 1.0;
    const double eosq = eo * eo;
    const double betao2 = 1.0 - eosq;
    const double betao = std::sqrt(betao2);
    const double del1_mm = 1.5 * kCk2 * x3thm1 / (a1 * a1 * betao * betao2);
    const double ao = a1 * (1.0 - del1_mm * (0.5 * kTothrd + del1_mm * (1.0 + 134.0 / 81.0 * del1_mm)));
    const double delo_mm = 1.5 * kCk2 * x3thm1 / (ao * ao * betao * betao2);
    const double xnodp = xno_radpm / (1.0 + delo_mm);
    // DEVIATION FROM STR3 -- determined by the vectors, not stated in either
    // text. STR3 §6/§7 write AODP=AO/(1.-DELO). Here AODP is instead the
    // Kepler-consistent axis (xke/xnodp)^(2/3) of the same recovered mean
    // motion. The two agree to ~1e-13 for every ordinary orbit (verified
    // for 88888, 00005, 06251, 11801 and a GEO case) and differ at O(DELO^2)
    // only when the J2 correction is large: satellite 33333 (e = 0.995,
    // DELO = -0.108) has them 0.064% apart, i.e. PINVSQ 0.142% apart, and
    // PINVSQ multiplies every J2/J4 secular rate. A fit of free factors on
    // the three secular-rate terms to 33333's rows returned (1.001423,
    // 1.001421, 1.002847) -- exactly PINVSQ scaled by 1.001421, with
    // nothing else altered -- and PINVSQ from this definition is 1.001422.
    // With it, 33333 agrees to 7 micrometres at all five rows; with STR3's
    // AO/(1-DELO), by 3398 km at the last (PROVENANCE.md §38.6, §38.8). Not
    // resting on 33333 alone: with STR3's definition 29141 sits 1.6 mm, 28350
    // 0.22 mm, 22312 43 micrometres, 28623 19 and 28872 16 from their published
    // rows, and with this one all five sit at 6-7 (the files' print resolution).
    const double aodp = std::pow(kXke / xnodp, kTothrd);

    st.deep_space = (kTwopi / xnodp) >= 225.0;  // STR3 §11's own period dispatch
    st.xnodp = xnodp;
    st.aodp = aodp;
    st.eo = eo;
    st.cosio = cosio;
    const double sinio = std::sin(xincl);
    st.sinio = sinio;
    st.theta2 = theta2;
    st.x3thm1 = x3thm1;
    st.x1mth2 = 1.0 - theta2;
    st.x7thm1 = 7.0 * theta2 - 1.0;
    st.betao = betao;
    st.betao2 = betao2;

    // -- perigee-dependent s*, (qo-s)^4 (STR3 §6/§7, identical in both) --
    double s4 = s_const();
    double qoms24 = qoms2t_const();
    const double perige = (aodp * (1.0 - eo) - kAe) * kXkmper;
    if (perige < 156.0) {
        s4 = perige > 98.0 ? perige - 78.0 : 20.0;
        qoms24 = std::pow((120.0 - s4) / kXkmper, 4);
        s4 = s4 / kXkmper + kAe;
    }

    const double pinvsq = 1.0 / (aodp * aodp * betao2 * betao2);
    const double tsi = 1.0 / (aodp - s4);
    const double eta = aodp * eo * tsi;
    const double etasq = eta * eta;
    const double eeta = eo * eta;
    const double psisq = std::abs(1.0 - etasq);
    const double coef = qoms24 * std::pow(tsi, 4);
    const double coef1 = coef / std::pow(psisq, 3.5);
    const double c2 = coef1 * xnodp *
        (aodp * (1.0 + 1.5 * etasq + eeta * (4.0 + etasq)) +
         0.75 * kCk2 * tsi / psisq * x3thm1 * (8.0 + 3.0 * etasq * (8.0 + etasq)));
    const double c1 = tle.bstar * c2;
    const double a3ovk2 = -kXj3 / kCk2 * kAe * kAe * kAe;
    const double c4 = 2.0 * xnodp * coef1 * aodp * betao2 *
        (eta * (2.0 + 0.5 * etasq) + eo * (0.5 + 2.0 * etasq) -
         2.0 * kCk2 * tsi / (aodp * psisq) *
             (-3.0 * x3thm1 * (1.0 - 2.0 * eeta + etasq * (1.5 - 0.5 * eeta)) +
              0.75 * st.x1mth2 * (2.0 * etasq - eeta * (1.0 + etasq)) * std::cos(2.0 * omegao)));
    const double theta4 = theta2 * theta2;
    const double temp1 = 3.0 * kCk2 * pinvsq * xnodp;
    const double temp2 = temp1 * kCk2 * pinvsq;
    const double temp3 = 1.25 * kCk4 * pinvsq * pinvsq * xnodp;
    const double xmdot = xnodp + 0.5 * temp1 * betao * x3thm1 +
        0.0625 * temp2 * betao * (13.0 - 78.0 * theta2 + 137.0 * theta4);
    const double x1m5th = 1.0 - 5.0 * theta2;
    const double omgdot = -0.5 * temp1 * x1m5th +
        0.0625 * temp2 * (7.0 - 114.0 * theta2 + 395.0 * theta4) +
        temp3 * (3.0 - 36.0 * theta2 + 49.0 * theta4);
    const double xhdot1 = -temp1 * cosio;
    const double xnodot = xhdot1 + (0.5 * temp2 * (4.0 - 19.0 * theta2) +
                                     2.0 * temp3 * (3.0 - 7.0 * theta2)) * cosio;
    const double xnodcf = 3.5 * betao2 * xhdot1 * c1;
    const double xlcof = 0.125 * a3ovk2 * sinio * (3.0 + 5.0 * cosio) / (1.0 + cosio);
    const double aycof = 0.25 * a3ovk2 * sinio;
    // T2COF: STR3 §6 AND §7 both compute it (SGP4's own listing states it right
    // after XNODCF; SDP4's own listing does too, identically) -- shared, like
    // XNODCF/XLCOF/AYCOF above, NOT near-earth-only. A first draft scoped it
    // inside the near-earth branch below; every deep-space case defaulted it
    // to 0, zeroing TEMPL and silently dropping SGP4/SDP4's own drag
    // contribution to mean longitude for every deep-space orbit. Found by an
    // independent Python re-derivation of the full DPSEC/DPPER chain for
    // satellite 11801 disagreeing with this port only in TEMPL.
    const double t2cof = 1.5 * c1;

    st.c1 = c1;
    st.c2 = c2;
    st.c4 = c4;
    st.omgdot = omgdot;
    st.xmdot = xmdot;
    st.xnodot = xnodot;
    st.xnodcf = xnodcf;
    st.a3ovk2 = a3ovk2;
    st.xlcof = xlcof;
    st.aycof = aycof;
    st.t2cof = t2cof;

    if (!st.deep_space) {
        // -- near-earth only: STR3 §6's own C3/C5 and ISIMP-gated terms --
        // C3 (and OMGCOF through it) divide by EO, XMCOF by EETA = EO*ETA:
        // below kLowEccTol those are the "certain drag terms" VAL06 Table 1
        // and the verification file's own comment on 28057 say are set to
        // zero. WHICH terms is this port's reading ("to avoid math errors /
        // loss of precision" names the two that divide by the eccentricity),
        // and the vectors confirm it now that 28057 is read correctly: zeroing
        // exactly C3/OMGCOF and XMCOF leaves 28057 7 micrometres from its
        // published rows; none zeroed leaves 16 mm, C3 alone 14 mm, XMCOF alone
        // 1.7 mm, and those two plus C5 31 mm (PROVENANCE.md §38.8).
        if (eo >= kLowEccTol) {
            st.c3 = coef * tsi * a3ovk2 * xnodp * kAe * sinio / eo;
            st.omgcof = tle.bstar * st.c3 * std::cos(omegao);
            st.xmcof = -kTothrd * coef * tle.bstar * kAe / eeta;
        }
        st.c5 = 2.0 * coef1 * aodp * betao2 * (1.0 + 2.75 * (etasq + eeta) + eeta * etasq);
        st.eta = eta;
        st.delmo = std::pow(1.0 + eta * std::cos(xmo), 3);
        st.sinmo = std::sin(xmo);

        st.isimp = (aodp * (1.0 - eo) / kAe) < (220.0 / kXkmper + kAe);
        if (!st.isimp) {
            const double c1sq = c1 * c1;
            const double d2 = 4.0 * aodp * tsi * c1sq;
            const double temp = d2 * tsi * c1 / 3.0;
            const double d3 = (17.0 * aodp + s4) * temp;
            const double d4 = 0.5 * temp * aodp * tsi * (221.0 * aodp + 31.0 * s4) * c1;
            st.d2 = d2;
            st.d3 = d3;
            st.d4 = d4;
            st.t3cof = d2 + 2.0 * c1sq;
            st.t4cof = 0.25 * (3.0 * d3 + c1 * (12.0 * d2 + 10.0 * c1sq));
            st.t5cof = 0.2 * (3.0 * d4 + 12.0 * c1 * d3 + 6.0 * d2 * d2 + 15.0 * c1sq * (2.0 * d2 + c1sq));
        }
        return st;
    }

    // -- deep space: STR3 §10 DPINIT --
    st.omgdt_saved = omgdot;

    // THGR, the Greenwich hour angle at epoch -- CORRECTION, VAL06 §II.F. STR3's
    // own THETAG is the 1950-epoch linear form 1.72944494 + 6.3003880987*DS50.
    // §II.F lists the versions in circulation and prints the constants of the
    // 1970-epoch one:
    //     C1 = 1.72027916940703639D-2, THGR70 = 1.7321343856509374D0,
    //     FK5R = 5.07551419432269442D-15, C1P2P = C1 + TWOPI,
    //     THGR = DMOD(THGR70 + C1*DS70 + C1P2P*TFRAC + TS70*TS70*FK5R, TWOPI)
    // ("these approaches yield 'essentially' the same values"). They do not, for
    // a 2006 epoch: the 1970 form is 6.75e-6 rad ahead of STR3's (4.3e-6 rad for
    // a 1980 epoch) -- 1.4 arcsec, nothing at all to a TLE, but the 12 h and 24 h
    // resonance phases read it: with STR3's form all eleven geopotential-
    // resonant satellites in the verification battery sat 4 mm to 36 cm from the
    // published rows; with this one they sit within 0.07 mm. A free additive
    // shift fitted to each resonant case alone returned 6.751/6.746/6.746/6.748
    // e-6 rad for the four 12 h cases that fix it, against 6.7479e-6 computed
    // from the two formulas (PROVENANCE.md §38.8). TS70 is not defined in the
    // paper; it is read as the elapsed days DS70 + TFRAC, which agrees with the
    // IAU-1982 GMST polynomial (VAL06 eq. (2)) to 1.4e-9 rad at every epoch tried.
    // DS50 (days since 1950 Jan 0.0) is computed from this tree's own already-
    // parsed TLE epoch via `calendar_elapsed_seconds`, not by re-deriving
    // THETAG's fragile two-digit-year decode (which, as printed, maps a 2006
    // epoch to 1986); 1970 Jan 0.0 is exactly 7305 days later.
    const time::Calendar epoch_cal = tle_epoch_calendar(tle);
    const double ds50 =
        calendar_elapsed_seconds(time::Calendar{1949, 12, 31, 0, 0, 0.0}, epoch_cal) / 86400.0;
    constexpr double kDaysFrom1950To1970 = 7305.0;
    const double ts70 = ds50 - kDaysFrom1950To1970;
    const double ds70 = std::floor(ts70);
    const double tfrac = ts70 - ds70;
    constexpr double kC1 = 1.72027916940703639e-2;
    constexpr double kThgr70 = 1.7321343856509374;
    constexpr double kFk5r = 5.07551419432269442e-15;
    const double thgr = fmod2p(kThgr70 + kC1 * ds70 + (kC1 + kTwopi) * tfrac + ts70 * ts70 * kFk5r);
    st.thgr = thgr;

    const double eq = eo;
    const double xnq = xnodp;
    const double aqnv = 1.0 / aodp;
    const double xqncl = xincl;
    const double xmao = xmo;
    const double xpidot = omgdot + xnodot;
    const double sinq = std::sin(xnodeo);
    const double cosq = std::cos(xnodeo);
    st.eq = eq;
    st.xqncl = xqncl;
    st.aqnv = aqnv;
    st.xmao = xmao;
    st.xpidot = xpidot;

    const double siniq = sinio, cosiq = cosio, bsq = betao2;
    const double sinomo = std::sin(omegao), cosomo = std::cos(omegao);

    const double day = ds50 + 18261.5;
    const double xnodce = 4.5236020 - 9.2422029e-4 * day;
    const double stem = std::sin(xnodce), ctem = std::cos(xnodce);
    const double zcosil = 0.91375164 - 0.03568096 * ctem;
    const double zsinil = std::sqrt(1.0 - zcosil * zcosil);
    const double zsinhl = 0.089683511 * stem / zsinil;
    const double zcoshl = std::sqrt(1.0 - zsinhl * zsinhl);
    const double c_lun = 4.7199672 + 0.22997150 * day;
    const double gam = 5.8351514 + 0.0019443680 * day;
    st.zmol = fmod2p(c_lun - gam);
    double zx = 0.39785416 * stem / zsinil;
    const double zy = zcoshl * ctem + 0.91744867 * zsinhl * stem;
    zx = actan(zx, zy);
    zx = gam + zx - xnodce;
    const double zcosgl = std::cos(zx), zsingl = std::sin(zx);
    st.zmos = fmod2p(6.2565837 + 0.017201977 * day);

    const LuniSolarTerms sol = lunisolar_term_block(
        0.1945905, -0.98088458, 0.91744867, 0.39785416, cosq, sinq, 2.9864797e-6, 1.19459e-5, 0.01675,
        cosiq, siniq, cosomo, sinomo, eosq, bsq, eq, xnq, xqncl);
    const LuniSolarTerms lun = lunisolar_term_block(
        zcosgl, zsingl, zcosil, zsinil, zcoshl * cosq + zsinhl * sinq, sinq * zcoshl - cosq * zsinhl,
        4.7968065e-7, 1.5835218e-4, 0.05490, cosiq, siniq, cosomo, sinomo, eosq, bsq, eq, xnq, xqncl);

    st.sse = sol.se + lun.se;
    st.ssi = sol.si + lun.si;
    st.ssl = sol.sl + lun.sl;
    st.ssh = sol.sh / siniq + lun.sh / siniq;
    st.ssg = (sol.sgh - cosiq * (sol.sh / siniq)) + (lun.sgh - cosiq / siniq * lun.sh);
    st.se2 = sol.ee2; st.si2 = sol.xi2; st.sl2 = sol.xl2; st.sgh2 = sol.xgh2; st.sh2 = sol.xh2;
    st.se3 = sol.e3; st.si3 = sol.xi3; st.sl3 = sol.xl3; st.sgh3 = sol.xgh3; st.sh3 = sol.xh3;
    st.sl4 = sol.xl4; st.sgh4 = sol.xgh4;
    st.lun_ee2 = lun.ee2; st.lun_e3 = lun.e3;
    st.lun_xi2 = lun.xi2; st.lun_xi3 = lun.xi3;
    st.lun_xl2 = lun.xl2; st.lun_xl3 = lun.xl3; st.lun_xl4 = lun.xl4;
    st.lun_xgh2 = lun.xgh2; st.lun_xgh3 = lun.xgh3; st.lun_xgh4 = lun.xgh4;
    st.lun_xh2 = lun.xh2; st.lun_xh3 = lun.xh3;

    // -- geopotential resonance initialization. STR3 §10's own two bands:
    //    n in (0.0034906585, 0.0052359877) rad/min (~1436 min, sidereal-day
    //    period) is the SYNCHRONOUS/24h case; n in [8.26e-3, 9.24e-3] with
    //    e>=0.5 (~720 min, 12h) is the resonant Molniya-style case. --
    const bool is_synchronous_band = xnq < 0.0052359877 && xnq > 0.0034906585;
    const bool is_12h_resonant_band = !is_synchronous_band && xnq >= 8.26e-3 && xnq <= 9.24e-3 && eq >= 0.5;

    if (is_synchronous_band) {
        st.iresfl = true;
        st.isynfl = true;
        const double g200 = 1.0 + eosq * (-2.5 + 0.8125 * eosq);
        const double g310 = 1.0 + 2.0 * eosq;
        const double g300 = 1.0 + eosq * (-6.0 + 6.60937 * eosq);
        const double f220 = 0.75 * (1.0 + cosiq) * (1.0 + cosiq);
        const double f311 = 0.9375 * siniq * siniq * (1.0 + 3.0 * cosiq) - 0.75 * (1.0 + cosiq);
        double f330 = 1.0 + cosiq;
        f330 = 1.875 * f330 * f330 * f330;
        double del1v = 3.0 * xnq * xnq * aqnv * aqnv;
        const double del2v = 2.0 * del1v * f220 * g200 * 1.7891679e-6;
        const double del3v = 3.0 * del1v * f330 * g300 * 2.2123015e-7 * aqnv;
        del1v = del1v * f311 * g310 * 2.1460748e-6 * aqnv;
        st.del1 = del1v;
        st.del2 = del2v;
        st.del3 = del3v;
        st.fasx2 = 0.13130908;
        st.fasx4 = 2.8843198;
        st.fasx6 = 0.37448087;
        st.xlamo = xmao + xnodeo + omegao - thgr;
        double bfact = xmdot + xpidot - kThdt;  // STR3's own XLLDOT == XMDOT (DPINIT's own arg order)
        bfact += st.ssl + st.ssg + st.ssh;
        st.xfact = bfact - xnq;
    } else if (is_12h_resonant_band) {
        st.iresfl = true;
        const double eoc = eq * eosq;
        const double g201 = -0.306 - (eq - 0.64) * 0.440;
        double g211, g310, g322, g410, g422, g520;
        if (eq <= 0.65) {
            g211 = 3.616 - 13.247 * eq + 16.290 * eosq;
            g310 = -19.302 + 117.390 * eq - 228.419 * eosq + 156.591 * eoc;
            g322 = -18.9068 + 109.7927 * eq - 214.6334 * eosq + 146.5816 * eoc;
            g410 = -41.122 + 242.694 * eq - 471.094 * eosq + 313.953 * eoc;
            g422 = -146.407 + 841.880 * eq - 1629.014 * eosq + 1083.435 * eoc;
            // DEVIATION FROM STR3 -- determined by the vectors, not stated in either
            // text. STR3's listing prints this coefficient as `-5740*EQSQ` (checked
            // on the page image, p.63); every other coefficient in the block has
            // 3-6 decimals. Satellite 26975, the battery's only case with
            // 0.5 <= e <= 0.65, sits 7.6 cm from its published rows with 5740
            // and 14 micrometres with 5740.032: a one-parameter fit of this
            // coefficient alone to 26975's rows, from a blind scan of 5739.9-5740.1,
            // is V-shaped (2.4e-3 km per unit) with its minimum at 5740.0320 +/-
            // 0.0001 (PROVENANCE.md §38.8). One satellite exercises this branch, so
            // it is not independent evidence the value is right.
            g520 = -532.114 + 3017.977 * eq - 5740.032 * eosq + 3708.276 * eoc;
        } else {
            g211 = -72.099 + 331.819 * eq - 508.738 * eosq + 266.724 * eoc;
            g310 = -346.844 + 1582.851 * eq - 2415.925 * eosq + 1246.113 * eoc;
            g322 = -342.585 + 1554.908 * eq - 2366.899 * eosq + 1215.972 * eoc;
            g410 = -1052.797 + 4758.686 * eq - 7193.992 * eosq + 3651.957 * eoc;
            g422 = -3581.69 + 16178.11 * eq - 24462.77 * eosq + 12422.52 * eoc;
            g520 = eq <= 0.715 ? 1464.74 - 4664.75 * eq + 3763.64 * eosq
                                : -5149.66 + 29936.92 * eq - 54087.36 * eosq + 31324.56 * eoc;
        }
        double g533, g521, g532;
        if (eq < 0.7) {
            g533 = -919.2277 + 4988.61 * eq - 9064.77 * eosq + 5542.21 * eoc;
            g521 = -822.71072 + 4568.6173 * eq - 8491.4146 * eosq + 5337.524 * eoc;
            g532 = -853.666 + 4690.25 * eq - 8624.77 * eosq + 5341.4 * eoc;
        } else {
            g533 = -37995.78 + 161616.52 * eq - 229838.2 * eosq + 109377.94 * eoc;
            g521 = -51752.104 + 218913.95 * eq - 309468.16 * eosq + 146349.42 * eoc;
            g532 = -40023.88 + 170470.89 * eq - 242699.48 * eosq + 115605.82 * eoc;
        }
        const double sini2 = siniq * siniq;
        const double f220 = 0.75 * (1.0 + 2.0 * cosiq + theta2);
        const double f221 = 1.5 * sini2;
        const double f321 = 1.875 * siniq * (1.0 - 2.0 * cosiq - 3.0 * theta2);
        const double f322 = -1.875 * siniq * (1.0 + 2.0 * cosiq - 3.0 * theta2);
        const double f441 = 35.0 * sini2 * f220;
        const double f442 = 39.3750 * sini2 * sini2;
        const double f522 = 9.84375 * siniq *
            (sini2 * (1.0 - 2.0 * cosiq - 5.0 * theta2) + 0.33333333 * (-2.0 + 4.0 * cosiq + 6.0 * theta2));
        const double f523 = siniq * (4.92187512 * sini2 * (-2.0 - 4.0 * cosiq + 10.0 * theta2) +
                                     6.56250012 * (1.0 + 2.0 * cosiq - 3.0 * theta2));
        const double f542 = 29.53125 * siniq *
            (2.0 - 8.0 * cosiq + theta2 * (-12.0 + 8.0 * cosiq + 10.0 * theta2));
        const double f543 = 29.53125 * siniq *
            (-2.0 - 8.0 * cosiq + theta2 * (12.0 + 8.0 * cosiq - 10.0 * theta2));
        const double xno2 = xnq * xnq;
        const double ainv2 = aqnv * aqnv;
        double temp1v = 3.0 * xno2 * ainv2;
        double tempv = temp1v * 1.7891679e-6;
        st.d2201 = tempv * f220 * g201;
        st.d2211 = tempv * f221 * g211;
        temp1v *= aqnv;
        tempv = temp1v * 3.7393792e-7;
        st.d3210 = tempv * f321 * g310;
        st.d3222 = tempv * f322 * g322;
        temp1v *= aqnv;
        tempv = 2.0 * temp1v * 7.3636953e-9;
        st.d4410 = tempv * f441 * g410;
        st.d4422 = tempv * f442 * g422;
        temp1v *= aqnv;
        tempv = temp1v * 1.1428639e-7;
        st.d5220 = tempv * f522 * g520;
        st.d5232 = tempv * f523 * g532;
        tempv = 2.0 * temp1v * 2.1765803e-9;
        st.d5421 = tempv * f542 * g521;
        st.d5433 = tempv * f543 * g533;
        st.xlamo = xmao + xnodeo + xnodeo - thgr - thgr;
        double bfact = xmdot + xnodot + xnodot - kThdt - kThdt;
        bfact += st.ssl + st.ssh + st.ssh;
        st.xfact = bfact - xnq;
    }
    // Neither band: iresfl stays false -- DPSEC's own secular step (below)
    // still applies SSL/SSG/SSH/SSE/SSI; there is simply no resonance
    // integration to run.

    return st;
}

bool sgp4_is_deep_space(const Sgp4InitialState& state) noexcept { return state.deep_space; }

namespace {

constexpr double kG22 = 5.7686396, kG32 = 0.95240898, kG44 = 1.8014998, kG52 = 1.0508330, kG54 = 4.4108898;

struct DotTerms { double xndot, xnddt, xldot; };

/// STR3 §10 `DPSEC`'s own resonance DOT-TERMS block (labels 150/152/154),
/// evaluated at the integrator's own CURRENT (xli, xni, atime) -- `xldot`
/// depends on `xni`, the loop's own evolving state, not the fixed initial
/// mean motion.
DotTerms resonance_dot_terms(const Sgp4InitialState& st, double xli, double xni, double atime) {
    double xndot, xnddt;
    if (st.isynfl) {
        xndot = st.del1 * std::sin(xli - st.fasx2) + st.del2 * std::sin(2.0 * (xli - st.fasx4)) +
                st.del3 * std::sin(3.0 * (xli - st.fasx6));
        xnddt = st.del1 * std::cos(xli - st.fasx2) + 2.0 * st.del2 * std::cos(2.0 * (xli - st.fasx4)) +
                3.0 * st.del3 * std::cos(3.0 * (xli - st.fasx6));
    } else {
        const double xomi = st.omegao_saved + st.omgdt_saved * atime;
        const double x2omi = xomi + xomi;
        const double x2li = xli + xli;
        xndot = st.d2201 * std::sin(x2omi + xli - kG22) + st.d2211 * std::sin(xli - kG22) +
                st.d3210 * std::sin(xomi + xli - kG32) + st.d3222 * std::sin(-xomi + xli - kG32) +
                st.d4410 * std::sin(x2omi + x2li - kG44) + st.d4422 * std::sin(x2li - kG44) +
                st.d5220 * std::sin(xomi + xli - kG52) + st.d5232 * std::sin(-xomi + xli - kG52) +
                st.d5421 * std::sin(xomi + x2li - kG54) + st.d5433 * std::sin(-xomi + x2li - kG54);
        xnddt = st.d2201 * std::cos(x2omi + xli - kG22) + st.d2211 * std::cos(xli - kG22) +
                st.d3210 * std::cos(xomi + xli - kG32) + st.d3222 * std::cos(-xomi + xli - kG32) +
                st.d5220 * std::cos(xomi + xli - kG52) + st.d5232 * std::cos(-xomi + xli - kG52) +
                2.0 * (st.d4410 * std::cos(x2omi + x2li - kG44) + st.d4422 * std::cos(x2li - kG44) +
                       st.d5421 * std::cos(xomi + x2li - kG54) + st.d5433 * std::cos(-xomi + x2li - kG54));
    }
    const double xldot = xni + st.xfact;
    xnddt *= xldot;
    return {xndot, xnddt, xldot};
}

struct DpsecResult { double xll, omgasm, xnodes, em, xinc, xn; };

/// STR3 §10 `DPSEC`. CORRECTION, VAL06 §VI.D: re-derived from ATIME=0 on
/// EVERY call ("always integrate from the epoch to the required time, and
/// restart each time the model is called... led to repeatable results") --
/// this module also carries no state between calls at all, so there is
/// nothing to persist regardless.
///
/// CORRECTION, VAL06 §VI.C: STR3's own `IF(XINC .GE. 0.) ... XINC=-XINC,
/// XNODES+PI, OMGASM-PI` negative-inclination swap sat right here, on the
/// SECULAR inclination alone. VAL06: "we corrected this by removing the
/// quadrant check from DSINIT before the 'initialize resonance terms'
/// section, but kept the check in SGP4 before the 'long period periodics'
/// section" -- i.e. the swap moves to AFTER the lunar-solar periodics, on
/// the fully perturbed inclination (`sgp4_propagate`, below). Swapping early
/// is what VAL06 calls "correcting negative inclination prematurely" (its
/// satellites 25954 and 28626): the periodic `pinc` is then added to the
/// flipped, positive inclination instead of the true negative one.
DpsecResult dpsec(const Sgp4InitialState& st, double xll_in, double omgasm_in, double xnodes_in, double t) {
    DpsecResult r{};
    r.xll = xll_in + st.ssl * t;
    r.omgasm = omgasm_in + st.ssg * t;
    r.xnodes = xnodes_in + st.ssh * t;
    r.em = st.eq + st.sse * t;
    r.xinc = st.xqncl + st.ssi * t;
    r.xn = st.xnodp;
    if (!st.iresfl) return r;

    constexpr double kStepp = 720.0, kStep2 = 259200.0;
    const double delt0 = t >= 0.0 ? kStepp : -kStepp;
    double atime = 0.0;
    double xli = st.xlamo;
    double xni = st.xnodp;
    while (std::abs(t - atime) >= kStepp) {
        const DotTerms dt = resonance_dot_terms(st, xli, xni, atime);
        xli = xli + dt.xldot * delt0 + dt.xndot * kStep2;
        xni = xni + dt.xndot * delt0 + dt.xnddt * kStep2;
        atime += delt0;
    }
    const double ft = t - atime;
    const DotTerms dt = resonance_dot_terms(st, xli, xni, atime);
    const double xn = xni + dt.xndot * ft + dt.xnddt * ft * ft * 0.5;
    const double xl = xli + dt.xldot * ft + dt.xndot * ft * ft * 0.5;
    const double temp = -r.xnodes + st.thgr + t * kThdt;
    double xll = xl - r.omgasm + temp;
    if (!st.isynfl) xll = xl + temp + temp;
    r.xll = xll;
    r.xn = xn;
    return r;
}

struct DpperResult { double em, xinc, omgasm, xnodes, xll; };

/// STR3 §10 `DPPER`, with these corrections (each cited where it is made):
/// the SAVTSN 30-minute skip removed (VAL06 §III fourth bullet); the
/// inclination-dependent quantities taken from the PERTURBED inclination
/// (VAL06 §III seventh bullet); the Lyddane test on the perturbed
/// inclination (VAL06 §III fifth bullet / "Option (b)"); the node reduced
/// mod 2*pi and the result placed in the quadrant nearest the original
/// (VAL06 §III fifth bullet and its "intrinsic functions" bullet).
DpperResult dpper(const Sgp4InitialState& st, double em_in, double xinc_in, double omgasm_in,
                  double xnodes_in, double xll_in, double t) {
    // SAVTSN removed -- CORRECTION, VAL06 §III fourth bullet: "The practice of
    // only computing the lunar-solar terms if propagation time changes by
    // more than 30 minutes to save CPU effort was dropped... This was the
    // only function of the SAVTSN variable in the original DPPER subroutine."
    const double zm_s = st.zmos + 1.19459e-5 * t;
    double zf = zm_s + 2.0 * 0.01675 * std::sin(zm_s);
    double sinzf = std::sin(zf);
    double f2 = 0.5 * sinzf * sinzf - 0.25;
    double f3 = -0.5 * sinzf * std::cos(zf);
    const double ses = st.se2 * f2 + st.se3 * f3;
    const double sis = st.si2 * f2 + st.si3 * f3;
    const double sls = st.sl2 * f2 + st.sl3 * f3 + st.sl4 * sinzf;
    const double sghs = st.sgh2 * f2 + st.sgh3 * f3 + st.sgh4 * sinzf;
    const double shs = st.sh2 * f2 + st.sh3 * f3;

    const double zm_l = st.zmol + 1.5835218e-4 * t;
    zf = zm_l + 2.0 * 0.05490 * std::sin(zm_l);
    sinzf = std::sin(zf);
    f2 = 0.5 * sinzf * sinzf - 0.25;
    f3 = -0.5 * sinzf * std::cos(zf);
    const double sel = st.lun_ee2 * f2 + st.lun_e3 * f3;
    const double sil = st.lun_xi2 * f2 + st.lun_xi3 * f3;
    const double sll = st.lun_xl2 * f2 + st.lun_xl3 * f3 + st.lun_xl4 * sinzf;
    const double sghl = st.lun_xgh2 * f2 + st.lun_xgh3 * f3 + st.lun_xgh4 * sinzf;
    const double shl = st.lun_xh2 * f2 + st.lun_xh3 * f3;

    const double pe = ses + sel;
    const double pinc = sis + sil;
    const double pl = sls + sll;
    const double pgh = sghs + sghl;
    double ph = shs + shl;

    const double xinc = xinc_in + pinc;
    const double em = em_in + pe;
    double omgasm = omgasm_in;
    double xnodes = xnodes_in;
    double xll = xll_in;

    // CORRECTION, VAL06 §III seventh bullet: "The second difficulty with the
    // lunar-solar perturbations was the initialization of deep-space terms
    // based on perturbed values. This was corrected in the DPPER and SGP4
    // routines of the Dundee and GSFC versions. In STR#3, the terms computed
    // during initialization assumed fixed epoch values for inclination,
    // etc., but of course they are perturbed by the deep-space terms. The
    // approach used by the Dundee and GSFC versions includes any terms based
    // on the Keplerian orbit being re-computed based on the new perturbed
    // values." Here: STR3's DPPER takes SINIS/COSIS from the inclination
    // BEFORE `XINC=XINC+PINC` and, in its direct branch, SINIQ/COSIQ from the
    // fixed epoch inclination; both use the perturbed one instead. Evidence
    // that this is the reading: with this change alone (Lyddane branch;
    // PROVENANCE.md §38.6's own table) satellites 25954/26900/28626, whose
    // perturbed inclination is several times the epoch one, went from
    // 4.1/2.4/1.2 km out to 4.6/2.4/1.7 m, and 04632/14128 from 7.7/1.7 km
    // to 4.5/5.3 m (mm once the other corrections below were in).
    const double sinip = std::sin(xinc);
    const double cosip = std::cos(xinc);

    // The Lyddane choice, VAL06 §III fifth bullet and its "Option (b)": the
    // test is on the PERTURBED inclination at each call (not STR3's cached
    // epoch XQNCL). STR3's own branch sense (§10, labels 218/220): SMALL
    // inclination (< 0.2 rad) takes the LYDDANE/ACTAN route -- it exists
    // specifically to avoid the direct route's own PH/SINIQ divide-by-near-
    // zero there; LARGE inclination takes the direct route. "Perturbed"
    // includes the periodic PINC: tested both ways, the secular-only and the
    // epoch inclination put 04632 and 14128 (whose inclinations straddle
    // 0.2 rad) 7.7 km and 1.7 km out.
    if (xinc >= 0.2) {
        ph = ph / sinip;
        const double pgh_corr = pgh - cosip * ph;
        omgasm = omgasm + pgh_corr;
        xnodes = xnodes + ph;
        xll = xll + pl;
    } else {
        // CORRECTION, VAL06 §III fifth bullet: "The GSFC code (the IF
        // statements at the end of the 'apply periodics' section in DPPER)
        // confirmed the suspicions of several researchers about the need to
        // evaluate the relative quadrant of the resulting angle and to
        // correct accordingly. A similar problem exists with the modulo 2pi
        // reduction of the XNODE variable." and its "intrinsic functions"
        // bullet: "the MOD function modifies a variable that is then used
        // outside a trigonometric expression. In this case, we opted to
        // retain the modern MOD function, but simply add an IF statement to
        // check if the result is less than zero. The ATAN2 function a few
        // lines later may also require a similar modification". Read here as:
        // reduce XNODES into [0, 2*pi) FIRST (so the `COSIS*XNODES` and
        // `PINC*XNODES*SINIS` terms, which use it outside a trigonometric
        // expression, see the same node on both sides of ACTAN), take the
        // new node from ATAN2 into [0, 2*pi), then move it by +/-2*pi to the
        // representation nearest the reduced original. With the node left
        // unreduced (an earlier draft of this port) 16 rows of the battery
        // sat up to 0.96 km out; satellite 23599, whose node crosses 0/2*pi
        // at ~400 min, steps by ~0.9 km there.
        double xnoh = std::fmod(xnodes, kTwopi);
        if (xnoh < 0.0) xnoh += kTwopi;
        const double sinok = std::sin(xnoh);
        const double cosok = std::cos(xnoh);
        double alfdp = sinip * sinok;
        double betdp = sinip * cosok;
        const double dalf = ph * cosok + pinc * cosip * sinok;
        const double dbet = -ph * sinok + pinc * cosip * cosok;
        alfdp += dalf;
        betdp += dbet;
        double xls = xll + omgasm + cosip * xnoh;
        const double dls = pl + pgh - pinc * xnoh * sinip;
        xls += dls;
        double xnodes_new = std::atan2(alfdp, betdp);
        if (xnodes_new < 0.0) xnodes_new += kTwopi;
        if (std::abs(xnoh - xnodes_new) > kPi) {
            if (xnodes_new < xnoh) xnodes_new += kTwopi;
            else xnodes_new -= kTwopi;
        }
        xnodes = xnodes_new;
        xll = xll + pl;
        omgasm = xls - xll - cosip * xnodes;  // cos of the PERTURBED xinc, as STR3's own final line
    }

    return {em, xinc, omgasm, xnodes, xll};
}

}  // namespace

namespace {

/// The inclination-dependent coefficients the shared tail uses. Near-earth:
/// STR3's own fixed epoch values. Deep space: re-computed from the perturbed
/// inclination (VAL06 §III seventh bullet, "in the DPPER and SGP4 routines").
struct TailCoeffs {
    double cosio, sinio, x3thm1, x1mth2, x7thm1, xlcof, aycof;
};

/// STR3 §6/§7's SHARED tail, identical text in both: long-period periodics,
/// Kepler's equation, short-period periodics, orientation vectors,
/// position/velocity -- from `AXN=E*COS(OMEGA)` (or `OMGADF`, SDP4's own
/// name for the same slot) onward. `incl_base` is `XINCL` for near-earth
/// (fixed) or DPPER's own perturbed `XINC` for deep-space; `k` carries the
/// coefficients STR3's own text takes from the fixed epoch inclination in
/// both SGP4 and SDP4 (its `XINCK` correction term, `X3THM1`, `X1MTH2`,
/// `X7THM1`, `XLCOF`, `AYCOF`).
///
/// CORRECTIONS to the Kepler solve (VAL06 §III third bullet, §VI.E): the
/// tolerance is 1e-12 not STR3's 1e-6, and each Newton correction is limited
/// to +/-0.95 -- see `kKeplerTol` and `kKeplerStepLimit` for what each fixes
/// and what was tried first. The 10-iteration budget is STR3's own.
///
/// Refuses IOSG-F-002 ("decayed") when the computed radius drops below one
/// Earth radius -- VAL06 §VI.A: "the decay condition simply checks the
/// position magnitude on each step." Refuses IOSG-F-005 when the semi-latus
/// rectum is not positive -- the verification file's own comment on
/// satellite 33333 ("check error code 4"): its published rows stop at 20 min
/// and this port's own `pl` goes negative between 20 and 21.
odl::Result<Sgp4RawState, Sgp4Error> final_position_velocity(
    const TailCoeffs& k, double a, double e, double xl, double xnode, double omega, double incl_base) {
    const double beta = std::sqrt(1.0 - e * e);
    const double xn = kXke / std::pow(a, 1.5);

    const double axn = e * std::cos(omega);
    const double temp0 = 1.0 / (a * beta * beta);
    const double xll = temp0 * k.xlcof * axn;
    const double aynl = temp0 * k.aycof;
    const double xlt = xl + xll;
    const double ayn = e * std::sin(omega) + aynl;

    const double capu = fmod2p(xlt - xnode);
    double temp2 = capu;
    double sinepw = 0.0, cosepw = 0.0, temp3 = 0.0, temp4 = 0.0, temp5 = 0.0, temp6 = 0.0;
    for (int i = 0; i < 10; ++i) {
        sinepw = std::sin(temp2);
        cosepw = std::cos(temp2);
        temp3 = axn * sinepw;
        temp4 = ayn * cosepw;
        temp5 = axn * cosepw;
        temp6 = ayn * sinepw;
        double delta = (capu - temp4 + temp3 - temp2) / (1.0 - temp5 - temp6);
        if (delta > kKeplerStepLimit) delta = kKeplerStepLimit;
        if (delta < -kKeplerStepLimit) delta = -kKeplerStepLimit;
        const double epw = temp2 + delta;
        const bool converged = std::abs(epw - temp2) <= kKeplerTol;
        temp2 = epw;
        if (converged) break;
    }

    const double ecose = temp5 + temp6;
    const double esine = temp3 - temp4;
    const double elsq = axn * axn + ayn * ayn;
    const double temp_1mel = 1.0 - elsq;
    const double pl = a * temp_1mel;
    if (!(pl > 0.0)) {
        return odl::err(Sgp4Error{"IOSG-F-005", "semi-latus rectum not positive"});
    }
    const double r = a * (1.0 - ecose);
    const double rinv = 1.0 / r;
    const double rdot = kXke * std::sqrt(a) * esine * rinv;
    const double rfdot = kXke * std::sqrt(pl) * rinv;
    const double temp2b = a * rinv;
    const double betal = std::sqrt(temp_1mel);
    const double temp3b = 1.0 / (1.0 + betal);
    const double cosu = temp2b * (cosepw - axn + ayn * esine * temp3b);
    const double sinu = temp2b * (sinepw - ayn - axn * esine * temp3b);
    const double u = actan(sinu, cosu);
    const double sin2u = 2.0 * sinu * cosu;
    const double cos2u = 2.0 * cosu * cosu - 1.0;
    const double templ = 1.0 / pl;
    const double temp1c = kCk2 * templ;
    const double temp2c = temp1c * templ;

    const double rk = r * (1.0 - 1.5 * temp2c * betal * k.x3thm1) + 0.5 * temp1c * k.x1mth2 * cos2u;
    if (rk < 1.0) {
        return odl::err(Sgp4Error{"IOSG-F-002", "decayed: computed radius below one Earth radius"});
    }
    const double uk = u - 0.25 * temp2c * k.x7thm1 * sin2u;
    const double xnodek = xnode + 1.5 * temp2c * k.cosio * sin2u;
    const double xinck = incl_base + 1.5 * temp2c * k.cosio * k.sinio * cos2u;
    const double rdotk = rdot - xn * temp1c * k.x1mth2 * sin2u;
    const double rfdotk = rfdot + xn * temp1c * (k.x1mth2 * cos2u + 1.5 * k.x3thm1);

    const double sinuk = std::sin(uk), cosuk = std::cos(uk);
    const double sinik = std::sin(xinck), cosik = std::cos(xinck);
    const double sinnok = std::sin(xnodek), cosnok = std::cos(xnodek);
    const double xmx = -sinnok * cosik, xmy = cosnok * cosik;
    const double ux = xmx * sinuk + cosnok * cosuk;
    const double uy = xmy * sinuk + sinnok * cosuk;
    const double uz = sinik * sinuk;
    const double vx = xmx * cosuk - cosnok * sinuk;
    const double vy = xmy * cosuk - sinnok * sinuk;
    const double vz = sinik * cosuk;

    // STR3 §11's own final AE->km, AE/min->km/s conversion (AE=1 throughout,
    // so the AE factors are algebraically absent here).
    Sgp4RawState raw;
    raw.x_km = rk * ux * kXkmper;
    raw.y_km = rk * uy * kXkmper;
    raw.z_km = rk * uz * kXkmper;
    const double v_scale = kXkmper * kXmnpda / 86400.0;
    raw.xdot_km_s = (rdotk * ux + rfdotk * vx) * v_scale;
    raw.ydot_km_s = (rdotk * uy + rfdotk * vy) * v_scale;
    raw.zdot_km_s = (rdotk * uz + rfdotk * vz) * v_scale;
    return raw;
}

/// The drag-modified mean eccentricity, `E=EO-TEMPE` (SGP4) / `E=EM-TEMPE`
/// (SDP4), with the two behaviours STR3 does not have -- both fixed by the
/// constants' own comments above: refuse IOSG-F-003 ("modified eccentricity
/// too low", VAL06 Table 1) below `kEccTrap`; hold it at `kEccFloor` between.
odl::Result<double, Sgp4Error> drag_modified_eccentricity(double e) {
    if (e < kEccTrap) {
        return odl::err(Sgp4Error{"IOSG-F-003", "modified eccentricity too low"});
    }
    return e < kEccFloor ? kEccFloor : e;
}

}  // namespace

odl::Result<Sgp4RawState, Sgp4Error> sgp4_propagate(const Sgp4InitialState& st, double tsince_minutes) {
    const double t = tsince_minutes;

    if (!st.deep_space) {
        // -- STR3 §6's own secular update --
        const double xmdf = st.xmo_saved + st.xmdot * t;
        const double omgadf = st.omegao_saved + st.omgdot * t;
        const double xnoddf = st.xnodeo_saved + st.xnodot * t;
        double omega = omgadf;
        double xmp = xmdf;
        const double tsq = t * t;
        const double xnode = xnoddf + st.xnodcf * tsq;
        double tempa = 1.0 - st.c1 * t;
        double tempe = st.bstar * st.c4 * t;
        double templ = st.t2cof * tsq;
        if (!st.isimp) {
            const double delomg = st.omgcof * t;
            const double delm = st.xmcof * (std::pow(1.0 + st.eta * std::cos(xmdf), 3) - st.delmo);
            const double temp = delomg + delm;
            xmp = xmdf + temp;
            omega = omgadf - temp;
            const double tcube = tsq * t;
            const double tfour = t * tcube;
            tempa = tempa - st.d2 * tsq - st.d3 * tcube - st.d4 * tfour;
            tempe = tempe + st.bstar * st.c5 * (std::sin(xmp) - st.sinmo);
            templ = templ + st.t3cof * tcube + tfour * (st.t4cof + t * st.t5cof);
        }
        const double a = st.aodp * tempa * tempa;
        const auto e = drag_modified_eccentricity(st.eo - tempe);
        if (!e) return odl::err(e.error());
        const double xl = xmp + omega + xnode + st.xnodp * templ;
        const TailCoeffs k{st.cosio, st.sinio, st.x3thm1, st.x1mth2, st.x7thm1, st.xlcof, st.aycof};
        return final_position_velocity(k, a, *e, xl, xnode, omega, st.xincl_saved);
    }

    // -- STR3 §7's own secular update, through DPSEC/DPPER --
    const double xmdf = st.xmo_saved + st.xmdot * t;
    const double omgadf = st.omegao_saved + st.omgdot * t;
    const double xnoddf = st.xnodeo_saved + st.xnodot * t;
    const double tsq = t * t;
    const double xnode0 = xnoddf + st.xnodcf * tsq;
    const double tempa = 1.0 - st.c1 * t;
    const double tempe = st.bstar * st.c4 * t;
    const double templ = st.t2cof * tsq;

    const DpsecResult sec = dpsec(st, xmdf, omgadf, xnode0, t);
    const double a = std::pow(kXke / sec.xn, kTothrd) * tempa * tempa;
    const auto e0 = drag_modified_eccentricity(sec.em - tempe);
    if (!e0) return odl::err(e0.error());
    const double xmam = sec.xll + st.xnodp * templ;

    const DpperResult per = dpper(st, *e0, sec.xinc, sec.omgasm, sec.xnodes, xmam, t);

    // IOSG-F-004: the periodics-perturbed eccentricity left [0, 1). The
    // verification file's own comment on satellite 33334 ("try to check error
    // code 3 looks like ep never goes below zero, tied close to ecc"): at
    // n = 1e-5 rev/day the lunar-solar periodics, which scale with 1/n, drive
    // it to -122 here. No published row can be matched -- 33334.e's one row is
    // 33333.e's last row copied verbatim (checked to all 8 decimals), i.e. what
    // the reference run printed after the case failed.
    if (per.em < 0.0 || per.em >= 1.0) {
        return odl::err(Sgp4Error{"IOSG-F-004", "perturbed eccentricity outside [0, 1)"});
    }

    // CORRECTION, VAL06 §VI.C (see `dpsec`'s own note): the negative-
    // inclination swap, here, on the fully perturbed inclination, "before the
    // 'long period periodics' section".
    double xinc = per.xinc;
    double xnodes = per.xnodes;
    double omgasm = per.omgasm;
    if (xinc < 0.0) {
        xinc = -xinc;
        xnodes += kPi;
        omgasm -= kPi;
    }
    // VAL06 §VI.A's tolerance "in both routines": the perturbed inclination
    // can reach 180 degrees in operation as well as at epoch.
    if (std::abs(kPi - xinc) < kInclTolRad) {
        return odl::err(Sgp4Error{"IOSG-F-001", "inclination within 0.086 deg of 180 deg"});
    }

    // CORRECTION, VAL06 §III seventh bullet: the coefficients STR3's own SDP4
    // takes from the fixed epoch inclination are re-computed from the
    // perturbed one ("in the DPPER and SGP4 routines").
    const double cosip = std::cos(xinc);
    const double sinip = std::sin(xinc);
    const double th2 = cosip * cosip;
    const TailCoeffs k{cosip,
                       sinip,
                       3.0 * th2 - 1.0,
                       1.0 - th2,
                       7.0 * th2 - 1.0,
                       0.125 * st.a3ovk2 * sinip * (3.0 + 5.0 * cosip) / (1.0 + cosip),
                       0.25 * st.a3ovk2 * sinip};
    const double xl = per.xll + omgasm + xnodes;
    return final_position_velocity(k, a, per.em, xl, xnodes, omgasm, xinc);
}

odl::Result<frames::TemeState, Sgp4Error> propagate_teme(
    const Tle& tle, const time::Epoch& target, const time::LeapTable& leaps) {
    const auto init = sgp4_init(tle);
    if (!init) return odl::err(init.error());

    const time::Calendar tle_cal = tle_epoch_calendar(tle);
    const auto target_cal = target.calendar(time::TimeScale::UTC, leaps);
    if (!target_cal) return odl::err(target_cal.error());

    const double tsince_minutes = calendar_elapsed_seconds(tle_cal, *target_cal) / 60.0;
    const auto raw = sgp4_propagate(*init, tsince_minutes);
    if (!raw) return odl::err(raw.error());

    return frames::TemeState{
        target,
        odl::Vec3{raw->x_km, raw->y_km, raw->z_km},
        odl::Vec3{raw->xdot_km_s, raw->ydot_km_s, raw->zdot_km_s},
    };
}

}  // namespace odl::io
