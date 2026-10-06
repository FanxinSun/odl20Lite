// angle_diurnal_tests.cpp — SPEC-measmod.md §6.2 (vi) and §8.7: MEAS-A-098b, the diurnal-aberration check. REGISTERED (the specification, the sizing tool and the predictions of the controls'
// power) AND COMMITTED as 5de6a45 BEFORE THIS CODE EXISTED; run ONCE, alone, with the log kept: the case carries the tag [pregistered], which an unfiltered run of the binary would execute.
//
// Why it exists: the angle gate's control "without the diurnal aberration's Jacobian A'(·; β_d)" had no power at the geometry the rule selected (a line of sight due north, a station moving
// east: the Jacobian's first-order effect on an angular displacement, the scalar 1 − n̂·β, is zero there; 0.47 ε for the 100 asked). The manager's ruling (plan/subplan_L6/L6-4.md, 7baf3ad):
// its claim — that G1 catches a missing or mis-wired A'(·; β_d) in the azimuth/elevation row — moves to this check, at a closed-form geometry with the observer moving ALONG the line of sight.
//
// The geometry: the identity Earth orientation (GCRS = ITRS axes: up = x, east = y, north = z), a site on the equator at longitude 0, the observer at (6 378 137, 0, 0) m with the inertial
// velocity of the equatorial surface, 465.1 m/s, along the line of sight — toward the target (+β) and then away from it (−β); a static target at azimuth 90°, elevation 30°, ρ = 1.2e6 m;
// the atmosphere of the first real normal point. As β ∥ n̂, A(n̂; ±β) = n̂: the model's row, the row without A' and the row with A'(·; −β) share one base direction and differ in the Jacobian alone.
//
// The criteria, frozen: for the model's row, in both signs, (a) B_pred/10 <= B <= 10 B_pred, (b) |f̂ − a| <= ε(h), (c) the velocity columns exactly 0 and the estimates within ν/h_v; and each
// of the two controls — the row without A', and the row with A' of the wrong sign — fails (b) by at least 10³ ε (predicted 4.5e3 and 9.1e3, written before the run). The sizing is the
// procedure of §6.2 (v) EVALUATED AT THIS GEOMETRY: ν = 3 [ulp(1.5708 rad) + ulp(6.978e6 m)/ρ], F_a(h) = F_pure(h; 30°) + F_extra/ρ³.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/measmod/angles.hpp>

#include "scenario.hpp"
#include "test_data.hpp"
#include "test_tracks.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

using namespace odl::measmod;
using namespace odl::measmod::testing;
using odl::Mat3;
using odl::Vec3;
using odl::time::Epoch;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kC = 299792458.0;
constexpr std::array<double, 5> kHs = {{10.0, 30.0, 100.0, 300.0, 1000.0}};
constexpr std::array<double, 4> kHvs = {{0.01, 0.1, 1.0, 10.0}};
constexpr double kRho = 1.2e6;
constexpr double kAz = kPi / 2.0;                                 // 90 degrees: due east
constexpr double kEl = 30.0 * kPi / 180.0;
constexpr double kSpeed = 7.2921150e-5 * 6378137.0;               // m/s: the equatorial surface speed, 465.1

// the specification's frozen numbers (§6.2 (vi))
constexpr double kNuLit = 2.9944e-15;
constexpr double kFExtraLit = 2.19e-2;                            // F_extra / rho^3
constexpr std::array<double, 5> kEpsLit = {{3.2936e-16, 3.6904e-16, 3.0221e-15, 2.6961e-14, 3.0028e-13}};
constexpr double kBPredLit = 3.0028e-13;
constexpr double kDeltaWithoutLit = 1.4928e-12;                   // rad/m: the largest |row without A' − row|
constexpr double kDeltaReversedLit = 2.9857e-12;
constexpr std::array<double, 5> kPowerWithoutLit = {{4533.0, 4045.0, 494.0, 55.0, 5.0}};      // |Δ|/ε at the five sizes
constexpr std::array<double, 5> kPowerReversedLit = {{9065.0, 8090.0, 988.0, 111.0, 10.0}};
constexpr double kAskedMultiple = 1.0e3;

double ulp_of(double x) { return std::nextafter(std::abs(x), std::numeric_limits<double>::infinity()) - std::abs(x); }
double f_pure_at(double h, double phi) { return 2.0 / (std::pow(kRho - h, 3.0) * std::pow(std::cos(phi + std::asin(h / kRho)), 3.0)); }

using Estimates = std::array<std::array<std::array<double, 5>, 3>, 2>;           // [output][component][size]

/// The diurnal check's scenario: the observer moves along the line of sight at +-465.1 m/s.
class DiurnalScenario {
public:
    explicit DiurnalScenario(double sign)
        : sign_(sign), t_o_(first_real_observation().epoch), los_{std::sin(kEl), std::cos(kEl) * std::sin(kAz), std::cos(kEl) * std::cos(kAz)},     // (up, east, north) -> x, y, z
          station_(t_o_, Vec3{6378137.0, 0.0, 0.0}, (sign * kSpeed) * los_, Vec3{1.0, 0.0, 0.0}), orientation_(Mat3::identity(), Vec3{0.0, 0.0, 7.292115e-5}),
          site_{OpticalStationNumber{9996}, Geodetic{0.0, 0.0, 0.0}, "an equatorial site at longitude 0: the diurnal check's closed-form geometry (SPEC-measmod 6.2 (vi))"},
          r0_(Vec3{6378137.0, 0.0, 0.0} + kRho * los_), t_p_(t_o_) {
        const DriftTrajectory nominal(t_o_, r0_, Vec3{}, Vec3{}, t_o_);
        auto em = solve_emission(t_o_, Vec3{6378137.0, 0.0, 0.0}, nominal);
        REQUIRE(em.has_value());
        t_p_ = em->emission;
    }
    DiurnalScenario(const DiurnalScenario&) = delete;
    DiurnalScenario& operator=(const DiurnalScenario&) = delete;

    [[nodiscard]] double sign() const { return sign_; }
    [[nodiscard]] const Epoch& epoch() const { return t_o_; }
    [[nodiscard]] const Vec3& line_of_sight() const { return los_; }
    [[nodiscard]] const StationTrack& station() const { return station_; }
    [[nodiscard]] const EarthOrientation& orientation() const { return orientation_; }
    [[nodiscard]] const OpticalSite& site() const { return site_; }
    [[nodiscard]] Vec3 target_position() const { return r0_; }
    [[nodiscard]] DriftTrajectory trajectory(Vec3 dr = {}, Vec3 dv = {}) const { return DriftTrajectory(t_o_, r0_, Vec3{}, Vec3{}, t_p_, dr, dv); }
    [[nodiscard]] AngleAtmosphere atmosphere() const {
        const RangeObservation& base = first_real_observation();
        // NOT-A-UNIT-CROSSING: nanometres to micrometres, the wavelength of the CRD record C0 to the refraction model's unit (as in scenario.hpp)
        return AngleAtmosphere{Atmosphere{base.meteorology.pressure_mbar, base.meteorology.temperature_k - 273.15, base.meteorology.relative_humidity_percent / 100.0}, base.wavelength_nm * 1e-3};
    }
    [[nodiscard]] AngleObservation observation() const {
        return AngleObservation{t_o_, AngleKind::AzEl, 0.0, 0.0, std::nullopt, AngleReduction::ApparentRefracted, AngleFrame::Local, Mat3::identity(), site_, atmosphere()};
    }

private:
    double sign_;
    Epoch t_o_;
    Vec3 los_;
    UniformStation station_;
    ConstantOrientation orientation_;
    OpticalSite site_;
    Vec3 r0_;
    Epoch t_p_;
};

std::array<double, 2> evaluate(const DiurnalScenario& sc, const DriftTrajectory& tr) {
    auto m = model_azel(sc.observation(), sc.station(), tr, sc.orientation());
    REQUIRE(m.has_value());
    return {m->azimuth_rad(), m->elevation_rad()};
}

Mat3 outer(const Vec3& a, const Vec3& b) {
    const double av[3] = {a.x, a.y, a.z}, bv[3] = {b.x, b.y, b.z};
    Mat3 m{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = av[i] * bv[j];
    return m;
}
Mat3 minus(const Mat3& a, const Mat3& b) {
    Mat3 m{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = a.r[i][j] - b.r[i][j];
    return m;
}
Mat3 times_scalar(const Mat3& a, double s) {
    Mat3 m{};
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = s * a.r[i][j];
    return m;
}

enum class Jacobian { Model, Without, Reversed };

/// The position part of the azimuth/elevation row assembled from the term functions and the geometric facts of solve_emission — MEAS-R-060's "Angles" paragraph — with the diurnal
/// aberration's Jacobian as the model has it, left out (the identity), or of the wrong sign (A'(·; −β_d)).
std::array<Vec3, 2> assemble_row(const DiurnalScenario& sc, Jacobian which) {
    auto s = sc.station().at(sc.epoch());
    REQUIRE(s.has_value());
    auto em = solve_emission(sc.epoch(), s->position_m, sc.trajectory());
    REQUIRE(em.has_value());
    const Vec3 n = em->direction, v = em->target_velocity_m_s;
    const Mat3 identity = Mat3::identity();
    const Mat3 k = minus(identity, times_scalar(outer(v, n), 1.0 / (kC + n.dot(v))));
    const Mat3 p = minus(identity, outer(n, n));
    const Mat3 jg = times_scalar(p.times(k), 1.0 / em->range_m);
    auto refco = refraction_constants(*sc.observation().atmosphere);
    REQUIRE(refco.has_value());
    const Vec3 beta_d = (1.0 / kC) * s->velocity_m_s;
    const Vec3 n_app = aberration_apply(n, beta_d);
    Mat3 j_red = identity;
    if (which == Jacobian::Model) j_red = aberration_apply_jacobian(n, beta_d);
    if (which == Jacobian::Reversed) j_red = aberration_apply_jacobian(n, -1.0 * beta_d);
    // identity orientation: GCRS -> ITRS is the identity; the local frame of a site at latitude 0, longitude 0
    Mat3 enu{};
    enu.r[0] = {0.0, 1.0, 0.0};          // east
    enu.r[1] = {0.0, 0.0, 1.0};          // north
    enu.r[2] = {1.0, 0.0, 0.0};          // up
    const Vec3 loc = enu.apply(n_app);
    const double h = std::hypot(loc.x, loc.y);
    const double z_v = std::atan2(h, loc.z), z_o = refracted_zenith_distance(z_v, *refco);
    const double dzo = refracted_zenith_derivative(z_o, *refco);
    const Mat3 j = enu.times(j_red).times(jg);
    const Vec3 row_e{j.r[0][0], j.r[0][1], j.r[0][2]}, row_n{j.r[1][0], j.r[1][1], j.r[1][2]}, row_u{j.r[2][0], j.r[2][1], j.r[2][2]};
    const Vec3 g_az = (1.0 / (h * h)) * (loc.y * row_e - loc.x * row_n);
    const Vec3 dh = (1.0 / h) * (loc.x * row_e + loc.y * row_n);
    const Vec3 g_zv = (1.0 / (h * h + loc.z * loc.z)) * (loc.z * dh - h * row_u);
    return {g_az, -dzo * g_zv};
}

double row_difference(const std::array<Vec3, 2>& a, const std::array<Vec3, 2>& b) {
    double worst = 0.0;
    for (std::size_t o = 0; o < 2; ++o) worst = std::max({worst, std::abs(a[o].x - b[o].x), std::abs(a[o].y - b[o].y), std::abs(a[o].z - b[o].z)});
    return worst;
}

double violation(const Estimates& fhat, const std::array<Vec3, 2>& candidate, const std::array<double, 5>& eps) {
    double worst = 0.0;
    for (std::size_t o = 0; o < 2; ++o) {
        const double a[3] = {candidate[o].x, candidate[o].y, candidate[o].z};
        for (std::size_t k = 0; k < 3; ++k)
            for (std::size_t i = 0; i < kHs.size(); ++i) worst = std::max(worst, std::abs(fhat[o][k][i] - a[k]) / eps[i]);
    }
    return worst;
}

double nu_here(const DiurnalScenario& sc) {
    const Vec3 r = sc.target_position();
    return 3.0 * (ulp_of(kAz) + ulp_of(std::max({std::abs(r.x), std::abs(r.y), std::abs(r.z)})) / kRho);
}
double f_a(std::size_t i) { return f_pure_at(kHs[i], kEl) + kFExtraLit / (kRho * kRho * kRho); }
double eps_of(const DiurnalScenario& sc, std::size_t i) { return kHs[i] * kHs[i] * f_a(i) / 6.0 + nu_here(sc) / kHs[i]; }

const char* direction_name(double sign) { return sign > 0.0 ? "toward the target" : "away from the target"; }

}  // namespace

TEST_CASE("MEAS-A-098b  the diurnal-aberration check (SPEC-measmod 6.2 (vi)): G1 for the azimuth/elevation row of ApparentRefracted at a closed-form geometry, the observer moving at 465.1 m/s ALONG "
          "the line of sight toward, and then away from, a static target at azimuth 90 and elevation 30 degrees (identity orientation), against the sizing of (v)'s procedure evaluated at that "
          "geometry: the model's row passes (a), (b), (c); the row without A' and the row with A' of the wrong sign each fail (b) by at least 1e3 epsilon",
          "[measmod][gate][diurnalcheck][pregistered]") {
    SECTION("the frozen figures: the literals and the formulas agree, and the controls' predicted power is what the C++ rows give") {
        for (double sign : {+1.0, -1.0}) {
            const DiurnalScenario sc(sign);
            INFO(direction_name(sign));
            CHECK_THAT(nu_here(sc), WithinRel(kNuLit, 1e-4));
            CHECK_THAT(ulp_of(kAz), WithinRel(std::ldexp(1.0, -52), 1e-12));
            CHECK_THAT(ulp_of(sc.target_position().x), WithinRel(std::ldexp(1.0, -30), 1e-12));
            std::array<double, 5> eps{};
            for (std::size_t i = 0; i < 5; ++i) {
                eps[i] = eps_of(sc, i);
                CHECK_THAT(eps[i], WithinRel(kEpsLit[i], 1e-3));
            }
            CHECK_THAT(*std::max_element(eps.begin(), eps.end()), WithinRel(kBPredLit, 1e-3));
            // the power of the controls at THIS geometry, from the assembled rows alone: no finite difference has been taken
            const std::array<Vec3, 2> model = assemble_row(sc, Jacobian::Model);
            const double d_without = row_difference(assemble_row(sc, Jacobian::Without), model), d_reversed = row_difference(assemble_row(sc, Jacobian::Reversed), model);
            CHECK_THAT(d_without, WithinRel(kDeltaWithoutLit, 1e-2));
            CHECK_THAT(d_reversed, WithinRel(kDeltaReversedLit, 1e-2));
            for (std::size_t i = 0; i < 5; ++i) {
                CHECK_THAT(d_without / eps[i], WithinAbs(kPowerWithoutLit[i], std::max(0.6, 1e-2 * kPowerWithoutLit[i])));
                CHECK_THAT(d_reversed / eps[i], WithinAbs(kPowerReversedLit[i], std::max(0.6, 1e-2 * kPowerReversedLit[i])));
            }
            WARN(std::setprecision(5) << direction_name(sign) << ": predicted power of the controls from the assembled rows: without A' " << d_without / eps[0] << " eps, reversed " << d_reversed / eps[0] << " eps at h = 10 m");
            // the geometry is the one the specification states: the observer's velocity is along the line of sight, so A(n; beta) = n
            auto st = sc.station().at(sc.epoch());
            REQUIRE(st.has_value());
            CHECK_THAT(st->velocity_m_s.norm(), WithinRel(kSpeed, 1e-12));
            CHECK_THAT(st->velocity_m_s.dot(sc.line_of_sight()) / st->velocity_m_s.norm(), WithinAbs(sign, 1e-12));
        }
    }

    SECTION("the check") {
        for (double sign : {+1.0, -1.0}) {
            const DiurnalScenario sc(sign);
            INFO(direction_name(sign));
            std::array<double, 5> eps{};
            for (std::size_t i = 0; i < 5; ++i) eps[i] = eps_of(sc, i);
            const double b_pred = *std::max_element(eps.begin(), eps.end());
            const double nu = nu_here(sc);

            // the model's row, and its reconstruction from the term functions
            auto nominal = model_azel(sc.observation(), sc.station(), sc.trajectory(), sc.orientation());
            REQUIRE(nominal.has_value());
            const std::array<Vec3, 2> analytic = {Vec3{nominal->partials().d[0][0], nominal->partials().d[0][1], nominal->partials().d[0][2]},
                                                  Vec3{nominal->partials().d[1][0], nominal->partials().d[1][1], nominal->partials().d[1][2]}};
            const std::array<Vec3, 2> rebuilt = assemble_row(sc, Jacobian::Model);
            for (std::size_t o = 0; o < 2; ++o) CHECK_THAT((rebuilt[o] - analytic[o]).norm() / analytic[o].norm(), WithinAbs(0.0, 1e-9));
            for (std::size_t o = 0; o < 2; ++o)
                for (std::size_t k = 3; k < 6; ++k) CHECK(nominal->partials().d[o][k] == 0.0);                       // (c): the analytic velocity columns

            // the central differences
            auto y = [&](const Vec3& dr, const Vec3& dv) { return evaluate(sc, sc.trajectory(dr, dv)); };
            auto difference = [&](std::size_t out, const std::array<double, 2>& hi, const std::array<double, 2>& lo, double two_h) {
                const double d = hi[out] - lo[out];
                return (out == 0 ? wrap_to_pi(d) : d) / two_h;
            };
            const Vec3 e[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
            Estimates fhat{};
            double band = 0.0, worst_b = 0.0, worst_c = 0.0;
            std::size_t worst_out = 0, worst_k = 0, worst_i = 0;
            bool inside_raw_spread = true;
            for (std::size_t out = 0; out < 2; ++out) {
                const double a[3] = {analytic[out].x, analytic[out].y, analytic[out].z};
                for (std::size_t k = 0; k < 3; ++k) {
                    double lo = std::numeric_limits<double>::infinity(), hi = -lo;
                    for (std::size_t i = 0; i < kHs.size(); ++i) {
                        const double h = kHs[i];
                        fhat[out][k][i] = difference(out, y(h * e[k], {}), y(-h * e[k], {}), 2.0 * h);
                        lo = std::min(lo, fhat[out][k][i]);
                        hi = std::max(hi, fhat[out][k][i]);
                        const double v = std::abs(fhat[out][k][i] - a[k]) / eps[i];
                        if (v > worst_b) {
                            worst_b = v;
                            worst_out = out;
                            worst_k = k;
                            worst_i = i;
                        }
                    }
                    band = std::max(band, hi - lo);
                    if (a[k] < lo || a[k] > hi) inside_raw_spread = false;
                    for (std::size_t i = 0; i < kHvs.size(); ++i) {
                        const double fv = difference(out, y({}, kHvs[i] * e[k]), y({}, -kHvs[i] * e[k]), 2.0 * kHvs[i]);
                        worst_c = std::max(worst_c, std::abs(fv) / (nu / kHvs[i]));
                    }
                }
            }
            WARN(std::setprecision(4) << direction_name(sign) << ": band B = " << band << " (B_pred " << b_pred << ", ratio " << band / b_pred << "); worst |f̂ − a|/ε = " << worst_b << " (output "
                                      << worst_out << ", component " << worst_k << ", h = " << kHs[worst_i] << " m); worst |f̂_v|/(ν/h_v) = " << worst_c << "; analytic row inside the raw spread: "
                                      << (inside_raw_spread ? "yes" : "no"));
            CHECK(band >= b_pred / 10.0);                       // (a)
            CHECK(band <= b_pred * 10.0);
            CHECK(worst_b <= 1.0);                              // (b)
            CHECK(worst_c <= 1.0);                              // (c)

            // the controls, rule 5 and the ruling's "right sign" half of the wiring claim
            const double v_model = violation(fhat, analytic, eps);
            const double v_without = violation(fhat, assemble_row(sc, Jacobian::Without), eps);
            const double v_reversed = violation(fhat, assemble_row(sc, Jacobian::Reversed), eps);
            WARN(std::setprecision(5) << direction_name(sign) << ": the model's row fails (b) by " << v_model << " ε; control 1 (without A') by " << v_without << " ε (predicted " << kPowerWithoutLit[0]
                                      << ", asked " << kAskedMultiple << "); control 2 (A' of the wrong sign) by " << v_reversed << " ε (predicted " << kPowerReversedLit[0] << ", asked " << kAskedMultiple << ")");
            CHECK(v_model <= 1.0);
            CHECK(v_without >= kAskedMultiple);
            CHECK(v_reversed >= kAskedMultiple);
        }
    }
}
