#include <odl/eop/series.hpp>

#include "tides.hpp"

#include <algorithm>
#include <cmath>
#include <charconv>
#include <sstream>
#include <string>
#include <vector>

namespace odl::eop {
namespace {

using odl::time::Epoch;
using odl::time::LeapTable;

constexpr double kPi = 3.14159265358979323846;
constexpr double kArcsecToRad = kPi / (180.0 * 3600.0);
constexpr double kMasToRad    = kArcsecToRad * 1.0e-3;
constexpr double kDay         = 86400.0;

/// EOP-R-011.  Ranges for the historical era, checked BEFORE conversion, so that
/// a unit confusion between the two products trips immediately: finals2000A
/// gives dX, dY in milliarcseconds and LOD in milliseconds where C04 gives
/// arcseconds and seconds, and a factor of a thousand in dX lands far outside.
struct Limits {
    static constexpr double kPoleArcsec = 1.0;
    static constexpr double kLodSeconds = 1.0e-2;
    static constexpr double kCipMas     = 100.0;
};

std::string where(const EopProvenance& p, std::size_t line) {
    return p.product + " file " + p.source_id + ": line " + std::to_string(line);
}

odl::Result<std::int64_t, EopError> tai_at_0h(std::int64_t mjd, const LeapTable& leaps,
                                               const EopProvenance& prov) {
    auto dat = leaps.delta_at_on(mjd);
    if (!dat.has_value()) {
        return odl::err(EopError{"EOP-F-008",
            "EOP row at MJD " + std::to_string(mjd) + " in " + prov.source_id +
            " cannot be placed on a continuous scale: " + dat.error().message});
    }
    return (mjd - Epoch::kOriginMjd) * 86400 + *dat;
}

}  // namespace

// --------------------------------------------------------------------------- //

odl::Result<EopSeries, EopError> EopSeries::load_c04(std::string_view text,
                                                      EopProvenance provenance,
                                                      const LeapTable& leaps) {
    EopSeries s;
    provenance.product = "EOP 20 C04";
    std::istringstream in{std::string(text)};
    std::string line;
    std::size_t line_no = 0;
    // The file's own sequence, tracked separately from the rows kept.  Checking
    // contiguity against the STORED rows breaks the moment any row is skipped —
    // and rows are skipped at both ends, before 1972 and past the leap table's
    // expiry — which reported a phantom gap.
    std::int64_t last_file_mjd = 0;

    while (std::getline(in, line)) {
        ++line_no;
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) continue;
        if (line[first] == '#') {
            // EOP-R-023: record the file's own model line AS FOUND.  The C04 data
            // file says "IAU 2000" while the series readme says "dX, dY IAU
            // 2006/2000A"; the product contradicts itself and the parser must not
            // reconcile it.  EOP-Q-002 is escalated to the owner.
            if (line.find("Precession-Nutation Model") != std::string::npos) {
                provenance.header_model = line.substr(line.find(':') + 1);
                const auto b = provenance.header_model.find_first_not_of(" \t\r");
                if (b != std::string::npos) provenance.header_model.erase(0, b);
            }
            if (line.find("14 C04") != std::string::npos ||
                line.find("EOP (IERS) 14") != std::string::npos) {
                return odl::err(EopError{"EOP-F-001",
                    where(provenance, line_no) + " identifies as 14 C04: \"" + line +
                    "\". This tree requires EOP 20 C04, which replaced it in February 2023. "
                    "They are not interchangeable: 20 C04 is consistent with ITRF2020 where "
                    "14 C04 was consistent with ITRF2014, so the pole coordinates differ."});
            }
            continue;
        }

        std::istringstream row{line};
        int yr = 0, mm = 0, dd = 0, hh = 0;
        double mjd = 0, x = 0, y = 0, dut1 = 0, dx = 0, dy = 0, xrt = 0, yrt = 0, lod = 0;
        if (!(row >> yr >> mm >> dd >> hh >> mjd >> x >> y >> dut1 >> dx >> dy >> xrt >> yrt
                  >> lod)) {
            return odl::err(EopError{"EOP-F-003",
                where(provenance, line_no) + " does not parse as "
                "'YR MM DD HH MJD x y UT1-UTC dX dY xrt yrt LOD ...': \"" + line + "\""});
        }
        const auto mjd_i = static_cast<std::int64_t>(std::llround(mjd));

        if (std::abs(x) > Limits::kPoleArcsec || std::abs(y) > Limits::kPoleArcsec ||
            std::abs(dx) > Limits::kCipMas * 1.0e-3 || std::abs(dy) > Limits::kCipMas * 1.0e-3 ||
            std::abs(lod) > Limits::kLodSeconds) {
            return odl::err(EopError{"EOP-F-004",
                where(provenance, line_no) + ": a field is outside its documented range "
                "(|x|,|y| < 1\", |dX|,|dY| < 100 mas, |LOD| < 10 ms). Got x=" +
                std::to_string(x) + "\" y=" + std::to_string(y) + "\" dX=" + std::to_string(dx) +
                "\" LOD=" + std::to_string(lod) + " s. A units confusion between this product "
                "and finals2000A is a factor of a thousand and lands here."});
        }
        if (last_file_mjd != 0 && mjd_i <= last_file_mjd) {
            return odl::err(EopError{"EOP-F-003",
                where(provenance, line_no) + ": MJD " + std::to_string(mjd_i) +
                " does not increase on the previous row's " + std::to_string(last_file_mjd)});
        }
        if (last_file_mjd != 0 && mjd_i != last_file_mjd + 1) {
            return odl::err(EopError{"EOP-F-003",
                where(provenance, line_no) + ": a gap between MJD " +
                std::to_string(last_file_mjd) + " and " + std::to_string(mjd_i) +
                ". Interpolating across a gap is how a missing week becomes a smooth curve."});
        }
        last_file_mjd = mjd_i;

        if (mjd_i < LeapTable::kFirstSupportedMjd) { ++s.skipped_pre_1972_; continue; }
        if (leaps.expiry_mjd() != 0 && mjd_i > leaps.expiry_mjd() &&
            !leaps.assuming_no_further_leap_seconds()) {
            ++s.skipped_expired_;
            continue;
        }

        auto tai = tai_at_0h(mjd_i, leaps, provenance);
        if (!tai.has_value()) return odl::err(tai.error());

        EopRow r;
        r.mjd = mjd_i;
        r.tai_at_0h = *tai;
        r.value.xp = x * kArcsecToRad;
        r.value.yp = y * kArcsecToRad;
        r.value.dut1 = dut1;
        r.value.dx = dx * kArcsecToRad;
        r.value.dy = dy * kArcsecToRad;
        r.value.lod = lod;
        r.value.xrt = xrt * kArcsecToRad / kDay;
        r.value.yrt = yrt * kArcsecToRad / kDay;
        r.value.quality = EopQuality{Quality::Final, Quality::Final, Quality::Final};
        r.value.fcn_removed = false;
        s.rows_.push_back(r);
    }

    if (s.rows_.size() < 4) {
        return odl::err(EopError{"EOP-F-003",
            provenance.product + " file " + provenance.source_id + " yielded only " +
            std::to_string(s.rows_.size()) + " usable rows; four are the minimum for the "
            "interpolation scheme"});
    }
    s.provenance_.push_back(std::move(provenance));
    return s;
}

// --------------------------------------------------------------------------- //

namespace {

/// finals2000A is FIXED-WIDTH and its numeric fields abut without separators.
bool field(const std::string& line, std::size_t from1, std::size_t to1, double& out) {
    if (line.size() < to1) return false;
    const std::string t = line.substr(from1 - 1, to1 - from1 + 1);
    if (t.find_first_not_of(" \t\r") == std::string::npos) return false;   // blank = ABSENT
    try { out = std::stod(t); } catch (...) { return false; }
    return true;
}

Quality flag_at(const std::string& line, std::size_t col1) {
    if (line.size() < col1) return Quality::Predicted;
    return line[col1 - 1] == 'I' ? Quality::Rapid : Quality::Predicted;
}

}  // namespace

odl::Result<EopSeries, EopError> EopSeries::load_finals2000a(std::string_view text,
                                                              EopProvenance provenance,
                                                              const LeapTable& leaps) {
    EopSeries s;
    provenance.product = "finals2000A";
    std::istringstream in{std::string(text)};
    std::string line;
    std::size_t line_no = 0;
    std::int64_t last_file_mjd = 0;

    while (std::getline(in, line)) {
        ++line_no;
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;

        double mjd = 0;
        if (!field(line, 8, 15, mjd)) continue;            // not a data line
        const auto mjd_i = static_cast<std::int64_t>(std::llround(mjd));

        double x = 0, y = 0, dut1 = 0, lod = 0, dx_mas = 0, dy_mas = 0;
        const bool has_a_pm  = field(line, 19, 27, x) && field(line, 38, 46, y);
        const bool has_a_ut1 = field(line, 59, 68, dut1);
        const bool has_lod   = field(line, 80, 86, lod);
        // dX, dY may be absent; zero is the right reading for an offset that has
        // not been determined, unlike UT1 where blank would be a 0.9 s error.
        (void)(field(line, 98, 106, dx_mas) && field(line, 117, 125, dy_mas));
        if (!has_a_pm || !has_a_ut1) continue;             // no Bulletin A values yet

        EopQuality q{flag_at(line, 17), flag_at(line, 58), flag_at(line, 96)};

        // Bulletin B, where it has reached: better than Bulletin A and preferred.
        double bx = 0, by = 0, bu = 0, bdx = 0, bdy = 0;
        if (field(line, 135, 144, bx) && field(line, 145, 154, by) &&
            field(line, 155, 165, bu)) {
            x = bx; y = by; dut1 = bu;
            q.pole = Quality::BulletinB;
            q.ut1 = Quality::BulletinB;
            if (field(line, 166, 175, bdx) && field(line, 176, 185, bdy)) {
                dx_mas = bdx; dy_mas = bdy;
                q.nutation = Quality::BulletinB;
            }
        }

        if (std::abs(x) > Limits::kPoleArcsec || std::abs(y) > Limits::kPoleArcsec ||
            std::abs(dx_mas) > Limits::kCipMas) {
            return odl::err(EopError{"EOP-F-004",
                where(provenance, line_no) + ": a field is outside its documented range. "
                "Got x=" + std::to_string(x) + "\" dX=" + std::to_string(dx_mas) + " mas. "
                "If dX is a thousand times too large this is finals.all (dPsi/dEps against "
                "IAU 1980), not finals2000A.all (dX/dY against IAU 2000A) — EOP-R-002."});
        }
        if (last_file_mjd != 0 && mjd_i != last_file_mjd + 1) {
            return odl::err(EopError{"EOP-F-003",
                where(provenance, line_no) + ": MJD " + std::to_string(mjd_i) +
                " does not follow " + std::to_string(last_file_mjd)});
        }
        last_file_mjd = mjd_i;
        if (mjd_i < LeapTable::kFirstSupportedMjd) { ++s.skipped_pre_1972_; continue; }
        if (leaps.expiry_mjd() != 0 && mjd_i > leaps.expiry_mjd() &&
            !leaps.assuming_no_further_leap_seconds()) {
            ++s.skipped_expired_;
            continue;
        }
        auto tai = tai_at_0h(mjd_i, leaps, provenance);
        if (!tai.has_value()) return odl::err(tai.error());

        EopRow r;
        r.mjd = mjd_i;
        r.tai_at_0h = *tai;
        r.value.xp = x * kArcsecToRad;
        r.value.yp = y * kArcsecToRad;
        r.value.dut1 = dut1;
        r.value.lod = has_lod ? lod * 1.0e-3 : 0.0;    // EOP-R-025: ms -> s, blank is 0 LOD
        r.value.dx = dx_mas * kMasToRad;
        r.value.dy = dy_mas * kMasToRad;
        r.value.quality = q;
        r.value.fcn_removed = false;                   // EOP-R-028
        s.rows_.push_back(r);
    }
    if (s.rows_.size() < 4) {
        return odl::err(EopError{"EOP-F-003",
            provenance.product + " file " + provenance.source_id + " yielded only " +
            std::to_string(s.rows_.size()) + " usable rows"});
    }
    s.provenance_.push_back(std::move(provenance));
    return s;
}

// --------------------------------------------------------------------------- //

Coverage EopSeries::coverage() const noexcept {
    // EOP-R-052: the reported interval is the USABLE one.  The 4-point scheme
    // needs two tabulated points each side, so it is two days inside the data's
    // own extent, and a caller that checks coverage before a run must be told
    // the interval it can actually ask about.
    if (rows_.size() < 4) return Coverage{1, 0};
    return Coverage{rows_.front().mjd + 1, rows_.back().mjd - 2};
}

odl::Result<EopSeries, EopError> EopSeries::splice(const EopSeries& final_series,
                                                    const EopSeries& rapid_series,
                                                    const EopPolicy& policy) {
    EopSeries s;
    s.provenance_ = final_series.provenance_;
    s.provenance_.insert(s.provenance_.end(), rapid_series.provenance_.begin(),
                         rapid_series.provenance_.end());
    s.skipped_pre_1972_ = final_series.skipped_pre_1972_ + rapid_series.skipped_pre_1972_;
    s.skipped_expired_ = final_series.skipped_expired_ + rapid_series.skipped_expired_;

    const std::int64_t boundary = final_series.rows_.empty()
                                      ? 0 : final_series.rows_.back().mjd;

    // EOP-R-033: measure the discontinuity where the two products overlap and
    // refuse one above the threshold, because at that size it means mismatched
    // products rather than measurement noise.
    const auto find = [](const EopSeries& e, std::int64_t mjd) -> const EopRow* {
        const auto it = std::lower_bound(e.rows_.begin(), e.rows_.end(), mjd,
                                          [](const EopRow& r, std::int64_t m) { return r.mjd < m; });
        return (it != e.rows_.end() && it->mjd == mjd) ? &*it : nullptr;
    };
    if (const EopRow* a = find(final_series, boundary)) {
        if (const EopRow* b = find(rapid_series, boundary)) {
            const double dpole = std::max(std::abs(a->value.xp - b->value.xp),
                                          std::abs(a->value.yp - b->value.yp));
            const double dut1 = std::abs(a->value.dut1 - b->value.dut1);
            if (dpole > policy.splice_pole_limit_rad || dut1 > policy.splice_ut1_limit_s) {
                return odl::err(EopError{"EOP-F-006",
                    "splice discontinuity at MJD " + std::to_string(boundary) + " between " +
                    final_series.provenance_.front().source_id + " and " +
                    rapid_series.provenance_.front().source_id + ": pole " +
                    std::to_string(dpole / (kArcsecToRad * 1.0e-3)) + " mas, UT1 " +
                    std::to_string(dut1 * 1.0e3) + " ms, thresholds " +
                    std::to_string(policy.splice_pole_limit_rad / (kArcsecToRad * 1.0e-3)) +
                    " mas and " + std::to_string(policy.splice_ut1_limit_s * 1.0e3) +
                    " ms. At this size the two products are not the same realisation — a C04 "
                    "from before a retroactive revision spliced against a current "
                    "finals2000A will do it."});
            }
        }
    }

    // EOP-R-034: a hard switch, never a blend.  A taper hides the discontinuity
    // the check above exists to expose.
    s.rows_ = final_series.rows_;
    for (const EopRow& r : rapid_series.rows_) {
        if (r.mjd > boundary) s.rows_.push_back(r);
    }
    if (s.rows_.size() < 4) {
        return odl::err(EopError{"EOP-F-003", "the spliced series has fewer than four rows"});
    }
    return s;
}

// --------------------------------------------------------------------------- //

odl::Result<EopRecord, EopError> EopSeries::at(const Epoch& when,
                                                const EopPolicy& policy) const {
    return query(when, policy, policy.apply_subdaily);
}

odl::Result<EopRecord, EopError> EopSeries::raw_at(const Epoch& when,
                                                    const EopPolicy& policy) const {
    return query(when, policy, false);
}

odl::Result<EopRecord, EopError> EopSeries::query(const Epoch& when, const EopPolicy& policy,
                                                   bool subdaily) const {
    const std::int64_t t = when.tai_seconds();
    const Coverage cov = coverage();
    const auto out_of_range = [&](const char* why) {
        return EopError{"EOP-F-007",
            std::string("EOP requested outside the usable coverage: ") + why +
            ". Requested TAI second " + std::to_string(t) + "; usable coverage is MJD " +
            std::to_string(cov.first_mjd) + " to " + std::to_string(cov.last_mjd) +
            " (the data run MJD " + std::to_string(rows_.front().mjd) + " to " +
            std::to_string(rows_.back().mjd) + "; the interpolation needs two tabulated "
            "points each side, so the usable interval is two days inside that). Sources: " +
            provenance_.front().source_id +
            ". There is no extrapolation: holding the last UT1 constant drifts at the rate of "
            "LOD, about 0.5 m of position per day at LEO, and a fit absorbs it."};
    };

    if (rows_.size() < 4) return odl::err(out_of_range("the series has fewer than four rows"));

    // Locate the bracketing rows on the TAI axis, so a leap-second day is 86401
    // seconds long here exactly as it is in the world.
    const auto it = std::upper_bound(rows_.begin(), rows_.end(), t,
                                      [](std::int64_t v, const EopRow& r) {
                                          return v < r.tai_at_0h;
                                      });
    if (it == rows_.begin() || it == rows_.end()) {
        return odl::err(out_of_range("before the first row or after the last"));
    }
    const std::size_t i = static_cast<std::size_t>(std::distance(rows_.begin(), it)) - 1;
    // EOP-R-041: two points before and two after.  This is STRICTER than the IERS
    // reference routine, whose LAGINT clamps its window at the ends of the array
    // — a silent reduction in interpolation order, and at the very end a silent
    // extrapolation. Plan §5 constraint 4 forbids both.
    if (i < 1 || i + 2 >= rows_.size()) {
        return odl::err(out_of_range("within two tabulated days of an end of the series"));
    }

    const double span = static_cast<double>(rows_[i + 1].tai_at_0h - rows_[i].tai_at_0h);
    const double frac = (static_cast<double>(t - rows_[i].tai_at_0h) + when.tai_fraction()) / span;
    const double xq = static_cast<double>(rows_[i].mjd) + frac;

    // Four-point Lagrange, matching the IERS reference scheme.
    double w[4];
    const double xs[4] = {static_cast<double>(rows_[i - 1].mjd), static_cast<double>(rows_[i].mjd),
                          static_cast<double>(rows_[i + 1].mjd),
                          static_cast<double>(rows_[i + 2].mjd)};
    for (int k = 0; k < 4; ++k) {
        double p = 1.0;
        for (int m = 0; m < 4; ++m) {
            if (m != k) p *= (xq - xs[m]) / (xs[k] - xs[m]);
        }
        w[k] = p;
    }
    const auto lerp = [&](double EopRecord::*f) {
        double v = 0.0;
        for (int k = 0; k < 4; ++k) v += w[k] * (rows_[i - 1 + static_cast<std::size_t>(k)].value.*f);
        return v;
    };

    EopRecord out;
    out.xp = lerp(&EopRecord::xp);
    out.yp = lerp(&EopRecord::yp);
    out.dut1 = lerp(&EopRecord::dut1);
    out.lod = lerp(&EopRecord::lod);
    out.dx = lerp(&EopRecord::dx);
    out.dy = lerp(&EopRecord::dy);
    out.quality = rows_[i].value.quality;
    out.fcn_removed = rows_[i].value.fcn_removed;

    if (std::abs(out.dut1) > policy.dut1_range_limit_s) {
        return odl::err(EopError{"EOP-F-004",
            "interpolated UT1−UTC of " + std::to_string(out.dut1) + " s exceeds the policy "
            "limit of " + std::to_string(policy.dut1_range_limit_s) + " s. The limit is "
            "configurable and NOT hard-coded at 0.9 s, because CGPM Resolution 4 (2022) "
            "raises the permitted maximum when leap seconds stop."});
    }
    if (out.quality.worst() > policy.max_quality) {
        return odl::err(EopError{"EOP-F-005",
            std::string("EOP at this epoch is ") + name_of(out.quality.worst()) +
            ", worse than the policy's " + name_of(policy.max_quality) + ". Pole is " +
            name_of(out.quality.pole) + ", UT1 " + name_of(out.quality.ut1) + ", nutation " +
            name_of(out.quality.nutation) + ". Prediction is legitimate for planning and "
            "illegitimate for a published fit, and the difference is a declared choice."});
    }

    if (subdaily) {
        const double mjd_jd1 = std::floor(xq) + 2400000.5;
        const auto args = tides::arguments_at(mjd_jd1, xq - std::floor(xq),
                                              mjd_jd1, xq - std::floor(xq));
        const auto c = tides::subdaily(args);
        out.xp += c.dxp;
        out.yp += c.dyp;
        out.dut1 += c.dut1;
        out.lod += c.dlod;
        out.subdaily_applied = true;
    }
    return out;
}

}  // namespace odl::eop
