#include <odl/forcemodel/truncation.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <sstream>

namespace odl::forcemodel {

odl::Result<TruncationChoice, ForceModelError>
degree_meeting_criterion(const gravity::ConventionalField& field, double radius_m, double f_min_m_s2, std::string compared_against) {
    if (!(radius_m > 0.0) || !std::isfinite(radius_m) || !(f_min_m_s2 > 0.0) || !std::isfinite(f_min_m_s2)) {
        std::ostringstream m;
        m << "the truncation criterion needs a positive radius and comparator; given r = " << radius_m << " m, F_min = " << f_min_m_s2
          << " m/s^2 (FMOD-R-011)";
        return odl::err(ForceModelError{"FMOD-F-003", m.str()});
    }
    TruncationChoice out;
    out.radius_m = radius_m;
    out.f_min_m_s2 = f_min_m_s2;
    out.compared_against = std::move(compared_against);
    double previous = 0.0;
    for (int n = 2; n <= field.max_degree(); ++n) {
        auto deg = gravity::Degree::of(n);
        if (!deg) return odl::err(ForceModelError{deg.error().id, deg.error().message});
        auto rms = field.truncation_rms(radius_m, *deg);
        if (!rms) return odl::err(ForceModelError{rms.error().id, rms.error().message});
        if (*rms <= f_min_m_s2 || n == field.max_degree()) {
            out.degree = n;
            out.rms_at_degree = *rms;
            out.rms_below = previous;
            return out;
        }
        previous = *rms;
    }
    return odl::err(ForceModelError{"FMOD-F-003", "the truncation criterion found no degree"});     // unreachable: the loop returns at max_degree
}

odl::Result<SmallestContribution, ForceModelError>
smallest_nonzero_contribution(const std::vector<dyn::Contribution>& contributions, const std::vector<std::string>& excluded) {
    SmallestContribution best;
    best.value_m_s2 = 0.0;
    bool found = false;
    for (const auto& c : contributions) {
        if (std::find(excluded.begin(), excluded.end(), c.force.name) != excluded.end()) continue;
        const double v = c.acceleration_m_s2.norm();
        if (!(v > 0.0)) continue;
        if (!found || v < best.value_m_s2) {
            best = SmallestContribution{c.force.name, v};
            found = true;
        }
    }
    if (!found) {
        std::ostringstream m;
        m << "no force other than the truncated series is non-zero at this point (" << contributions.size()
          << " contributions, " << excluded.size() << " excluded); the truncation criterion has no comparator (FMOD-F-003)";
        return odl::err(ForceModelError{"FMOD-F-003", m.str()});
    }
    return best;
}

odl::Result<std::vector<double>, ForceModelError>
orbit_tail_maxima(const gravity::ConventionalField& field, double radius_m, double inclination_rad,
                  const std::vector<int>& degrees, int samples) {
    constexpr double kOmegaEarth = 7.2921150e-5;                      // rad/s, the mean rotation rate (the track's longitude only; a stated simplification)
    const double gm = field.scaling().gm_m3_s2();
    const double period = 2.0 * std::numbers::pi * std::sqrt(radius_m * radius_m * radius_m / gm);
    std::vector<double> maxima(degrees.size(), 0.0);
    const int top = field.max_degree();
    auto d_top = gravity::Degree::of(top);
    auto o_top = gravity::Order::of(gravity::kEgm2008MaxOrder);
    if (!d_top || !o_top) return odl::err(ForceModelError{"FMOD-F-003", "the model's top degree is not constructible"});
    for (int k = 0; k < samples; ++k) {
        const double u = 2.0 * std::numbers::pi * static_cast<double>(k) / static_cast<double>(samples);   // argument of latitude
        const double lat = std::asin(std::sin(inclination_rad) * std::sin(u));
        const double lon = std::atan2(std::cos(inclination_rad) * std::sin(u), std::cos(u))
                         - kOmegaEarth * period * u / (2.0 * std::numbers::pi);
        const Vec3 p{radius_m * std::cos(lat) * std::cos(lon), radius_m * std::cos(lat) * std::sin(lon), radius_m * std::sin(lat)};
        auto by_degree = field.acceleration_by_degree(frames::ItrsPosition{p}, *d_top, *o_top);
        if (!by_degree) return odl::err(ForceModelError{by_degree.error().id, by_degree.error().message});
        // tail(N) = sum_{n > N} by_degree[n]
        std::vector<Vec3> tail(by_degree->size() + 1, Vec3{});
        for (std::size_t n = by_degree->size(); n-- > 0;) tail[n] = tail[n + 1] + (*by_degree)[n];
        for (std::size_t i = 0; i < degrees.size(); ++i) {
            const auto nn = static_cast<std::size_t>(degrees[i]) + 1;     // the sum from degree N+1
            maxima[i] = std::max(maxima[i], tail[std::min(nn, tail.size() - 1)].norm());
        }
    }
    return maxima;
}

}  // namespace odl::forcemodel
