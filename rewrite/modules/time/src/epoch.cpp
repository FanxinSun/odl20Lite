#include <odl/time/epoch.hpp>

extern "C" {
#include <erfa.h>
}

#include <cmath>
#include <string>

namespace odl::time {
namespace {

constexpr double kSecondsPerDay = 86400.0;
constexpr double kMjdToJd       = 2400000.5;

// TT − TAI, split so that the addition is exact in the integer part (TIME-R-020).
constexpr std::int64_t kTtMinusTaiWhole = 32;
constexpr double       kTtMinusTaiFrac  = 0.184;

// GPS.  ERFA declares no GPS routines at all (TIME-R-024), so this is ours.
// Derived in SPEC-time §4.4 from the IERS table: ΔAT = 19 s at 1980-01-01, and
// GPS was aligned with UTC at 1980-01-06T00:00:00 UTC.
constexpr std::int64_t kTaiMinusGps = 19;

// IERS Conventions (2010) TN36 Table 1.1 and eq. (10.1), (10.3).
constexpr double kLg   = 6.969290134e-10;
constexpr double kLb   = 1.550519768e-8;
constexpr double kTdb0 = -6.55e-5;
constexpr double kT0   = 2443144.5003725;   // JD, 1977-01-01T00:00:00 TAI

struct Parts {
    std::int64_t whole;
    double       frac;
};

Parts normalise(std::int64_t whole, double frac) noexcept {
    const double k = std::floor(frac);
    return Parts{whole + static_cast<std::int64_t>(k), frac - k};
}

/// The pair ERFA wants: integral Julian day, then the day fraction (TIME-R-014).
///
/// THE DAY COUNT AND THE FRACTION ARE NEVER ADDED TOGETHER.  An earlier version
/// of this function formed the whole Julian date as one double and then split
/// it, which quantises at 2^-31 d = 40 µs near JD 2.46e6 — the exact failure
/// SPEC-time §4.2 disqualifies a bare f64 Julian Date for, reintroduced inside
/// the module built to avoid it.  It cost 1.9e-5 s on TCB − TDB and was caught
/// by comparing two routes to the same published quantity.
TwoPartJd two_part_from_mjd(std::int64_t mjd, double day_fraction) noexcept {
    // JD = MJD + 2400000.5, so the integral Julian day is MJD + 2400000 and
    // there is a half day left over to carry into the fraction.
    std::int64_t whole = mjd + 2400000;
    double frac = 0.5 + day_fraction;
    if (frac >= 1.0) { frac -= 1.0; whole += 1; }
    return TwoPartJd{static_cast<double>(whole), frac};
}

TwoPartJd jd_from_linear(std::int64_t seconds, double fraction) noexcept {
    std::int64_t days = seconds / 86400;
    std::int64_t rem  = seconds - days * 86400;
    if (rem < 0) { --days; rem += 86400; }
    return two_part_from_mjd(Epoch::kOriginMjd + days,
                             (static_cast<double>(rem) + fraction) / kSecondsPerDay);
}

}  // namespace

Epoch Epoch::from_tai_parts(std::int64_t whole, double fraction) noexcept {
    const Parts p = normalise(whole, fraction);
    return Epoch{p.whole, p.frac};
}

Epoch Epoch::add(Duration d) const noexcept {
    return from_tai_parts(seconds_ + d.whole_seconds(), fraction_ + d.fraction());
}

Duration Epoch::difference(const Epoch& earlier) const noexcept {
    return Duration::from_parts(seconds_ - earlier.seconds_, fraction_ - earlier.fraction_);
}

// --------------------------------------------------------------------------- //
// TDB − TT, and the barycentric scales.

Duration Epoch::tdb_minus_tt(std::optional<Site> site) const {
    const Epoch tt = add(Duration::from_parts(kTtMinusTaiWhole, kTtMinusTaiFrac));
    const TwoPartJd j = jd_from_linear(tt.seconds_, tt.fraction_);
    // eraDtdb wants UT1 as a fraction of a day.  Only the diurnal term uses it,
    // and that term reaches ~2.1 µs, so an error of up to 0.9 s in UT1 moves the
    // result by under 0.2 ns.  UTC's fraction of a day is therefore an adequate
    // stand-in and no EOP is needed — see the report's note on SPEC-time §5,
    // whose signature does not provide ΔUT1.
    const double ut = j.fraction;
    return Duration::from_seconds(eraDtdb(j.day, j.fraction, ut,
                                          site ? site->east_longitude_rad : 0.0,
                                          site ? site->u_km : 0.0,
                                          site ? site->v_km : 0.0));
}

namespace {

/// scale − TAI, for the uniform scales only.  UTC is not here: it is not a
/// constant offset and is handled by the leap table.
Duration uniform_offset(TimeScale scale, std::int64_t tai_sec, double tai_frac,
                        const Epoch& self) {
    const Duration tt_off = Duration::from_parts(kTtMinusTaiWhole, kTtMinusTaiFrac);
    switch (scale) {
        case TimeScale::TAI: return Duration::zero();
        case TimeScale::TT:  return tt_off;
        case TimeScale::GPS: return Duration::from_parts(-kTaiMinusGps, 0.0);
        case TimeScale::TCG: {
            const auto p = normalise(tai_sec + kTtMinusTaiWhole, tai_frac + kTtMinusTaiFrac);
            const TwoPartJd j = jd_from_linear(p.whole, p.frac);
            const double tcg_minus_tt =
                (kLg / (1.0 - kLg)) * ((j.day - kT0) + j.fraction) * kSecondsPerDay;
            return tt_off + Duration::from_seconds(tcg_minus_tt);
        }
        case TimeScale::TDB: return tt_off + self.tdb_minus_tt();
        case TimeScale::TCB: {
            const Duration tdb_off = tt_off + self.tdb_minus_tt();
            const auto p = normalise(tai_sec + tdb_off.whole_seconds(),
                                     tai_frac + tdb_off.fraction());
            // TCB − TDB = L_B (JD_TCB − T0) 86400 − TDB0, implicit in JD_TCB.
            // Two fixed-point steps: the first residual is ~0.4 µs, the second
            // ~6e-15 s.  One step would sit right on the module's budget.
            double corr = 0.0;
            for (int i = 0; i < 2; ++i) {
                const auto q = normalise(p.whole, p.frac + corr);
                const TwoPartJd j = jd_from_linear(q.whole, q.frac);
                corr = kLb * ((j.day - kT0) + j.fraction) * kSecondsPerDay - kTdb0;
            }
            return tdb_off + Duration::from_seconds(corr);
        }
        case TimeScale::UTC: return Duration::zero();   // unreachable; see callers
    }
    return Duration::zero();
}

}  // namespace

// --------------------------------------------------------------------------- //
// UTC.

odl::Result<Epoch, TimeError> Epoch::from_calendar(TimeScale scale, const Calendar& c,
                                                    const LeapTable& leaps) {
    if (c.month < 1 || c.month > 12 || c.day < 1 || c.day > 31 || c.hour < 0 || c.hour > 23 ||
        c.minute < 0 || c.minute > 59 || c.second < 0.0 || c.second >= 61.0) {
        return odl::err("TIME-F-007",
                        "calendar field out of range: " + std::to_string(c.year) + "-" +
                        std::to_string(c.month) + "-" + std::to_string(c.day) + "T" +
                        std::to_string(c.hour) + ":" + std::to_string(c.minute) + ":" +
                        std::to_string(c.second) + " in " + std::string(name_of(scale)) +
                        ". Valid: month 1-12, day 1-31, hour 0-23, minute 0-59, "
                        "second [0,61). Fields are not normalised by carrying.");
    }
    double djm0 = 0.0, djm = 0.0;
    if (eraCal2jd(c.year, c.month, c.day, &djm0, &djm) != 0) {
        return odl::err("TIME-F-007",
                        "calendar date " + std::to_string(c.year) + "-" +
                        std::to_string(c.month) + "-" + std::to_string(c.day) +
                        " is not a valid Gregorian date (ERFA eraCal2jd refused it)");
    }
    const auto mjd = static_cast<std::int64_t>(std::llround(djm));
    const double sod = static_cast<double>(c.hour) * 3600.0 +
                       static_cast<double>(c.minute) * 60.0 + c.second;

    if (scale == TimeScale::UTC) {
        auto dat = leaps.delta_at_on(mjd);
        if (!dat.has_value()) return odl::err(dat.error());
        const std::int64_t leap = leaps.leap_at_end_of(mjd);
        if (c.second >= 60.0 && !(leap == 1 && c.hour == 23 && c.minute == 59)) {
            return odl::err("TIME-F-003",
                            "seconds field " + std::to_string(c.second) + " on " +
                            std::to_string(c.year) + "-" + std::to_string(c.month) + "-" +
                            std::to_string(c.day) + " (MJD " + std::to_string(mjd) +
                            "): that day does not end in a positive leap second. dAT in force is "
                            + std::to_string(*dat) + " s and does not change at the end of the "
                            "day. A 61st second exists only in the last minute of a day the "
                            "IERS table steps at.");
        }
        const double whole_sod = std::floor(sod);
        return from_tai_parts((mjd - kOriginMjd) * 86400 +
                              static_cast<std::int64_t>(whole_sod) + *dat,
                              sod - whole_sod);
    }

    const double whole_sod = std::floor(sod);
    const Epoch naive = from_tai_parts((mjd - kOriginMjd) * 86400 +
                                       static_cast<std::int64_t>(whole_sod), sod - whole_sod);

    // Inverting scale = TAI + offset(TAI) needs a fixed point, because for TCG,
    // TDB and TCB the offset depends on the epoch.  Evaluating it once at the
    // naive epoch is NOT good enough: TCB − TAI is about 56 s and TCB advances
    // on TDB at L_B = 1.55e-8 s/s, so a single evaluation is wrong by
    // 1.55e-8 × 56 ≈ 0.9 µs — measured as 1.15 µs across all scales by
    // TIME-A-008, against a budget of 1 ns.  (An earlier comment here asserted
    // femtoseconds; that was an arithmetic slip, and the test caught it.)
    // Each iteration multiplies the residual by the rate, so two steps leave
    // about 1e-14 s.
    Epoch guess = naive;
    for (int i = 0; i < 3; ++i) {
        const Duration off = uniform_offset(scale, guess.seconds_, guess.fraction_, guess);
        guess = naive.add(-off);
    }
    return guess;
}

odl::Result<Calendar, TimeError> Epoch::calendar(TimeScale scale, const LeapTable& leaps,
                                                  int decimals) const {
    std::int64_t lin_sec = 0;
    double lin_frac = fraction_;
    bool inside_leap = false;

    if (scale == TimeScale::UTC) {
        const auto& es = leaps.entries();
        std::int64_t dat = es.front().delta_at;
        std::size_t idx = 0;
        for (std::size_t i = 0; i < es.size(); ++i) {
            const std::int64_t step = (es[i].mjd_utc - kOriginMjd) * 86400 + es[i].delta_at;
            if (seconds_ >= step) { dat = es[i].delta_at; idx = i; } else { break; }
        }
        // Inside an inserted leap second the next step has not been reached but
        // the previous ΔAT still applies, and the correct rendering is 23:59:60
        // of the preceding day, not 00:00:00 of the next one.
        if (idx + 1 < es.size()) {
            const auto& nx = es[idx + 1];
            const std::int64_t step = (nx.mjd_utc - kOriginMjd) * 86400 + nx.delta_at;
            const std::int64_t inserted = nx.delta_at - dat;
            inside_leap = inserted > 0 && seconds_ >= step - inserted && seconds_ < step;
        }
        lin_sec = seconds_ - dat;
    } else {
        const Duration off = uniform_offset(scale, seconds_, fraction_, *this);
        const auto p = normalise(seconds_ + off.whole_seconds(), fraction_ + off.fraction());
        lin_sec = p.whole;
        lin_frac = p.frac;
    }

    std::int64_t days = lin_sec / 86400;
    std::int64_t rem  = lin_sec - days * 86400;
    if (rem < 0) { --days; rem += 86400; }
    if (inside_leap) { --days; rem += 86400; }

    if (scale == TimeScale::UTC) {
        // Enforce the range policy on output as well as input: TIME-F-002 before
        // 1972 and TIME-F-004 past the table's expiry.
        auto guard = leaps.delta_at_on(kOriginMjd + days);
        if (!guard.has_value()) return odl::err(guard.error());
    }

    int iy = 0, im = 0, id = 0;
    double fd = 0.0;
    // (2400000.5, MJD) rather than a single summed Julian date: the day count is
    // integral in the second argument and nothing is quantised.  Only y/m/d is
    // taken from this call, but the pattern matters more than this use of it.
    if (eraJd2cal(kMjdToJd, static_cast<double>(kOriginMjd + days),
                  &iy, &im, &id, &fd) != 0) {
        return odl::err("TIME-F-007",
                        "epoch is outside the range ERFA's calendar conversion supports");
    }

    Calendar c;
    c.year = iy; c.month = im; c.day = id;
    if (rem >= 86400) {                      // the inserted second: 23:59:60
        c.hour = 23; c.minute = 59;
        c.second = static_cast<double>(rem - 86340) + lin_frac;
    } else {
        c.hour   = static_cast<int>(rem / 3600);
        c.minute = static_cast<int>((rem % 3600) / 60);
        c.second = static_cast<double>(rem % 60) + lin_frac;
    }

    // TIME-R-033: rounding for display must not invent a 61st second on a day
    // without one, nor a 24th hour on any day.  A carry that would do either is
    // clamped to the largest representable value below the boundary.
    if (decimals >= 0 && decimals < 17) {
        const double f = std::pow(10.0, decimals);
        const double rounded = std::round(c.second * f) / f;
        const double limit = inside_leap ? 61.0 : 60.0;
        c.second = (rounded >= limit) ? std::nextafter(limit, 0.0) : rounded;
    }
    return c;
}

odl::Result<Duration, TimeError> Epoch::delta_at(const LeapTable& leaps) const {
    auto cal = calendar(TimeScale::UTC, leaps);
    if (!cal.has_value()) return odl::err(cal.error());
    double djm0 = 0.0, djm = 0.0;
    eraCal2jd(cal->year, cal->month, cal->day, &djm0, &djm);
    auto dat = leaps.delta_at_on(static_cast<std::int64_t>(std::llround(djm)));
    if (!dat.has_value()) return odl::err(dat.error());
    return Duration::from_parts(*dat, 0.0);
}

// --------------------------------------------------------------------------- //

odl::Result<TwoPartJd, TimeError> Epoch::two_part_jd(TimeScale scale,
                                                      const LeapTable& leaps) const {
    if (scale == TimeScale::UTC) {
        // ERFA's quasi-JD convention: the JD day represents a UTC day whether it
        // is 86399, 86400 or 86401 SI seconds long, so the fraction is scaled by
        // the day's actual length and stays below 1 even across a leap second.
        auto cal = calendar(scale, leaps, -1);
        if (!cal.has_value()) return odl::err(cal.error());
        double djm0 = 0.0, djm = 0.0;
        eraCal2jd(cal->year, cal->month, cal->day, &djm0, &djm);
        const auto mjd = static_cast<std::int64_t>(std::llround(djm));
        const double day_len = 86400.0 + static_cast<double>(leaps.leap_at_end_of(mjd));
        const double sod = static_cast<double>(cal->hour) * 3600.0 +
                           static_cast<double>(cal->minute) * 60.0 + cal->second;
        return two_part_from_mjd(mjd, sod / day_len);
    }
    const Duration off = uniform_offset(scale, seconds_, fraction_, *this);
    const auto p = normalise(seconds_ + off.whole_seconds(), fraction_ + off.fraction());
    return jd_from_linear(p.whole, p.frac);
}

odl::Result<Epoch, TimeError> Epoch::from_two_part_jd(TimeScale scale, double part1,
                                                       double part2, const LeapTable& leaps) {
    int iy = 0, im = 0, id = 0;
    double fd = 0.0;
    if (eraJd2cal(part1, part2, &iy, &im, &id, &fd) != 0) {
        return odl::err("TIME-F-007",
                        "two-part Julian date (" + std::to_string(part1) + ", " +
                        std::to_string(part2) + ") in " + std::string(name_of(scale)) +
                        " is outside the range ERFA's calendar conversion supports");
    }
    double day_len = 86400.0;
    if (scale == TimeScale::UTC) {
        double djm0 = 0.0, djm = 0.0;
        eraCal2jd(iy, im, id, &djm0, &djm);
        day_len += static_cast<double>(leaps.leap_at_end_of(
            static_cast<std::int64_t>(std::llround(djm))));
    }
    const double sod = fd * day_len;
    Calendar c;
    c.year = iy; c.month = im; c.day = id;
    c.hour   = static_cast<int>(sod / 3600.0);
    c.minute = static_cast<int>((sod - static_cast<double>(c.hour) * 3600.0) / 60.0);
    c.second = sod - static_cast<double>(c.hour) * 3600.0 - static_cast<double>(c.minute) * 60.0;
    return from_calendar(scale, c, leaps);
}

odl::Result<Epoch, TimeError> Epoch::from_gps_week(std::int64_t week, double seconds_of_week) {
    if (week < 1024) {
        return odl::err("TIME-F-006",
                        "GPS week " + std::to_string(week) + " is ambiguous: a bare 10-bit week "
                        "repeats every 1024 weeks (19.6 years), so this could be any of several "
                        "epochs. Supply an unambiguous extended week number. Guessing the "
                        "rollover nearest today is how an epoch silently lands two decades away.");
    }
    // GPS epoch 1980-01-06T00:00:00 UTC = MJD 44244, at which dAT was 19 s.
    constexpr std::int64_t kGpsEpochMjd = 44244;
    const double whole = std::floor(seconds_of_week);
    return from_tai_parts((kGpsEpochMjd - kOriginMjd) * 86400 + week * 604800 +
                          static_cast<std::int64_t>(whole) + kTaiMinusGps,
                          seconds_of_week - whole);
}

odl::Result<TwoPartJd, TimeError> Epoch::ut1_two_part_jd(Duration dut1,
                                                          const LeapTable& leaps) const {
    // UT1 = UTC + ΔUT1, and UTC = TAI − ΔAT.  ΔUT1 is published by the IERS
    // against UTC, so the leap table is not avoidable here — an earlier draft of
    // this function added ΔUT1 straight to TAI, which is UT1 only if ΔAT is zero
    // and was wrong by 37 seconds.
    auto dat = delta_at(leaps);
    if (!dat.has_value()) return odl::err(dat.error());
    const auto p = normalise(seconds_ - dat->whole_seconds() + dut1.whole_seconds(),
                             fraction_ - dat->fraction() + dut1.fraction());
    return jd_from_linear(p.whole, p.frac);
}

}  // namespace odl::time
