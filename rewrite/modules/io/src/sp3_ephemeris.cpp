#include <odl/io/sp3_ephemeris.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>

namespace odl::io {

namespace {

/// Days since 1970-01-01 for the proleptic Gregorian calendar. Exact for any
/// (year, month, day), needs no leap-second table -- it is pure calendar
/// arithmetic, not a physical-time computation. Howard Hinnant's
/// `days_from_civil` algorithm (public domain,
/// howardhinnant.github.io/date_algorithms.html), the same one C++20's
/// std::chrono uses internally for `year_month_day`.
std::int64_t days_from_civil(int y, int m, int d) noexcept {
    y -= m <= 2;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = static_cast<unsigned>((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

/// A gap between two consecutive-in-time samples beyond this multiple of the
/// file's own nominal interval is a missed epoch, not floating-point jitter
/// on an otherwise-uniform grid.
constexpr double kGapFactor = 1.5;

}  // namespace

double calendar_elapsed_seconds(const time::Calendar& from, const time::Calendar& to) {
    const std::int64_t day_delta =
        days_from_civil(to.year, to.month, to.day) - days_from_civil(from.year, from.month, from.day);
    const double sec_of_day_from = from.hour * 3600.0 + from.minute * 60.0 + from.second;
    const double sec_of_day_to = to.hour * 3600.0 + to.minute * 60.0 + to.second;
    return static_cast<double>(day_delta) * 86400.0 + (sec_of_day_to - sec_of_day_from);
}

odl::Vec3 lagrange_interpolate(const std::vector<std::pair<double, odl::Vec3>>& points, double t_query) {
    // Classic Lagrange form, evaluated independently per component. Callers
    // of this function use at most a handful of points (Sp3Ephemeris's own
    // default order is 10, 11 points) and call it a handful of times, not in
    // a hot loop, so the O(n^2) double loop costs nothing at this size, and a
    // Neville or barycentric form would only spend complexity budget this
    // call site does not need.
    double x = 0.0, y = 0.0, z = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        double basis = 1.0;
        for (std::size_t j = 0; j < points.size(); ++j) {
            if (j == i) continue;
            basis *= (t_query - points[j].first) / (points[i].first - points[j].first);
        }
        x += basis * points[i].second.x;
        y += basis * points[i].second.y;
        z += basis * points[i].second.z;
    }
    return odl::Vec3{x, y, z};
}

odl::Result<Sp3Ephemeris, Sp3EphemerisError> Sp3Ephemeris::build(
        const Sp3File& file, const std::string& satellite_id, int target_order) {
    std::vector<std::pair<time::Calendar, Sp3PositionRecord>> found;
    for (const auto& epoch : file.epochs) {
        for (const auto& sat : epoch.satellites) {
            if (sat.position.satellite_id == satellite_id) {
                found.emplace_back(epoch.epoch, sat.position);
                break;  // a well-formed epoch names a satellite at most once
            }
        }
    }
    if (found.size() < 2) {
        return odl::err(Sp3EphemerisError{"IOFM-F-012",
            "satellite '" + satellite_id + "' has " + std::to_string(found.size()) +
            " sample(s) in this file; at least 2 are needed to interpolate anything"});
    }

    const time::Calendar first_epoch = found.front().first;
    std::vector<Sample> samples;
    samples.reserve(found.size());
    for (const auto& [cal, pos] : found) {
        samples.push_back(Sample{calendar_elapsed_seconds(first_epoch, cal),
                                  odl::Vec3{pos.x_km, pos.y_km, pos.z_km}, pos.maneuver});
    }
    // file.epochs is read in file order, which IOFM-R-001's own sentinel-
    // driven reading never reorders, and SP3D's own epoch records are
    // chronological -- samples_ inherits strictly increasing t_s.

    double nominal_interval_s = file.header.epoch_interval_s;
    if (!(nominal_interval_s > 0.0)) {
        // No stated interval (a hand-built or malformed header): fall back
        // to the smallest observed gap, so gap detection below still means
        // something instead of comparing against zero.
        nominal_interval_s = samples[1].t_s - samples[0].t_s;
        for (std::size_t i = 2; i < samples.size(); ++i) {
            nominal_interval_s = std::min(nominal_interval_s, samples[i].t_s - samples[i - 1].t_s);
        }
    }

    const int order = std::min(target_order, static_cast<int>(samples.size()) - 1);
    return Sp3Ephemeris(std::move(samples), order, nominal_interval_s, first_epoch);
}

double Sp3Ephemeris::first_sample_seconds() const noexcept { return samples_.front().t_s; }
double Sp3Ephemeris::last_sample_seconds() const noexcept { return samples_.back().t_s; }

odl::Result<odl::Vec3, Sp3EphemerisError> Sp3Ephemeris::position_km_at(double t_s) const {
    if (t_s < samples_.front().t_s || t_s > samples_.back().t_s) {
        return odl::err(Sp3EphemerisError{"IOFM-F-009",
            "t=" + std::to_string(t_s) + "s is outside the sampled span [" +
            std::to_string(samples_.front().t_s) + ", " + std::to_string(samples_.back().t_s) +
            "]s -- this ephemeris never extrapolates"});
    }

    // idx: the last sample at or before t_s (upper_bound's predecessor).
    const auto it = std::upper_bound(samples_.begin(), samples_.end(), t_s,
        [](double v, const Sample& s) { return v < s.t_s; });
    const std::size_t idx = static_cast<std::size_t>((it - samples_.begin())) - 1;

    const int npoints = order_ + 1;
    // Centre the window on the bracketing pair (idx, idx+1), clamped to the
    // array -- the same "centred where possible, shifted away from the file's
    // own time limits" placement Schenewerk (2003) describes.
    long lo = static_cast<long>(idx) - (npoints - 1) / 2;
    lo = std::max<long>(0, std::min<long>(lo, static_cast<long>(samples_.size()) - npoints));
    const auto window_lo = static_cast<std::size_t>(lo);
    const std::size_t window_hi = window_lo + static_cast<std::size_t>(npoints);  // exclusive

    for (std::size_t i = window_lo; i < window_hi; ++i) {
        if (samples_[i].maneuver) {
            return odl::err(Sp3EphemerisError{"IOFM-F-011",
                "the interpolation window for t=" + std::to_string(t_s) +
                "s includes a manoeuvre-flagged sample at t=" + std::to_string(samples_[i].t_s) +
                "s -- refusing rather than fitting a polynomial across a real discontinuity"});
        }
    }
    for (std::size_t i = window_lo + 1; i < window_hi; ++i) {
        const double delta = samples_[i].t_s - samples_[i - 1].t_s;
        if (delta > kGapFactor * nominal_interval_s_) {
            return odl::err(Sp3EphemerisError{"IOFM-F-010",
                "the interpolation window for t=" + std::to_string(t_s) + "s spans a gap of " +
                std::to_string(delta) + "s (nominal interval " +
                std::to_string(nominal_interval_s_) + "s) between t=" +
                std::to_string(samples_[i - 1].t_s) + "s and t=" + std::to_string(samples_[i].t_s) +
                "s -- refusing rather than fitting a polynomial across a missed epoch"});
        }
    }

    std::vector<std::pair<double, odl::Vec3>> window;
    window.reserve(static_cast<std::size_t>(npoints));
    for (std::size_t i = window_lo; i < window_hi; ++i) {
        window.emplace_back(samples_[i].t_s, samples_[i].r_km);
    }
    return lagrange_interpolate(window, t_s);
}

odl::Result<odl::Vec3, Sp3EphemerisError> central_difference_velocity_km_s(
        const Sp3Ephemeris& eph, double t_s, double h_s) {
    auto r_minus = eph.position_km_at(t_s - h_s);
    if (!r_minus.has_value()) return odl::err(r_minus.error());
    auto r_plus = eph.position_km_at(t_s + h_s);
    if (!r_plus.has_value()) return odl::err(r_plus.error());
    return (1.0 / (2.0 * h_s)) * (*r_plus - *r_minus);
}

}  // namespace odl::io
