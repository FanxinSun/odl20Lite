#include "tides.hpp"
#include "tide_tables.hpp"

extern "C" {
#include <erfa.h>
}

#include <cmath>

namespace odl::eop::tides {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kMicroArcsecToRad = kPi / (180.0 * 3600.0 * 1.0e6);
constexpr double kMicroSecToSec    = 1.0e-6;
constexpr double kMjdToJd          = 2400000.5;

/// TN36 §5.5.1.1: only the NEAR-DIURNAL rows of Table 5.1a are added.  "The
/// long-period terms, as well as the secular variation of the libration
/// contribution, are already contained in the observed polar motion", so adding
/// the whole table double-counts them.  The ten rows with periods near a day are
/// exactly the ten the IERS routine PMSDNUT2 uses.
constexpr bool is_near_diurnal(double period_days) noexcept {
    return period_days > 0.9 && period_days < 1.2;
}

struct Sum { double a = 0.0, b = 0.0; };

template <std::size_t N>
Sum accumulate(const TideTerm (&table)[N], const Arguments& args) noexcept {
    Sum s;
    const double arg[6] = {args.gamma, args.l, args.lp, args.F, args.D, args.Om};
    for (const TideTerm& t : table) {
        double theta = 0.0;
        for (std::size_t i = 0; i < 6; ++i) theta += static_cast<double>(t.arg[i]) * arg[i];
        const double st = std::sin(theta), ct = std::cos(theta);
        s.a += t.sin1 * st + t.cos1 * ct;
        s.b += t.sin2 * st + t.cos2 * ct;
    }
    return s;
}

}  // namespace

Arguments arguments_at(double tt1, double tt2, double ut1_1, double ut1_2) noexcept {
    const double t = ((tt1 - 2451545.0) + tt2) / 36525.0;
    Arguments a;
    a.gamma = eraGmst06(ut1_1, ut1_2, tt1, tt2) + kPi;
    a.l     = eraFal03(t);
    a.lp    = eraFalp03(t);
    a.F     = eraFaf03(t);
    a.D     = eraFad03(t);
    a.Om    = eraFaom03(t);
    return a;
}

Arguments arguments_at_mjd(double mjd) noexcept {
    const double whole = std::floor(mjd);
    const double frac = mjd - whole;
    const double jd1 = whole + kMjdToJd;
    return arguments_at(jd1, frac, jd1, frac);
}

Correction ocean_tides(const Arguments& a) noexcept {
    const Sum pd = accumulate(kPoleOceanDiurnal, a);
    const Sum ps = accumulate(kPoleOceanSemidiurnal, a);
    const Sum ud = accumulate(kUt1OceanDiurnal, a);
    const Sum us = accumulate(kUt1OceanSemidiurnal, a);
    return Correction{(pd.a + ps.a) * kMicroArcsecToRad,
                      (pd.b + ps.b) * kMicroArcsecToRad,
                      (ud.a + us.a) * kMicroSecToSec,
                      (ud.b + us.b) * kMicroSecToSec};
}

Correction libration(const Arguments& a) noexcept {
    const double arg[6] = {a.gamma, a.l, a.lp, a.F, a.D, a.Om};
    Sum pole;
    for (const TideTerm& t : kPoleLibration) {
        if (!is_near_diurnal(t.period_days)) continue;
        double theta = 0.0;
        for (std::size_t k = 0; k < 6; ++k) theta += static_cast<double>(t.arg[k]) * arg[k];
        const double st = std::sin(theta), ct = std::cos(theta);
        pole.a += t.sin1 * st + t.cos1 * ct;
        pole.b += t.sin2 * st + t.cos2 * ct;
    }
    const Sum ut = accumulate(kUt1Libration, a);
    return Correction{pole.a * kMicroArcsecToRad, pole.b * kMicroArcsecToRad,
                      ut.a * kMicroSecToSec, ut.b * kMicroSecToSec};
}

}  // namespace odl::eop::tides
