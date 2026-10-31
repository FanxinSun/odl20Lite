#pragma once
// arc_fixture.hpp — L7 step 3, part 2: the solar-sail arc through the real chain (the manager's rulings of 2026-10-31).
//
// WHAT IT IS.  A satellite with the area-to-mass ratio of LightSail-2 (32 m^2 / 4.93 kg = 6.49 m^2/kg) flies three days under the force model
// {EGM2008 to degree 10, the Sun and the Moon as third bodies, solar radiation pressure with the conical shadow of L2}; DRAG IS OFF, and the
// arc says so (the seven-cutoff question of the atmosphere is not settled and is not decided here).  Three stations of the real SLRF2020
// registry range it through L6's range model (light time, troposphere, Shapiro, eccentricity, centre of mass).  The six state components at the
// first epoch, in metres and metres per second, are the ONLY estimated parameters: no C_R is estimated, no other coefficient.
//
// THE LIMIT, STATED: the SAME model generates the data and fits it.  The gate therefore tests the estimator's convergence on the full chain,
// not the model's fidelity; a fit that reaches the truth here says nothing about how well the model describes a real sail.
//
// THE SRP MODEL (part 2 of the report states it, and this is its source): a flat surface of area 32 m^2 that always faces the Sun (the
// `flat_surface_sun_pointing` surface of L5), absorptivity 0.10, specular reflectivity 0.80, diffuse reflectivity 0.10 (sum 1), mass 4.93 kg.
// The attitude assumption is therefore "sun-pointing, normal to the Sun line at every instant": a FIXED CANNONBALL IS NOT A SAIL and this is not
// one.  The reflectivities are test-stated values of a typical aluminised film, not LightSail-2's measured ones.
//
// TRAJECTORY AT THE OBSERVATION EPOCHS ONLY.  Each observation is tagged at the BOUNCE (`EpochEvent::Bounce`), so L6 asks the trajectory
// for the state at that very epoch and no other; the state there is the chain of the variational equations from the first epoch through the
// earlier observation epochs (Phi(t_k, t_0) = Phi(t_k, t_{k-1}) Phi(t_{k-1}, t_0)), and the row of the Jacobian is L6's 1x6 partial times that
// Phi.  Both are in SI (m, m/s): Phi is the same matrix in either unit system.
#include <catch2/catch_test_macros.hpp>

#include "../../modules/forcemodel/tests/fixture.hpp"
#include "../../modules/measmod/tests/test_data.hpp"

#include <odl/dynamics/force_set.hpp>
#include <odl/dynamics/parameter.hpp>
#include <odl/dynamics/variational.hpp>
#include <odl/macromodel/cited.hpp>
#include <odl/macromodel/macromodel.hpp>
#include <odl/measmod/range.hpp>
#include <odl/measmod/tracks.hpp>
#include <odl/shadow/conical.hpp>
#include <odl/srp/srp.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace l7arc {

using odl::Vec3;
namespace mm = odl::measmod;
namespace dyn = odl::dyn;
using Mat6 = dyn::Mat6;

inline constexpr double kPi = 3.14159265358979323846;
inline constexpr double kC = 299792458.0;                    // m/s
inline constexpr double kAreaM2 = 32.0, kMassKg = 4.93;      // LightSail-2
inline constexpr double kAbsorptivity = 0.10, kSpecular = 0.80, kDiffuse = 0.10;
inline constexpr double kRangeSigmaM = 0.01;                 // the weight of the fit, and the noise of the one seeded run
inline constexpr double kMinElevationDeg = 10.0;
inline constexpr double kPropagationTolerance = 1e-9;        // the integrator tolerance for the state and the variational equations (measured: 7 mm/day against a 1e-13 reference, 4.6 s/day with the STM)

/// One arc: its first epoch, the Keplerian truth, the span, and the stations.
struct ArcDesign {
    const char* name;
    int year, month, day, hour;                              // UTC of the first epoch
    double a_m, e, inc_deg, raan_deg, argp_deg, mean_anomaly_deg;
    double days;
    double spacing_s;                                        // one observation per spacing while a station sees the satellite
};

inline odl::time::Epoch epoch_of(const ArcDesign& d) {
    odl::time::Calendar c;
    c.year = d.year; c.month = d.month; c.day = d.day; c.hour = d.hour; c.minute = 0; c.second = 0.0;
    auto e = odl::time::Epoch::from_calendar(odl::time::TimeScale::UTC, c, fx::leaps());
    REQUIRE(e.has_value());
    return *e;
}

/// Keplerian elements to the osculating GCRS state, metres and metres per second (two-body, mu = the fixture's GM).
inline std::array<double, 6> cartesian_of(const ArcDesign& d) {
    const double mu = fx::kGmFixture;
    const double i = d.inc_deg * kPi / 180.0, O = d.raan_deg * kPi / 180.0, w = d.argp_deg * kPi / 180.0, M = d.mean_anomaly_deg * kPi / 180.0;
    double E = M;
    for (int k = 0; k < 50; ++k) E -= (E - d.e * std::sin(E) - M) / (1.0 - d.e * std::cos(E));
    const double xp = d.a_m * (std::cos(E) - d.e), yp = d.a_m * std::sqrt(1.0 - d.e * d.e) * std::sin(E);
    const double n = std::sqrt(mu / (d.a_m * d.a_m * d.a_m)), rr = d.a_m * (1.0 - d.e * std::cos(E));
    const double vxp = -d.a_m * n * std::sin(E) * d.a_m / rr, vyp = d.a_m * n * std::sqrt(1.0 - d.e * d.e) * std::cos(E) * d.a_m / rr;
    const double cO = std::cos(O), sO = std::sin(O), ci = std::cos(i), si = std::sin(i), cw = std::cos(w), sw = std::sin(w);
    const double r11 = cO * cw - sO * sw * ci, r12 = -cO * sw - sO * cw * ci;
    const double r21 = sO * cw + cO * sw * ci, r22 = -sO * sw + cO * cw * ci;
    const double r31 = sw * si, r32 = cw * si;
    return {r11 * xp + r12 * yp, r21 * xp + r22 * yp, r31 * xp + r32 * yp, r11 * vxp + r12 * vyp, r21 * vxp + r22 * vyp, r31 * vxp + r32 * vyp};
}

inline odl::frames::GcrsState state_from(const odl::time::Epoch& t, const std::array<double, 6>& x) {
    return odl::frames::GcrsState{t, odl::km_from_metres(Vec3{x[0], x[1], x[2]}), odl::km_from_metres(Vec3{x[3], x[4], x[5]})};
}
inline std::array<double, 6> metres_of(const odl::frames::GcrsState& s) {
    const Vec3 r = odl::metres_from_km(s.position()), v = odl::metres_from_km(s.velocity());
    return {r.x, r.y, r.z, v.x, v.y, v.z};
}

/// The force model of the arc: gravity to degree 10, the Sun and the Moon, SRP with the conical shadow.  No drag, no tides, no relativity (stated above).
class ArcForces {
public:
    ArcForces() : set_(registry_), params_(registry_) {
        const auto eo = fx::orientation_series();
        odl::forcemodel::GravityOptions go;
        go.extrapolate_secular = true;
        go.sources = fx::sources();
        REQUIRE(set_.add(std::make_shared<odl::forcemodel::Gravity>(fx::model(), eo, fx::deg(10), fx::ord(10), go)).has_value());
        const std::vector<odl::eph::Body> bodies{odl::eph::Body::Sun, odl::eph::Body::Moon};
        auto tb = odl::forcemodel::third_bodies(bodies, fx::ephemeris(), fx::gm(), fx::leaps(), fx::sources());
        REQUIRE(tb.has_value());
        for (auto& f : *tb) REQUIRE(set_.add(f).has_value());
        auto area = odl::macromodel::cited(kAreaM2, "LightSail-2 sail area, 32 m^2 (the project's stated value)");
        auto ab = odl::macromodel::cited(kAbsorptivity, "test-stated");
        auto sp = odl::macromodel::cited(kSpecular, "test-stated");
        auto df = odl::macromodel::cited(kDiffuse, "test-stated");
        REQUIRE(area.has_value()); REQUIRE(ab.has_value()); REQUIRE(sp.has_value()); REQUIRE(df.has_value());
        auto panel = odl::macromodel::flat_surface_sun_pointing(*area, *ab, *sp, *df);
        REQUIRE(panel.has_value());
        odl::macromodel::MacromodelBuilder b;
        b.add_surface(*panel);
        auto mass = odl::macromodel::cited(kMassKg, "LightSail-2 mass, 4.93 kg (the project's stated value)");
        auto com = odl::macromodel::cited(Vec3{0.0, 0.0, 0.0}, "test-stated");
        REQUIRE(mass.has_value()); REQUIRE(com.has_value());
        b.set_mass(*mass).set_centre_of_mass(*com);
        auto m = std::move(b).build();
        REQUIRE(m.has_value());
        REQUIRE(set_.add(std::make_shared<odl::srp::Srp>(*m, fx::ephemeris(), fx::leaps())).has_value());
    }
    ArcForces(const ArcForces&) = delete;
    ArcForces& operator=(const ArcForces&) = delete;
    [[nodiscard]] const dyn::ForceSet& forces() const { return set_; }
    [[nodiscard]] const dyn::ParameterSet& params() const { return params_; }

private:
    dyn::ParameterRegistry registry_;
    dyn::ForceSet set_;
    dyn::ParameterSet params_;
};

inline double seconds_between(const odl::time::Epoch& later, const odl::time::Epoch& earlier) { return later.difference(earlier).to_seconds(); }

/// The state at t0 propagated to `t` (no variational equations).
inline odl::frames::GcrsState propagate_to(const ArcForces& f, const odl::frames::GcrsState& from, const odl::time::Epoch& t) {
    auto r = dyn::propagate(f.forces(), f.params(), from, seconds_between(t, from.epoch()), kPropagationTolerance);
    if (!r) FAIL("propagate: " << r.error().id << " " << r.error().message);
    return *r;
}

/// A grid of states every `step_s` seconds from the first epoch, chained propagation (the truth for geometry, never for the fit).
inline std::vector<odl::frames::GcrsState> state_grid(const ArcForces& f, const odl::frames::GcrsState& x0, double span_s, double step_s) {
    std::vector<odl::frames::GcrsState> g;
    g.push_back(x0);
    const auto n = static_cast<std::size_t>(std::llround(span_s / step_s));
    for (std::size_t k = 1; k <= n; ++k) g.push_back(propagate_to(f, g.back(), x0.epoch().add(odl::time::Duration::from_seconds(step_s * static_cast<double>(k)))));
    return g;
}

/// The ephemeris Sun, geocentric GCRS, metres.
inline Vec3 sun_m(const odl::time::Epoch& t) {
    auto s = fx::ephemeris().geocentric_state(odl::eph::Body::Sun, t, fx::leaps());
    REQUIRE(s.has_value());
    return odl::metres_from_km(s->position());
}

/// THE CLOSED-FORM CLEARANCE MARGIN, independent of the shadow module (the manager's ruling of 2026-10-31).  Definition, registered:
///   s = the unit vector Earth -> Sun, D = the Sun's distance, Re = 6378137 m, Rs = 6.957e8 m (the shadow module's own radii, so that the cones are the same
///   cones), x = -(r . s) the depth behind the Earth's centre, rho = |r - (r . s) s| the distance from the shadow axis, and the PENUMBRA cone
///   radius at depth x  rho_p(x) = Re + x tan(alpha_p), tan(alpha_p) = (Rs + Re) / D  (the outer cone: the Sun's disc is not wholly visible inside it).
///   The clearance of one position is  psi = (rho - rho_p(x)) / |r|  radians, for x > 0; for x <= 0 (the satellite is on the sunward side of the Earth's
///   centre) it is +infinity.  The margin of an arc is the MINIMUM of psi over its samples, the Sun taken from the ephemeris at each sample epoch
///   (the Sun's motion), the positions from the propagated trajectory itself (the node's drift, the force model).  Positive means clear of the penumbra.
inline double clearance_rad(const Vec3& r_m, const Vec3& sun_pos_m) {
    constexpr double Re = 6378137.0, Rs = 6.957e8;
    const double D = sun_pos_m.norm();
    const Vec3 s = (1.0 / D) * sun_pos_m;
    const double along = r_m.x * s.x + r_m.y * s.y + r_m.z * s.z;
    const double x = -along;
    if (x <= 0.0) return INFINITY;
    const Vec3 perp = r_m - along * s;
    const double rho = perp.norm();
    const double rho_p = Re + x * (Rs + Re) / D;
    return (rho - rho_p) / r_m.norm();
}

/// Cubic Hermite interpolation of a position between two grid states (positions and velocities in metres, metres/second).
inline Vec3 hermite_position(const std::array<double, 6>& a, const std::array<double, 6>& b, double h, double u) {
    const double u2 = u * u, u3 = u2 * u;
    const double h00 = 2 * u3 - 3 * u2 + 1, h10 = u3 - 2 * u2 + u, h01 = -2 * u3 + 3 * u2, h11 = u3 - u2;
    return Vec3{h00 * a[0] + h10 * h * a[3] + h01 * b[0] + h11 * h * b[3], h00 * a[1] + h10 * h * a[4] + h01 * b[1] + h11 * h * b[4],
                h00 * a[2] + h10 * h * a[5] + h01 * b[2] + h11 * h * b[5]};
}

struct IlluminationSummary {
    double min_clearance_rad = INFINITY;     // the closed-form margin, minimum over all samples
    double min_nu = 1.0;                     // the shadow module's illumination fraction, minimum over all samples
    double dark_seconds = 0.0;               // sampled time with nu < 1
    std::size_t samples = 0;
    double sample_step_s = 0.0;
    std::size_t edge_crossings = 0;          // sign changes of (nu == 1)
};

/// Sample a state grid at `sample_s` (<= the grid step, by Hermite) for the margin and the shadow module's nu.
inline IlluminationSummary illumination(const std::vector<odl::frames::GcrsState>& grid, double grid_step_s, double sample_s) {
    IlluminationSummary s;
    s.sample_step_s = sample_s;
    const auto sub = static_cast<std::size_t>(std::llround(grid_step_s / sample_s));
    bool prev_lit = true;
    bool first = true;
    for (std::size_t k = 0; k + 1 < grid.size(); ++k) {
        const auto a = metres_of(grid[k]), b = metres_of(grid[k + 1]);
        for (std::size_t j = 0; j < sub; ++j) {
            const double u = static_cast<double>(j) / static_cast<double>(sub);
            const Vec3 r = hermite_position(a, b, grid_step_s, u);
            const auto t = grid[k].epoch().add(odl::time::Duration::from_seconds(u * grid_step_s));
            const Vec3 sun = sun_m(t);
            s.min_clearance_rad = std::min(s.min_clearance_rad, clearance_rad(r, sun));
            auto nu = odl::shadow::conical(odl::frames::Position<odl::frames::Frame::GCRS>{sun}, odl::frames::Position<odl::frames::Frame::GCRS>{r});
            REQUIRE(nu.has_value());
            s.min_nu = std::min(s.min_nu, nu->fraction);
            if (nu->fraction < 1.0) s.dark_seconds += sample_s;
            const bool lit = nu->fraction >= 1.0;
            if (!first && lit != prev_lit) ++s.edge_crossings;
            prev_lit = lit;
            first = false;
            ++s.samples;
        }
    }
    return s;
}

}  // namespace l7arc
