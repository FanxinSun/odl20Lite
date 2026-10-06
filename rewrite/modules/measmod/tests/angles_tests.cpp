// angles_tests.cpp — SPEC-measmod.md §8.7 (the angle models): MEAS-A-080 … -095 and -089, everything except the finite-difference gate (the gate is angle_gate_tests.cpp, MEAS-A-096 … -099).
//
// The closed-form tests put a station and a target where an answer can be written down (the identity orientation, a station at rest at the equator or in uniform motion, a target
// at a stated azimuth and elevation or in uniform motion); the aberration test (A-090) puts the real ephemeris behind the Earth's motion and builds the same direction a second
// way. The reference values of A-080/-081/-088 come from tools/measmod_reference.py, in 60 digits.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/ephemerides/ephemeris.hpp>
#include <odl/io/iod.hpp>
#include <odl/measmod/angles.hpp>

#include "measmod_reference.hpp"
#include "test_data.hpp"
#include "test_tracks.hpp"

#include <erfa.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <string>
#include <vector>

using namespace odl::measmod;
using namespace odl::measmod::testing;
namespace ref = odl::measmod::ref;
using odl::Mat3;
using odl::Vec3;
using odl::time::Duration;
using odl::time::Epoch;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kC = 299792458.0;
constexpr double kRadPerArcsec = kPi / 648000.0;
constexpr double kRadPerMas = kRadPerArcsec / 1.0e3;     // NOT-A-UNIT-CROSSING: milli-arcseconds in a test's own arithmetic (a test input, not a model conversion)

Vec3 unit(const Vec3& v) { return (1.0 / v.norm()) * v; }
double separation(const Vec3& a, const Vec3& b) { return std::atan2(a.cross(b).norm(), a.dot(b)); }
double max_abs(const Vec3& v) { return std::max({std::abs(v.x), std::abs(v.y), std::abs(v.z)}); }
Vec3 radec_vector(double ra, double dec) { return Vec3{std::cos(dec) * std::cos(ra), std::cos(dec) * std::sin(ra), std::sin(dec)}; }

/// The observer's own site of the tests: a made-up one, cited as a test input.
const OpticalRegistry& sites() {
    static const OpticalRegistry r = [] {
        OpticalRegistry reg;
        auto a = reg.add(OpticalStationNumber{2420}, 0.9, 0.1, 100.0, "a made-up site for the MEAS-A-083/-084/-094 tests (a test input, not a real observer)");
        if (!a) FAIL("the test site did not add: " << a.error().id);
        return reg;
    }();
    return r;
}

/// An IOD line built at IODFMT's own columns, as the iod tests build theirs: the format code (column 45), the epoch code (46; ' ' blank), the 14 angle columns (48-61) and the positional
/// uncertainty (63-64).
odl::io::IodObservation iod_line(char format, char epoch_code, const std::string& angles14, const std::string& pos_unc = "25", const std::string& station = "2420",
                                 const std::string& date = "20260115", const std::string& hhmm = "2213", const std::string& ss = "45678") {
    std::string line(80, ' ');
    auto put = [&](int first, const std::string& s) {
        for (std::size_t i = 0; i < s.size(); ++i) line[static_cast<std::size_t>(first - 1) + i] = s[i];
    };
    put(1, "12345 98067A");
    put(17, station);
    put(24, date);
    put(32, hhmm);
    put(36, ss);
    line[44] = format;
    line[45] = epoch_code;
    put(48, angles14);
    put(63, pos_unc);
    auto o = odl::io::read_iod(line);
    if (!o) FAIL("the test IOD line did not read: " << o.error().id << " " << o.error().message);
    return *o;
}

/// The standard atmosphere of the tests: 1013.25 hPa, 15 °C, 50 % humidity, green light.
AngleAtmosphere standard_atmosphere() { return AngleAtmosphere{Atmosphere{1013.25, 15.0, 0.5}, 0.55}; }
/// "Vacuum": a pressure so small that the refraction is below 1e-18 rad.
AngleAtmosphere vacuum_atmosphere() { return AngleAtmosphere{Atmosphere{1.0e-9, 15.0, 0.0}, 0.55}; }

AngleObservation make_observation(const Epoch& t, AngleKind kind, AngleReduction reduction, double a, double b, const OpticalSite& site, std::optional<AngleAtmosphere> atm = std::nullopt,
                                  AngleFrame frame = AngleFrame::Icrf, const Mat3& to_frame = Mat3::identity()) {
    return AngleObservation{t, kind, a, b, std::nullopt, reduction, kind == AngleKind::AzEl ? AngleFrame::Local : frame, to_frame, site, atm};
}

OpticalSite equator_site() { return OpticalSite{OpticalStationNumber{9999}, Geodetic{0.0, 0.0, 0.0}, "a test site on the equator at longitude 0"}; }

/// A station at the equator at longitude 0 under the IDENTITY orientation: GCRS axes = ITRS axes, up = +x, east = +y, north = +z.
struct EquatorFrame {
    Vec3 position{6378137.0, 0.0, 0.0};
    [[nodiscard]] Vec3 direction(double az, double el) const { return Vec3{std::sin(el), std::cos(el) * std::sin(az), std::cos(el) * std::cos(az)}; }   // (up, east, north) -> x, y, z
};

}  // namespace

// ---- MEAS-A-080 -------------------------------------------------------------------------------------------------------------------------------------------------------

namespace {

/// The first draft's inverse, kept as a control (PROVENANCE §39.5): the sign of the (n·β)/(1 + 1/γ) term not flipped.
Vec3 wrong_inverse(const Vec3& n, const Vec3& beta) {
    const double inv_gamma = std::sqrt(1.0 - beta.dot(beta)), nb = n.dot(beta);
    return (1.0 / (1.0 - nb)) * (inv_gamma * n - (1.0 + nb / (1.0 + inv_gamma)) * beta);
}

std::vector<Vec3> direction_grid() {
    std::vector<Vec3> g;
    for (double polar_deg : {10.0, 50.0, 90.0, 130.0, 170.0})
        for (double az_deg = 0.0; az_deg < 360.0; az_deg += 45.0) {
            const double th = polar_deg * kPi / 180.0, ph = az_deg * kPi / 180.0;
            g.push_back(Vec3{std::sin(th) * std::cos(ph), std::sin(th) * std::sin(ph), std::cos(th)});
        }
    return g;
}

std::vector<Vec3> beta_grid() {
    std::vector<Vec3> g;
    const std::array<Vec3, 5> dirs = {Vec3{1, 0, 0}, Vec3{0, 1, 0}, Vec3{0, 0, 1}, unit(Vec3{1, 1, 1}), unit(Vec3{1, -2, 0.5})};
    for (double mag : {1.0e-6, 1.0e-4, 1.0e-3})
        for (const Vec3& d : dirs) g.push_back(mag * d);
    return g;
}

}  // namespace

TEST_CASE("MEAS-A-080  the aberration A(n; beta) and its exact inverse D = A(n; -beta): A(D(n)) = n and D(A(n)) = n to 1e-15 on a grid of directions and beta up to 1e-3; beta = 0 is "
          "the identity; A keeps the unit norm; the first-order shift is beta - (n.beta) n with error at most beta^2 / 2 and the second-order term is written out; the Jacobians match "
          "differences of A and D; the 60-digit references agree; the wrong sign in D fails the inverse test",
          "[measmod][angles]") {
    const std::vector<Vec3> dirs = direction_grid(), betas = beta_grid();
    double worst_inverse = 0.0, worst_inverse2 = 0.0, worst_norm = 0.0, worst_first = 0.0, worst_second = 0.0;
    double worst_wrong = std::numeric_limits<double>::infinity();
    int wrong_checked = 0;

    for (const Vec3& n : dirs) {
        for (const Vec3& b : betas) {
            const double bm = b.norm(), nb = n.dot(b);
            const Vec3 a = aberration_apply(n, b), d = aberration_remove(n, b);
            worst_inverse = std::max(worst_inverse, max_abs(aberration_apply(d, b) - n));
            worst_inverse2 = std::max(worst_inverse2, max_abs(aberration_remove(a, b) - n));
            worst_norm = std::max(worst_norm, std::abs(a.norm() - 1.0));
            // first order and second order of A (the expansion of the formula, written out in this test from the specification's A)
            const Vec3 first = b - nb * n;
            const Vec3 err1 = a - n - first;
            worst_first = std::max(worst_first, (err1.norm() - 0.5001 * bm * bm - 4.0e-16) / (bm * bm));
            const Vec3 t2 = (nb * nb - 0.5 * bm * bm) * n - (0.5 * nb) * b;
            worst_second = std::max(worst_second, (max_abs(err1 - t2) - 0.26 * bm * bm * bm - 4.0e-16) / (bm * bm * bm));
            // the first draft's inverse: A(wrong_D(n)) - n is about (n.beta) beta
            if (std::abs(nb) * bm >= 1.0e-12) {
                const double e = (aberration_apply(wrong_inverse(n, b), b) - n).norm();      // the NORM: the error lies along beta, whose largest component is a fraction of it
                worst_wrong = std::min(worst_wrong, e / (std::abs(nb) * bm));
                ++wrong_checked;
            }
        }
    }
    INFO("worst A(D(n)) - n: " << worst_inverse << ", D(A(n)) - n: " << worst_inverse2);
    CHECK(worst_inverse <= 1.0e-15);
    CHECK(worst_inverse2 <= 1.0e-15);
    CHECK(worst_norm <= 5.0e-16);
    CHECK(worst_first <= 0.0);        // |A - n - beta_perp| <= 0.5001 beta^2 (+ 4e-16): the specification's "error beta^2" with its factor 1/2
    CHECK(worst_second <= 0.0);       // |A - n - beta_perp - T2| <= 0.26 beta^3 (+ 4e-16), T2 = n ((n.b)^2 - b^2/2) - (n.b) b / 2
    REQUIRE(wrong_checked > 100);
    INFO("the wrong inverse leaves A(wrong_D(n)) - n at " << worst_wrong << " of (n.beta) beta at its least");
    CHECK(worst_wrong >= 0.9);

    SECTION("beta = 0 is the identity, exactly") {
        for (const Vec3& n : dirs) {
            const Vec3 z{};
            CHECK(aberration_apply(n, z).x == n.x);
            CHECK(aberration_apply(n, z).y == n.y);
            CHECK(aberration_apply(n, z).z == n.z);
            CHECK(aberration_remove(n, z).z == n.z);
        }
    }

    SECTION("the wrong sign in D fails the inverse test (rule 5), at a stated case") {
        const Vec3 n{ref::ab_oblique_n_x, ref::ab_oblique_n_y, ref::ab_oblique_n_z}, b{ref::ab_oblique_beta_x, ref::ab_oblique_beta_y, ref::ab_oblique_beta_z};
        const double right = max_abs(aberration_apply(aberration_remove(n, b), b) - n);
        const double wrong = max_abs(aberration_apply(wrong_inverse(n, b), b) - n);
        INFO("the correct inverse leaves " << right << ", the first draft's " << wrong);
        CHECK(right <= 1.0e-15);
        CHECK(wrong >= 1.0e-9);        // (n.beta) beta = 8.4e-9, 8 million times the tolerance
    }

    SECTION("the 60-digit references: A and D at four cases, to 1e-15") {
        struct Case { Vec3 n, b, a, d; };
        const std::array<Case, 4> cases = {
            Case{{ref::ab_annual_n_x, ref::ab_annual_n_y, ref::ab_annual_n_z}, {ref::ab_annual_beta_x, ref::ab_annual_beta_y, ref::ab_annual_beta_z},
                 {ref::ab_annual_a_x, ref::ab_annual_a_y, ref::ab_annual_a_z}, {ref::ab_annual_d_x, ref::ab_annual_d_y, ref::ab_annual_d_z}},
            Case{{ref::ab_oblique_n_x, ref::ab_oblique_n_y, ref::ab_oblique_n_z}, {ref::ab_oblique_beta_x, ref::ab_oblique_beta_y, ref::ab_oblique_beta_z},
                 {ref::ab_oblique_a_x, ref::ab_oblique_a_y, ref::ab_oblique_a_z}, {ref::ab_oblique_d_x, ref::ab_oblique_d_y, ref::ab_oblique_d_z}},
            Case{{ref::ab_large_n_x, ref::ab_large_n_y, ref::ab_large_n_z}, {ref::ab_large_beta_x, ref::ab_large_beta_y, ref::ab_large_beta_z},
                 {ref::ab_large_a_x, ref::ab_large_a_y, ref::ab_large_a_z}, {ref::ab_large_d_x, ref::ab_large_d_y, ref::ab_large_d_z}},
            Case{{ref::ab_diurnal_n_x, ref::ab_diurnal_n_y, ref::ab_diurnal_n_z}, {ref::ab_diurnal_beta_x, ref::ab_diurnal_beta_y, ref::ab_diurnal_beta_z},
                 {ref::ab_diurnal_a_x, ref::ab_diurnal_a_y, ref::ab_diurnal_a_z}, {ref::ab_diurnal_d_x, ref::ab_diurnal_d_y, ref::ab_diurnal_d_z}}};
        for (const Case& c : cases) {
            CHECK(max_abs(aberration_apply(c.n, c.b) - c.a) <= 1.0e-15);
            CHECK(max_abs(aberration_remove(c.n, c.b) - c.d) <= 1.0e-15);
        }
    }

    SECTION("the Jacobians equal central differences of A and D (the term functions the rows are built from)") {
        const std::array<std::pair<Vec3, Vec3>, 3> cases = {std::make_pair(Vec3{ref::ab_oblique_n_x, ref::ab_oblique_n_y, ref::ab_oblique_n_z}, Vec3{3.0e-4, -4.0e-4, 5.0e-4}),
                                                           std::make_pair(Vec3{ref::ab_annual_n_x, ref::ab_annual_n_y, ref::ab_annual_n_z}, Vec3{0.0, 9.9e-5, 0.0}),
                                                           std::make_pair(Vec3{ref::ab_large_n_x, ref::ab_large_n_y, ref::ab_large_n_z}, Vec3{-1.0e-4, 2.0e-4, 1.0e-4})};
        const double eps = 1.0e-6;
        for (const auto& [n, b] : cases) {
            const Vec3 t1 = unit(n.cross(Vec3{0.3, -0.5, 0.8})), t2 = n.cross(t1);       // two unit vectors tangent to the sphere at n
            for (const Vec3& t : {t1, t2}) {
                const Mat3 ja = aberration_apply_jacobian(n, b), jd = aberration_remove_jacobian(n, b);
                const Vec3 fa = (1.0 / (2.0 * eps)) * (aberration_apply(n + eps * t, b) - aberration_apply(n - eps * t, b));
                const Vec3 fd = (1.0 / (2.0 * eps)) * (aberration_remove(n + eps * t, b) - aberration_remove(n - eps * t, b));
                CHECK(max_abs(ja.apply(t) - fa) <= 1.0e-8);
                CHECK(max_abs(jd.apply(t) - fd) <= 1.0e-8);
            }
        }
    }
}

// ---- MEAS-A-081 -------------------------------------------------------------------------------------------------------------------------------------------------------

TEST_CASE("MEAS-A-081  A agrees with ERFA's eraAb with the solar term switched off (a Sun distance of 1e30 au) to 1e-15, on the grid", "[measmod][angles]") {
    double worst = 0.0;
    for (const Vec3& n : direction_grid()) {
        for (const Vec3& b : beta_grid()) {
            double pnat[3] = {n.x, n.y, n.z}, v[3] = {b.x, b.y, b.z};      // ERFA's prototype takes non-const arrays
            double ppr[3];
            eraAb(pnat, v, 1.0e30, std::sqrt(1.0 - b.dot(b)), ppr);
            worst = std::max(worst, max_abs(aberration_apply(n, b) - Vec3{ppr[0], ppr[1], ppr[2]}));
        }
    }
    INFO("worst difference from eraAb: " << worst);
    CHECK(worst <= 1.0e-15);
}

// ---- MEAS-A-082 -------------------------------------------------------------------------------------------------------------------------------------------------------

namespace {
template <class... Args>
concept CanBuildAngleObservation = requires(Args&&... a) { angle_observation(std::forward<Args>(a)...); };
}

TEST_CASE("MEAS-A-082  no default reduction: angle_observation cannot be called without the reduction argument (a requires-expression), and a reduction that does not fit the kind "
          "refuses MEAS-F-015, both ways",
          "[measmod][angles]") {
    using odl::io::IodObservation;
    using odl::time::LeapTable;
    STATIC_REQUIRE(CanBuildAngleObservation<const IodObservation&, const OpticalRegistry&, AngleReduction, std::optional<EquinoxOfDate>, std::optional<AngleAtmosphere>, const LeapTable&>);
    STATIC_REQUIRE_FALSE(CanBuildAngleObservation<const IodObservation&, const OpticalRegistry&, std::optional<EquinoxOfDate>, std::optional<AngleAtmosphere>, const LeapTable&>);
    STATIC_REQUIRE_FALSE(CanBuildAngleObservation<const IodObservation&, const OpticalRegistry&, const LeapTable&>);
    STATIC_REQUIRE_FALSE(CanBuildAngleObservation<const IodObservation&, const OpticalRegistry&>);
    STATIC_REQUIRE_FALSE(CanBuildAngleObservation<const IodObservation&>);
    STATIC_REQUIRE_FALSE(CanBuildAngleObservation<const IodObservation&, const OpticalRegistry&, std::nullopt_t, std::nullopt_t, const LeapTable&>);

    const auto radec = iod_line('2', '5', "0917234+123456");
    const auto azel = iod_line('5', ' ', "1803000+450000", "25");
    for (AngleReduction bad : {AngleReduction::ApparentRefracted}) {
        auto r = angle_observation(radec, sites(), bad, std::nullopt, standard_atmosphere(), leaps());
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-015");
    }
    for (AngleReduction bad : {AngleReduction::Astrometric, AngleReduction::Geometric}) {
        auto r = angle_observation(azel, sites(), bad, std::nullopt, standard_atmosphere(), leaps());
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "MEAS-F-015");
    }
    // the fitting ones pass
    CHECK(angle_observation(radec, sites(), AngleReduction::Astrometric, std::nullopt, std::nullopt, leaps()).has_value());
    CHECK(angle_observation(radec, sites(), AngleReduction::Geometric, std::nullopt, std::nullopt, leaps()).has_value());
    CHECK(angle_observation(azel, sites(), AngleReduction::ApparentRefracted, std::nullopt, standard_atmosphere(), leaps()).has_value());
}

// ---- MEAS-A-083 -------------------------------------------------------------------------------------------------------------------------------------------------------

TEST_CASE("MEAS-A-083  the IOD angle decoder through the builder: the seven formats against values written out by hand; blanks are zero; the sign of the declination and the elevation; "
          "the MX uncertainty field; right ascension and azimuth in [0, 2pi)",
          "[measmod][angles]") {
    const double deg = kPi / 180.0;
    struct Row { char format; const char* angles; double first_deg, second_deg; };
    const std::array<Row, 8> rows = {
        Row{'1', "1234567+123456", (12.0 + 34.0 / 60.0 + 56.7 / 3600.0) * 15.0, 12.0 + 34.0 / 60.0 + 56.0 / 3600.0},          // RA HH MM SS.s, Dec DD MM SS
        Row{'2', "1234567-123456", (12.0 + 34.567 / 60.0) * 15.0, -(12.0 + 34.56 / 60.0)},                                   // RA HH MM.mmm, Dec DD MM.mm
        Row{'3', "1234567+451234", (12.0 + 34.567 / 60.0) * 15.0, 45.1234},                                                   // RA HH MM.mmm, Dec DD.dddd
        Row{'7', "0102034-051234", (1.0 + 2.0 / 60.0 + 3.4 / 3600.0) * 15.0, -5.1234},                                       // RA HH MM SS.s, Dec DD.dddd
        Row{'4', "1801530+450000", 180.0 + 15.0 / 60.0 + 30.0 / 3600.0, 45.0},                                                // Az DDD MM SS, El DD MM SS
        Row{'5', "2703050-203015", 270.0 + 30.0 / 60.0 + 50.0 / 6000.0, -(20.0 + 30.0 / 60.0 + 15.0 / 6000.0)},               // Az DDD MM.mm, El DD MM.mm
        Row{'6', "0905000+300000", 90.5, 30.0},                                                                               // Az DDD.dddd, El DD.dddd
        Row{'2', "0000000+000000", 0.0, 0.0}};                                                                                // zeros
    for (const Row& r : rows) {
        const bool azel = r.format == '4' || r.format == '5' || r.format == '6';
        auto iod = iod_line(r.format, azel ? ' ' : '5', r.angles);
        auto obs = angle_observation(iod, sites(), azel ? AngleReduction::ApparentRefracted : AngleReduction::Geometric, std::nullopt, azel ? std::optional(standard_atmosphere()) : std::nullopt, leaps());
        INFO("format " << r.format << " " << r.angles);
        REQUIRE(obs.has_value());
        CHECK(obs->kind == (azel ? AngleKind::AzEl : AngleKind::RaDec));
        CHECK_THAT(obs->a_rad, WithinAbs(r.first_deg * deg, 1.0e-15));
        CHECK_THAT(obs->b_rad, WithinAbs(r.second_deg * deg, 1.0e-15));
        CHECK(obs->a_rad >= 0.0);
        CHECK(obs->a_rad < 2.0 * kPi);
    }

    SECTION("blanks are zeros") {
        auto iod = iod_line('2', '5', "12  567+12  56");     // minutes blank in both coordinates
        auto obs = angle_observation(iod, sites(), AngleReduction::Geometric, std::nullopt, std::nullopt, leaps());
        REQUIRE(obs.has_value());
        CHECK_THAT(obs->a_rad, WithinAbs((12.0 + 0.567 / 60.0) * 15.0 * deg, 1.0e-15));
        CHECK_THAT(obs->b_rad, WithinAbs((12.0 + 0.56 / 60.0) * deg, 1.0e-15));
    }

    SECTION("the MX uncertainty field: M x 10^(X-8) in the format's own unit, returned in radians") {
        // format 2 (arcminutes): "25" = 2 x 10^-3 arcminutes; format 1 (arcseconds): "34" = 3 x 10^-4 arcseconds; format 3 (degrees): "98" = 9 degrees
        auto o2 = angle_observation(iod_line('2', '5', "0917234+123456", "25"), sites(), AngleReduction::Geometric, std::nullopt, std::nullopt, leaps());
        auto o1 = angle_observation(iod_line('1', '5', "0917234+123456", "34"), sites(), AngleReduction::Geometric, std::nullopt, std::nullopt, leaps());
        auto o3 = angle_observation(iod_line('3', '5', "0917234+121234", "98"), sites(), AngleReduction::Geometric, std::nullopt, std::nullopt, leaps());
        auto o0 = angle_observation(iod_line('2', '5', "0917234+123456", "  "), sites(), AngleReduction::Geometric, std::nullopt, std::nullopt, leaps());
        REQUIRE((o2.has_value() && o1.has_value() && o3.has_value() && o0.has_value()));
        REQUIRE(o2->sigma_rad.has_value());
        REQUIRE(o1->sigma_rad.has_value());
        REQUIRE(o3->sigma_rad.has_value());
        CHECK_THAT(*o2->sigma_rad, WithinRel(2.0e-3 * kPi / 10800.0, 1.0e-14));
        CHECK_THAT(*o1->sigma_rad, WithinRel(3.0e-4 * kRadPerArcsec, 1.0e-14));
        CHECK_THAT(*o3->sigma_rad, WithinRel(9.0 * deg, 1.0e-14));
        CHECK_FALSE(o0->sigma_rad.has_value());     // two blanks: not reported
    }

    SECTION("a station number the registry does not hold refuses MEAS-F-005; an angle error keeps the decoder's own id") {
        auto missing = angle_observation(iod_line('2', '5', "0917234+123456", "25", "9876"), sites(), AngleReduction::Geometric, std::nullopt, std::nullopt, leaps());
        REQUIRE_FALSE(missing.has_value());
        CHECK(missing.error().id == "MEAS-F-005");
        auto bad = angle_observation(iod_line('2', '5', "2517234+123456"), sites(), AngleReduction::Geometric, std::nullopt, std::nullopt, leaps());     // 25 hours
        REQUIRE_FALSE(bad.has_value());
        CHECK(bad.error().id == "IOFM-F-016");
    }
}

// ---- MEAS-A-084 -------------------------------------------------------------------------------------------------------------------------------------------------------

namespace {

Vec3 ecliptic_direction(double lon_rad, double lat_rad) {
    const double eps = 84381.406 * kRadPerArcsec;     // the IAU 2006 mean obliquity at J2000
    const double x = std::cos(lat_rad) * std::cos(lon_rad), y = std::cos(lat_rad) * std::sin(lon_rad), z = std::sin(lat_rad);
    return Vec3{x, y * std::cos(eps) - z * std::sin(eps), y * std::sin(eps) + z * std::cos(eps)};
}

/// The angle of a small rotation, from the antisymmetric part of its matrix (which does not lose it to the cosine).
double rotation_angle(const Mat3& m) {
    const double x = m.r[2][1] - m.r[1][2], y = m.r[0][2] - m.r[2][0], z = m.r[1][0] - m.r[0][1];
    return std::asin(0.5 * std::sqrt(x * x + y * y + z * z));
}

}  // namespace

TEST_CASE("MEAS-A-084  the epoch-code policy: code 5 is the ICRF; 1, 2, 3, 4, 6 refuse MEAS-F-016; 0 and blank refuse without a declared equinox and are accepted with the mean or the true "
          "equinox, rotating by ERFA's bias-precession or bias-precession-nutation matrix at the observation epoch; the sizes: 1307 arcseconds (within 1 %) at 2026.0, 23 mas at J2000.0",
          "[measmod][angles]") {
    const std::string raw = "0917234+123456";
    SECTION("code 5 is read as the ICRF, with the identity rotation") {
        auto o = angle_observation(iod_line('2', '5', raw), sites(), AngleReduction::Astrometric, std::nullopt, std::nullopt, leaps());
        REQUIRE(o.has_value());
        CHECK(o->frame == AngleFrame::Icrf);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) CHECK(o->gcrs_to_frame.r[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] == (i == j ? 1.0 : 0.0));
    }
    SECTION("codes 1, 2, 3, 4 and 6 refuse MEAS-F-016, each with the year in the message") {
        const std::array<std::pair<char, const char*>, 5> codes = {std::make_pair('1', "1855"), std::make_pair('2', "1875"), std::make_pair('3', "1900"), std::make_pair('4', "1950"), std::make_pair('6', "2050")};
        for (const auto& [code, year] : codes) {
            auto o = angle_observation(iod_line('2', code, raw), sites(), AngleReduction::Astrometric, std::nullopt, std::nullopt, leaps());
            INFO("epoch code " << code);
            REQUIRE_FALSE(o.has_value());
            CHECK(o.error().id == "MEAS-F-016");
            CHECK(o.error().message.find(year) != std::string::npos);
            // a declared equinox does not rescue a refused code
            auto o2 = angle_observation(iod_line('2', code, raw), sites(), AngleReduction::Astrometric, EquinoxOfDate::Mean, std::nullopt, leaps());
            REQUIRE_FALSE(o2.has_value());
            CHECK(o2.error().id == "MEAS-F-016");
        }
    }
    SECTION("code 0 and a blank code refuse without a declared equinox, and are accepted with one") {
        for (char code : {'0', ' '}) {
            auto none = angle_observation(iod_line('2', code, raw), sites(), AngleReduction::Astrometric, std::nullopt, std::nullopt, leaps());
            REQUIRE_FALSE(none.has_value());
            CHECK(none.error().id == "MEAS-F-016");
            auto mean = angle_observation(iod_line('2', code, raw), sites(), AngleReduction::Astrometric, EquinoxOfDate::Mean, std::nullopt, leaps());
            auto tru = angle_observation(iod_line('2', code, raw), sites(), AngleReduction::Astrometric, EquinoxOfDate::True, std::nullopt, leaps());
            REQUIRE((mean.has_value() && tru.has_value()));
            CHECK(mean->frame == AngleFrame::MeanOfDate);
            CHECK(tru->frame == AngleFrame::TrueOfDate);
        }
    }
    SECTION("the matrices are ERFA's, and the sizes") {
        // an epoch of 2026.0 (UTC 2026-01-01 00:00:00) and J2000.0 (TT 2000-01-01 12:00:00 = UTC 11:58:55.816)
        const Epoch t2026 = utc(2026, 1, 1, 0, 0, 0.0), t2000 = utc(2000, 1, 1, 11, 58, 55.816);
        const std::array<std::pair<Epoch, const char*>, 2> epochs = {std::make_pair(t2026, "20260101"), std::make_pair(t2000, "20000101")};
        for (const auto& [t, date] : epochs) {
            const bool is2000 = std::string(date) == "20000101";
            auto o = angle_observation(iod_line('2', '0', raw, "25", "2420", date, is2000 ? "1158" : "0000", is2000 ? "55816" : "00000"), sites(), AngleReduction::Geometric, EquinoxOfDate::Mean, std::nullopt, leaps());
            REQUIRE(o.has_value());
            CHECK(std::abs(o->epoch.difference(t).to_seconds()) < 1.0e-6);
            auto jd = t.two_part_jd(odl::time::TimeScale::TT, leaps());
            REQUIRE(jd.has_value());
            double r[3][3];
            eraPmat06(jd->day, jd->fraction, r);
            double worst = 0.0;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j) worst = std::max(worst, std::abs(o->gcrs_to_frame.r[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] - r[i][j]));
            CHECK(worst <= 1.0e-15);
        }
        // 2026.0: a direction on the ecliptic moves by the general precession in longitude, 50.29 arcseconds a year for 26 years, and one at 45 degrees of ecliptic latitude by its cosine
        for (double lat_deg : {0.0, 45.0}) {
            auto o = angle_observation(iod_line('2', '0', raw, "25", "2420", "20260101", "0000", "00000"), sites(), AngleReduction::Geometric, EquinoxOfDate::Mean, std::nullopt, leaps());
            REQUIRE(o.has_value());
            const Vec3 n = ecliptic_direction(100.0 * kPi / 180.0, lat_deg * kPi / 180.0);
            const double moved = separation(n, o->gcrs_to_frame.apply(n)) / kRadPerArcsec;
            const double expected = 50.29 * 26.0 * std::cos(lat_deg * kPi / 180.0);
            INFO("ecliptic latitude " << lat_deg << ": moved " << moved << " arcseconds, expected " << expected);
            CHECK_THAT(moved, WithinRel(expected, 0.01));
        }
        // J2000.0: the frame bias, sqrt(xi0^2 + eta0^2 + dalpha0^2) = 23.15 mas
        auto o = angle_observation(iod_line('2', '0', raw, "25", "2420", "20000101", "1158", "55816"), sites(), AngleReduction::Geometric, EquinoxOfDate::Mean, std::nullopt, leaps());
        REQUIRE(o.has_value());
        const double bias_mas = rotation_angle(o->gcrs_to_frame) / kRadPerMas;
        INFO("the rotation at J2000.0: " << bias_mas << " mas");
        CHECK(bias_mas > 22.9);
        CHECK(bias_mas < 23.4);
    }
    SECTION("the true equinox adds the nutation, a few to ten arcseconds, to the mean one at 2026.0") {
        auto mean = angle_observation(iod_line('2', '0', raw, "25", "2420", "20260101", "0000", "00000"), sites(), AngleReduction::Geometric, EquinoxOfDate::Mean, std::nullopt, leaps());
        auto tru = angle_observation(iod_line('2', '0', raw, "25", "2420", "20260101", "0000", "00000"), sites(), AngleReduction::Geometric, EquinoxOfDate::True, std::nullopt, leaps());
        REQUIRE((mean.has_value() && tru.has_value()));
        const Vec3 n = ecliptic_direction(100.0 * kPi / 180.0, 0.0);
        const double nut = separation(mean->gcrs_to_frame.apply(n), tru->gcrs_to_frame.apply(n)) / kRadPerArcsec;
        INFO("true minus mean of date: " << nut << " arcseconds");
        CHECK(nut > 1.0);
        CHECK(nut < 25.0);
    }
    SECTION("the model applies the rotation: the right ascension and declination of an of-date observation are those of the rotated geometric direction") {
        const Epoch t = utc(2026, 1, 15, 22, 13, 45.678);
        auto jd = t.two_part_jd(odl::time::TimeScale::TT, leaps());
        REQUIRE(jd.has_value());
        double r[3][3];
        eraPnm06a(jd->day, jd->fraction, r);
        Mat3 m{};
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = r[i][j];
        const OpticalSite site = sites().site(OpticalStationNumber{2420}).value();
        const UniformStation station(t, Vec3{6378137.0, 0.0, 0.0}, Vec3{});
        const Vec3 target_pos = Vec3{6378137.0, 0.0, 0.0} + 1.2e6 * unit(Vec3{0.3, -0.5, 0.8});
        const LinearTrajectory target(t, target_pos, Vec3{});
        const ConstantEarthMotion motion(Vec3{}, Vec3{});
        auto icrf = model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::Geometric, 0.0, 0.0, site), station, target, motion);
        auto of_date = model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::Geometric, 0.0, 0.0, site, std::nullopt, AngleFrame::TrueOfDate, m), station, target, motion);
        REQUIRE((icrf.has_value() && of_date.has_value()));
        const Vec3 expected = m.apply(radec_vector(icrf->ra_rad(), icrf->dec_rad()));
        CHECK(max_abs(radec_vector(of_date->ra_rad(), of_date->dec_rad()) - expected) <= 2.0e-15);
        CHECK(of_date->applied().frame == AngleFrame::TrueOfDate);
    }
}

// ---- MEAS-A-085 and -086 -----------------------------------------------------------------------------------------------------------------------------------------------

TEST_CASE("MEAS-A-085  the refraction: A and B are eraRefco's (ERFA's own published case, and a direct call); the solved observed zenith distance satisfies z_v = z_o + A tan z_o + "
          "B tan^3 z_o to 1e-14; zero at zero; monotonic; about an arcminute at 45 degrees; dz_o/dz_v is the derivative of the solution",
          "[measmod][angles]") {
    SECTION("ERFA's own published case: 800 hPa, 10 C, 90 %, 0.4 micrometres") {
        auto c = refraction_constants(AngleAtmosphere{Atmosphere{800.0, 10.0, 0.9}, 0.4});
        REQUIRE(c.has_value());
        CHECK_THAT(c->a, WithinAbs(0.2264949956241415009e-3, 1.0e-15));      // t_erfa_c.c's own tolerances
        CHECK_THAT(c->b, WithinAbs(-0.2598658261729343970e-6, 1.0e-18));
    }
    const AngleAtmosphere atm = standard_atmosphere();
    auto c = refraction_constants(atm);
    REQUIRE(c.has_value());
    SECTION("a direct eraRefco call for the supplied atmosphere gives the same constants, exactly") {
        double a = 0.0, b = 0.0;
        eraRefco(atm.atmosphere.pressure_hpa, atm.atmosphere.temperature_c, atm.atmosphere.relative_humidity, atm.wavelength_um, &a, &b);
        CHECK(c->a == a);
        CHECK(c->b == b);
        CHECK(a > 2.5e-4);
        CHECK(a < 3.1e-4);
    }
    SECTION("the equation, to 1e-14, over zenith distances 0 to 74.9 degrees") {
        double worst = 0.0;
        for (double z_deg = 0.0; z_deg <= 74.9; z_deg += 2.5) {
            const double z_v = z_deg * kPi / 180.0, z_o = refracted_zenith_distance(z_v, *c), t = std::tan(z_o);
            worst = std::max(worst, std::abs(z_o + c->a * t + c->b * t * t * t - z_v));
        }
        const double z_v = 74.9 * kPi / 180.0, z_o = refracted_zenith_distance(z_v, *c), t = std::tan(z_o);
        worst = std::max(worst, std::abs(z_o + c->a * t + c->b * t * t * t - z_v));
        INFO("worst residual of the equation: " << worst);
        CHECK(worst <= 1.0e-14);
    }
    SECTION("zero at zero, monotonic in zenith distance, an arcminute at 45 degrees") {
        CHECK(refracted_zenith_distance(0.0, *c) == 0.0);
        double last = 0.0;
        for (double z_deg = 1.0; z_deg <= 74.0; z_deg += 1.0) {
            const double z_v = z_deg * kPi / 180.0, r = z_v - refracted_zenith_distance(z_v, *c);
            CHECK(r > last);
            last = r;
        }
        const double r45 = (kPi / 4.0 - refracted_zenith_distance(kPi / 4.0, *c)) / kRadPerArcsec;
        INFO("refraction at 45 degrees: " << r45 << " arcseconds");
        CHECK(r45 > 50.0);
        CHECK(r45 < 70.0);
    }
    SECTION("dz_o/dz_v is the derivative of the solved z_o") {
        for (double z_deg : {10.0, 30.0, 50.0, 70.0}) {
            const double z_v = z_deg * kPi / 180.0, h = 1.0e-6;
            const double fd = (refracted_zenith_distance(z_v + h, *c) - refracted_zenith_distance(z_v - h, *c)) / (2.0 * h);
            CHECK_THAT(refracted_zenith_derivative(refracted_zenith_distance(z_v, *c), *c), WithinAbs(fd, 1.0e-9));
        }
    }
}

TEST_CASE("MEAS-A-086  refusals: a vacuum zenith distance above 75 degrees refuses MEAS-F-010 (74.9 accepted, 75.1 refuses); atmospheres with pressure <= 0, wavelength <= 0, "
          "temperature -100.1 or 60.1 C, humidity -0.01 or 1.01 refuse MEAS-F-022 and their accepted neighbours pass",
          "[measmod][angles]") {
    const Epoch t = utc(2026, 1, 15, 22, 13, 45.678);
    const EquatorFrame frame;
    const UniformStation station(t, frame.position, Vec3{}, Vec3{1.0, 0.0, 0.0});
    const ConstantOrientation orientation(Mat3::identity(), Vec3{0.0, 0.0, 7.292115e-5});
    auto at_zenith_distance = [&](double z_deg) {
        const double el = (90.0 - z_deg) * kPi / 180.0;
        const LinearTrajectory target(t, frame.position + 1.0e6 * frame.direction(0.3, el), Vec3{});
        return model_azel(make_observation(t, AngleKind::AzEl, AngleReduction::ApparentRefracted, 0.0, 0.0, equator_site(), standard_atmosphere()), station, target, orientation);
    };
    SECTION("F-010 at 75 degrees of vacuum zenith distance") {
        auto ok = at_zenith_distance(74.9);
        REQUIRE(ok.has_value());
        CHECK_THAT(ok->applied().zenith_vacuum_rad, WithinAbs(74.9 * kPi / 180.0, 1.0e-12));
        auto bad = at_zenith_distance(75.1);
        REQUIRE_FALSE(bad.has_value());
        CHECK(bad.error().id == "MEAS-F-010");
        auto below = at_zenith_distance(100.0);        // below the horizon
        REQUIRE_FALSE(below.has_value());
        CHECK(below.error().id == "MEAS-F-010");
    }
    SECTION("F-022 for every non-physical atmosphere, and the neighbours pass") {
        struct Case { double p, t, rh, wl; bool ok; };
        const std::array<Case, 18> cases = {
            Case{1013.25, 15.0, 0.5, 0.55, true},   Case{0.0, 15.0, 0.5, 0.55, false},    Case{-1.0, 15.0, 0.5, 0.55, false},   Case{1.0e-6, 15.0, 0.5, 0.55, true},
            Case{1013.25, 15.0, 0.5, 0.0, false},   Case{1013.25, 15.0, 0.5, -0.5, false}, Case{1013.25, 15.0, 0.5, 1.0e-3, true},
            Case{1013.25, -100.1, 0.5, 0.55, false}, Case{1013.25, -100.0, 0.5, 0.55, true}, Case{1013.25, 60.0, 0.5, 0.55, true},   Case{1013.25, 60.1, 0.5, 0.55, false},
            Case{1013.25, 15.0, -0.01, 0.55, false}, Case{1013.25, 15.0, 0.0, 0.55, true},   Case{1013.25, 15.0, 1.0, 0.55, true},    Case{1013.25, 15.0, 1.01, 0.55, false},
            Case{std::numeric_limits<double>::quiet_NaN(), 15.0, 0.5, 0.55, false}, Case{1013.25, 15.0, 0.5, std::numeric_limits<double>::infinity(), false},
            Case{1013.25, std::numeric_limits<double>::quiet_NaN(), 0.5, 0.55, false}};
        for (const Case& c : cases) {
            const AngleAtmosphere atm{Atmosphere{c.p, c.t, c.rh}, c.wl};
            INFO("p " << c.p << " t " << c.t << " rh " << c.rh << " wl " << c.wl);
            auto k = refraction_constants(atm);
            CHECK(k.has_value() == c.ok);
            if (!k) CHECK(k.error().id == "MEAS-F-022");
            // the builder says the same
            auto o = angle_observation(iod_line('5', ' ', "1801530+450000"), sites(), AngleReduction::ApparentRefracted, std::nullopt, atm, leaps());
            CHECK(o.has_value() == c.ok);
            if (!o) CHECK(o.error().id == "MEAS-F-022");
        }
        // an azimuth and elevation line without an atmosphere refuses the same way
        auto none = angle_observation(iod_line('5', ' ', "1801530+450000"), sites(), AngleReduction::ApparentRefracted, std::nullopt, std::nullopt, leaps());
        REQUIRE_FALSE(none.has_value());
        CHECK(none.error().id == "MEAS-F-022");
    }
}

// ---- MEAS-A-087 -------------------------------------------------------------------------------------------------------------------------------------------------------

TEST_CASE("MEAS-A-087  the diurnal aberration of ApparentRefracted: toward the observer's velocity (east for a station at rest in the Earth) by 0.32 arcsec x sin(theta), to 1e-9 rad of the "
          "first-order value; no shift along the velocity",
          "[measmod][angles]") {
    const Epoch t = utc(2026, 1, 15, 22, 13, 45.678);
    const EquatorFrame frame;
    const ConstantOrientation orientation(Mat3::identity(), Vec3{0.0, 0.0, 7.292115e-5});
    const double beta_east = 7.292115e-5 * 6378137.0 / kC;      // the equatorial station's v/c, 1.551e-6
    const double el = 45.0 * kPi / 180.0;
    const Vec3 los = frame.direction(0.0, el);                    // due north at 45 degrees: perpendicular to the (east) velocity

    SECTION("a station at rest in the Earth at the equator, a target due north at 45 degrees: displaced east by beta, theta = 90 degrees") {
        const UniformStation station(t, frame.position, Vec3{0.0, 7.292115e-5 * 6378137.0, 0.0}, Vec3{1.0, 0.0, 0.0});
        const LinearTrajectory target(t, frame.position + 1.0e6 * los, Vec3{});
        auto m = model_azel(make_observation(t, AngleKind::AzEl, AngleReduction::ApparentRefracted, 0.0, 0.0, equator_site(), vacuum_atmosphere()), station, target, orientation);
        REQUIRE(m.has_value());
        CHECK_THAT(m->applied().aberration_shift_rad, WithinAbs(beta_east, 1.0e-9));            // 0.32 arcsec x sin 90
        CHECK(m->applied().aberration_shift_rad > 0.31 * kRadPerArcsec);
        CHECK(m->applied().aberration_shift_rad < 0.33 * kRadPerArcsec);
        // east is positive azimuth: the azimuth shift is beta / cos(el), the elevation unchanged to second order
        CHECK_THAT(wrap_to_pi(m->azimuth_rad()), WithinAbs(beta_east / std::cos(el), 1.0e-9));
        CHECK_THAT(m->elevation_rad(), WithinAbs(el, 1.0e-9));
        CHECK_THAT(m->applied().aberration_beta.y, WithinAbs(beta_east, 1.0e-15));
    }
    SECTION("the shift is beta sin(theta) for the angle theta between the line of sight and the velocity") {
        // a station moving at 465.1 m/s in the direction `dir`; the target at 45 degrees elevation due north; theta from the dot product
        for (const Vec3& dir : {Vec3{0.0, 1.0, 0.0}, unit(Vec3{0.0, 1.0, 1.0}), unit(Vec3{1.0, 1.0, 0.0}), unit(Vec3{0.0, -1.0, 1.0})}) {
            const double speed = 7.292115e-5 * 6378137.0;
            const UniformStation station(t, frame.position, speed * dir, Vec3{1.0, 0.0, 0.0});
            const LinearTrajectory target(t, frame.position + 1.0e6 * los, Vec3{});
            auto m = model_azel(make_observation(t, AngleKind::AzEl, AngleReduction::ApparentRefracted, 0.0, 0.0, equator_site(), vacuum_atmosphere()), station, target, orientation);
            REQUIRE(m.has_value());
            const double theta = std::acos(los.dot(dir));
            INFO("theta = " << theta * 180.0 / kPi << " degrees");
            CHECK_THAT(m->applied().aberration_shift_rad, WithinAbs(beta_east * std::sin(theta), 1.0e-9));
        }
    }
    SECTION("no shift along the velocity (a station moving along its own line of sight)") {
        const UniformStation station(t, frame.position, 465.1 * los, Vec3{1.0, 0.0, 0.0});
        const LinearTrajectory target(t, frame.position + 1.0e6 * los, Vec3{});
        auto m = model_azel(make_observation(t, AngleKind::AzEl, AngleReduction::ApparentRefracted, 0.0, 0.0, equator_site(), vacuum_atmosphere()), station, target, orientation);
        REQUIRE(m.has_value());
        CHECK(m->applied().aberration_shift_rad <= 1.0e-12);
        CHECK_THAT(wrap_to_pi(m->azimuth_rad()), WithinAbs(0.0, 1.0e-12));
        CHECK_THAT(m->elevation_rad(), WithinAbs(el, 1.0e-12));
    }
}

// ---- MEAS-A-088 -------------------------------------------------------------------------------------------------------------------------------------------------------

TEST_CASE("MEAS-A-088  the emission epoch and the geometric direction in closed form: a target in uniform motion seen from an observer in uniform motion, solve_emission equals the "
          "closed-form root of the light-time quadratic to 1e-15 s and the Geometric right ascension and declination equal atan2 and asin of the closed-form direction to 1e-15 rad; "
          "the observer's velocity does not enter",
          "[measmod][angles]") {
    struct Case { const char* name; Vec3 s0, vs, r0, vr; double tau, rho; Vec3 n; };
    const std::array<Case, 2> cases = {
        Case{"leo", {ref::lt_leo_s0_x, ref::lt_leo_s0_y, ref::lt_leo_s0_z}, {ref::lt_leo_vs_x, ref::lt_leo_vs_y, ref::lt_leo_vs_z}, {ref::lt_leo_r0_x, ref::lt_leo_r0_y, ref::lt_leo_r0_z},
             {ref::lt_leo_vr_x, ref::lt_leo_vr_y, ref::lt_leo_vr_z}, ref::em_leo_tau_s, ref::em_leo_rho_m, {ref::em_leo_n_x, ref::em_leo_n_y, ref::em_leo_n_z}},
        Case{"lageos", {ref::lt_lageos_s0_x, ref::lt_lageos_s0_y, ref::lt_lageos_s0_z}, {ref::lt_lageos_vs_x, ref::lt_lageos_vs_y, ref::lt_lageos_vs_z},
             {ref::lt_lageos_r0_x, ref::lt_lageos_r0_y, ref::lt_lageos_r0_z}, {ref::lt_lageos_vr_x, ref::lt_lageos_vr_y, ref::lt_lageos_vr_z}, ref::em_lageos_tau_s, ref::em_lageos_rho_m,
             {ref::em_lageos_n_x, ref::em_lageos_n_y, ref::em_lageos_n_z}}};
    const Epoch t = utc(2026, 1, 15, 22, 13, 45.678);
    for (const Case& c : cases) {
        INFO(c.name);
        const LinearTrajectory target(t, c.r0, c.vr);                        // the target's position AT THE OBSERVATION epoch is r0
        auto em = solve_emission(t, c.s0, target);
        REQUIRE(em.has_value());
        CHECK_THAT(em->light_time_s, WithinAbs(c.tau, 1.0e-15));
        CHECK_THAT(em->range_m, WithinRel(c.rho, 1.0e-14));
        CHECK(max_abs(em->direction - c.n) <= 1.0e-15);
        CHECK(std::abs(t.difference(em->emission).to_seconds() - c.tau) <= 1.0e-15);
        CHECK(em->passes >= 1);
        CHECK(em->passes <= 6);
        // the target's own state at emission, from the closed form r0 - v tau
        CHECK(max_abs(em->target_position_m - (c.r0 - c.tau * c.vr)) <= 1.0e-6);
        CHECK(max_abs(em->target_velocity_m_s - c.vr) <= 1.0e-12);

        // the model: the observer in uniform motion (its velocity is the closed form's `vs`), Geometric
        const UniformStation station(t, c.s0, c.vs);
        const ConstantEarthMotion motion(Vec3{}, Vec3{});
        const OpticalSite site = equator_site();
        auto m = model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::Geometric, 0.0, 0.0, site), station, target, motion);
        REQUIRE(m.has_value());
        double ra = std::atan2(c.n.y, c.n.x);
        if (ra < 0.0) ra += 2.0 * kPi;
        CHECK_THAT(m->ra_rad(), WithinAbs(ra, 1.0e-15));
        CHECK_THAT(m->dec_rad(), WithinAbs(std::asin(c.n.z), 1.0e-15));
        CHECK(m->emission_tt() == em->emission);
        CHECK_THAT(m->applied().light_time_s, WithinAbs(c.tau, 1.0e-15));
        CHECK(m->applied().reduction == AngleReduction::Geometric);
        CHECK(m->applied().aberration_shift_rad == 0.0);
    }
    SECTION("a target at or beyond the speed of light does not converge: MEAS-F-011, and the last iterate is not returned") {
        const LinearTrajectory runaway(t, Vec3{1.0e7, 0.0, 0.0}, Vec3{1.2 * kC, 0.0, 0.0});
        auto em = solve_emission(t, Vec3{}, runaway);
        REQUIRE_FALSE(em.has_value());
        CHECK(em.error().id == "MEAS-F-011");
    }
}

// ---- MEAS-A-089 -------------------------------------------------------------------------------------------------------------------------------------------------------

TEST_CASE("MEAS-A-089  the observer, the Earth's orientation and the Earth's motion are evaluated at the observation epoch and nowhere else, whatever the target's trajectory (the "
          "stencil's variations of 1000 m and 10 m/s), in every reduction: the structure that makes the chain's floor cancel in the angle gate's difference",
          "[measmod][angles]") {
    const Epoch t = utc(2026, 1, 15, 22, 13, 45.678);
    const EquatorFrame frame;
    const UniformStation base_station(t, frame.position, Vec3{0.0, 465.1, 0.0}, Vec3{1.0, 0.0, 0.0});
    const ConstantOrientation base_orientation(Mat3::identity(), Vec3{0.0, 0.0, 7.292115e-5});
    const ConstantEarthMotion base_motion(Vec3{1.0e11, 5.0e10, 2.0e10}, Vec3{-1.5e4, 2.5e4, 1.1e4});
    const Vec3 r0 = frame.position + 1.2e6 * frame.direction(0.7, 35.0 * kPi / 180.0), v0 = Vec3{-2000.0, 5000.0, 4000.0}, a0 = (-3.986004415e14 / std::pow(r0.norm(), 3)) * r0;

    std::vector<Epoch> emissions;
    for (int variation = 0; variation < 13; ++variation) {
        Vec3 dr{}, dv{};
        if (variation > 0) {
            const int axis = (variation - 1) % 3;
            const double sign = variation <= 6 ? 1.0 : -1.0;
            const bool velocity = ((variation - 1) / 3) % 2 == 1;
            Vec3& d = velocity ? dv : dr;
            (axis == 0 ? d.x : axis == 1 ? d.y : d.z) = sign * (velocity ? 10.0 : 1000.0);
        }
        const DriftTrajectory target(t, r0, v0, a0, t.add(Duration::from_seconds(-0.004)), dr, dv);

        for (AngleReduction red : {AngleReduction::Geometric, AngleReduction::Astrometric, AngleReduction::ApparentRefracted}) {
            RecordingStation station(base_station);
            RecordingOrientation orientation(base_orientation);
            RecordingMotion motion(base_motion);
            INFO("variation " << variation << ", reduction " << static_cast<int>(red));
            if (red == AngleReduction::ApparentRefracted) {
                auto m = model_azel(make_observation(t, AngleKind::AzEl, red, 0.0, 0.0, equator_site(), standard_atmosphere()), station, target, orientation);
                REQUIRE(m.has_value());
                if (variation == 0 || variation == 1 || variation == 7) emissions.push_back(m->emission_tt());
            } else {
                auto m = model_radec(make_observation(t, AngleKind::RaDec, red, 0.0, 0.0, equator_site()), station, target, motion);
                REQUIRE(m.has_value());
            }
            for (const Epoch& e : station.asked()) CHECK(e == t);
            for (const Epoch& e : orientation.asked()) CHECK(e == t);
            for (const Epoch& e : motion.asked()) CHECK(e == t);
            // each is asked, and asked once (Geometric asks no orientation and no motion; Astrometric the motion; ApparentRefracted the orientation)
            CHECK(station.asked().size() == 1);
            CHECK(orientation.asked().size() == (red == AngleReduction::ApparentRefracted ? 1u : 0u));
            CHECK(motion.asked().size() == (red == AngleReduction::Astrometric ? 1u : 0u));
        }
    }
    // the target's own epoch DOES move with the variation: a displacement of 1000 m along the line of sight shifts the emission epoch by 3.3 microseconds
    REQUIRE(emissions.size() == 3);
    CHECK(emissions[0] != emissions[1]);
    CHECK(emissions[0] != emissions[2]);
    CHECK(std::abs(emissions[1].difference(emissions[2]).to_seconds()) > 1.0e-8);
}

// ---- MEAS-A-090 and -091: the aberration against an independent construction ----------------------------------------------------------------------------------------------

namespace {

const odl::eph::Ephemeris& ephemeris() {
    static const auto e = odl::eph::Ephemeris::open({ODL_DE440S_BSP}, {});
    if (!e) FAIL("de440s did not open: " << e.error().id << " " << e.error().message);
    return *e;
}

Vec3 earth_position_m(const Epoch& t) {
    auto s = ephemeris().barycentric_state(odl::eph::Body::Earth, t, leaps());
    if (!s) FAIL("the Earth's barycentric state: " << s.error().id << " " << s.error().message);
    return odl::metres_from_km(s->position());
}

Vec3 earth_velocity_m_s(const Epoch& t) {
    auto s = ephemeris().barycentric_state(odl::eph::Body::Earth, t, leaps());
    if (!s) FAIL("the Earth's barycentric state: " << s.error().id << " " << s.error().message);
    return odl::metres_from_km(s->velocity());
}

/// The direction from the observer's BARYCENTRIC position at the observation epoch to the target's barycentric position at its light-time-corrected emission epoch, the Earth's barycentric
/// position taken from the ephemeris at both epochs (MEAS-R-053's "equivalent, independent construction"): no aberration function enters.
Vec3 barycentric_direction(const Epoch& t_o, const Vec3& observer_gcrs_m, const Trajectory& target) {
    const Vec3 b_obs = earth_position_m(t_o) + observer_gcrs_m;
    Vec3 b_target = b_obs;
    double tau = 0.0;
    {
        auto s = target.state_at(t_o);
        tau = ((odl::metres_from_km(s->position()) - observer_gcrs_m).norm()) / kC;
    }
    for (int pass = 0; pass < 12; ++pass) {
        const Epoch t_e = t_o.add(Duration::from_seconds(-tau));
        auto s = target.state_at(t_e);
        b_target = earth_position_m(t_e) + odl::metres_from_km(s->position());
        tau = (b_target - b_obs).norm() / kC;
    }
    return unit(b_target - b_obs);
}

/// Twelve lines of sight at 70, 90 and 110 degrees from the Earth's velocity and four azimuths about it (chosen by geometry alone, from the ephemeris's velocity).
std::vector<Vec3> lines_of_sight(const Vec3& vhat) {
    const Vec3 a = unit(vhat.cross(Vec3{0.0, 0.0, 1.0})), b = vhat.cross(a);
    std::vector<Vec3> out;
    for (double theta_deg : {70.0, 90.0, 110.0})
        for (double phi_deg : {0.0, 90.0, 180.0, 270.0}) {
            const double th = theta_deg * kPi / 180.0, ph = phi_deg * kPi / 180.0;
            out.push_back(std::sin(th) * (std::cos(ph) * a + std::sin(ph) * b) + std::cos(th) * vhat);
        }
    return out;
}

}  // namespace

TEST_CASE("MEAS-A-090  the aberration test (ruling R5): the Astrometric direction against an independent construction (the direction from the observer's barycentric position to the target's "
          "light-time-corrected barycentric position, the Earth from the ephemeris at both epochs) agrees to 4.1 mas; the bare GCRS direction fails the same assertion by 10 arcseconds or more",
          "[measmod][angles][aberration]") {
    const double tolerance = 4.1 * kRadPerMas;
    const std::array<Epoch, 2> epochs = {utc(2026, 1, 1, 2, 7, 56.8005871), utc(2026, 7, 4, 12, 0, 0.0)};
    const OpticalSite site = equator_site();
    double worst_astrometric = 0.0, least_bare = std::numeric_limits<double>::infinity(), most_bare = 0.0;
    int cases = 0;
    for (const Epoch& t : epochs) {
        const EphemerisEarthMotion motion(ephemeris(), leaps());
        const Vec3 vhat = unit(earth_velocity_m_s(t));
        const Vec3 station_position = 6378137.0 * unit(Vec3{0.2, -0.9, 0.4});
        const UniformStation station(t, station_position, Vec3{});
        for (const Vec3& los : lines_of_sight(vhat)) {
            const Vec3 target_position = station_position + 1.2e6 * los;
            const Vec3 target_velocity = 7500.0 * unit(los.cross(Vec3{0.3, 0.8, -0.5}));
            const LinearTrajectory target(t, target_position, target_velocity);
            auto astrometric = model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::Astrometric, 0.0, 0.0, site), station, target, motion);
            auto geometric = model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::Geometric, 0.0, 0.0, site), station, target, motion);
            REQUIRE((astrometric.has_value() && geometric.has_value()));
            const Vec3 expected = barycentric_direction(t, station_position, target);
            const double e_astrometric = separation(radec_vector(astrometric->ra_rad(), astrometric->dec_rad()), expected);
            const double e_bare = separation(radec_vector(geometric->ra_rad(), geometric->dec_rad()), expected);
            worst_astrometric = std::max(worst_astrometric, e_astrometric);
            least_bare = std::min(least_bare, e_bare);
            most_bare = std::max(most_bare, e_bare);
            ++cases;
            CHECK(e_astrometric <= tolerance);
            CHECK(e_bare >= 10.0 * kRadPerArcsec);       // the control: the same assertion with no aberration fails by this much
        }
    }
    REQUIRE(cases == 24);
    WARN("MEAS-A-090: worst Astrometric disagreement " << worst_astrometric / kRadPerMas << " mas (tolerance 4.1 mas); the bare GCRS direction is off by " << least_bare / kRadPerArcsec << " to "
         << most_bare / kRadPerArcsec << " arcseconds");
    CHECK(least_bare >= 18.0 * kRadPerArcsec);
    CHECK(most_bare <= 20.9 * kRadPerArcsec);
}

TEST_CASE("MEAS-A-091  Astrometric minus Geometric is -beta_perp to first order (error beta^2/2), at most beta_E (20.15 arcsec at the July aphelion, 20.84 at the January perihelion), "
          "zero along the Earth's velocity; with the ephemeris's own velocity at two epochs and a synthetic one",
          "[measmod][angles][aberration]") {
    const OpticalSite site = equator_site();
    const std::array<Epoch, 2> epochs = {utc(2026, 1, 1, 2, 7, 56.8005871), utc(2026, 7, 4, 12, 0, 0.0)};
    double max_shift[2] = {0.0, 0.0};
    for (std::size_t k = 0; k < 2; ++k) {
        const Epoch& t = epochs[k];
        const Vec3 v_earth = earth_velocity_m_s(t), beta = (1.0 / kC) * v_earth, vhat = unit(v_earth);
        const ConstantEarthMotion motion(earth_position_m(t), v_earth);
        const UniformStation station(t, Vec3{6378137.0, 0.0, 0.0}, Vec3{});
        std::vector<Vec3> sky = lines_of_sight(vhat);
        sky.push_back(vhat);
        sky.push_back(-1.0 * vhat);
        sky.push_back(unit(vhat + 0.5 * unit(vhat.cross(Vec3{0.0, 0.0, 1.0}))));
        sky.push_back(unit(vhat - 2.0 * unit(vhat.cross(Vec3{0.0, 0.0, 1.0}))));
        for (const Vec3& los : sky) {
            const LinearTrajectory target(t, Vec3{6378137.0, 0.0, 0.0} + 1.2e6 * los, Vec3{});
            auto astrometric = model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::Astrometric, 0.0, 0.0, site), station, target, motion);
            auto geometric = model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::Geometric, 0.0, 0.0, site), station, target, motion);
            REQUIRE((astrometric.has_value() && geometric.has_value()));
            const Vec3 na = radec_vector(astrometric->ra_rad(), astrometric->dec_rad()), ng = radec_vector(geometric->ra_rad(), geometric->dec_rad());
            const Vec3 perp = beta - ng.dot(beta) * ng;
            const double bm = beta.norm();
            CHECK((na - ng + perp).norm() <= 0.5001 * bm * bm + 1.0e-15);
            const double shift = separation(na, ng);
            max_shift[k] = std::max(max_shift[k], shift);
            CHECK(shift <= bm * 1.0000001);
            if (std::abs(ng.dot(vhat)) > 1.0 - 1.0e-12) CHECK(shift <= 2.0e-15);     // along the velocity: nothing
            CHECK_THAT(astrometric->applied().aberration_shift_rad, WithinAbs(shift, 2.0e-15));      // the model's own record of the same angle
        }
        // the figure of merit: the shift of a perpendicular line of sight is |beta|, the Earth's speed over c
        CHECK_THAT(max_shift[k], WithinRel(beta.norm(), 1.0e-7));
    }
    const double january = max_shift[0] / kRadPerArcsec, july = max_shift[1] / kRadPerArcsec;
    WARN("MEAS-A-091: the largest Astrometric - Geometric shift is " << january << " arcseconds in January and " << july << " in July (the mean-speed figure of MEAS-P-8 is 20.49)");
    CHECK(january > 20.49);
    CHECK(january < 20.9);
    CHECK(july > 20.0);
    CHECK(july < 20.49);

    SECTION("a synthetic velocity: the shift is its perpendicular part, whatever the direction") {
        const Epoch t = epochs[0];
        const UniformStation station(t, Vec3{6378137.0, 0.0, 0.0}, Vec3{});
        const Vec3 v{-1.0e4, 2.0e4, 0.8e4};
        const ConstantEarthMotion motion(Vec3{}, v);
        for (const Vec3& los : direction_grid()) {
            const LinearTrajectory target(t, Vec3{6378137.0, 0.0, 0.0} + 1.2e6 * los, Vec3{});
            auto astrometric = model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::Astrometric, 0.0, 0.0, site), station, target, motion);
            auto geometric = model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::Geometric, 0.0, 0.0, site), station, target, motion);
            REQUIRE((astrometric.has_value() && geometric.has_value()));
            const Vec3 na = radec_vector(astrometric->ra_rad(), astrometric->dec_rad()), ng = radec_vector(geometric->ra_rad(), geometric->dec_rad());
            const Vec3 beta = (1.0 / kC) * v;
            CHECK((na - ng + (beta - ng.dot(beta) * ng)).norm() <= 0.5001 * beta.dot(beta) + 1.0e-15);
        }
    }
}

// ---- MEAS-A-092 and -093 -----------------------------------------------------------------------------------------------------------------------------------------------

TEST_CASE("MEAS-A-092  the local-frame algebra of ApparentRefracted with a constant EarthOrientation: due north at 45 degrees gives azimuth 0 and elevation 45 (vacuum); due east gives 90; "
          "south 180, west 270; near the zenith the elevation is 90 degrees less the offset; at the zenith it refuses MEAS-F-026",
          "[measmod][angles]") {
    const Epoch t = utc(2026, 1, 15, 22, 13, 45.678);
    const EquatorFrame frame;
    const UniformStation station(t, frame.position, Vec3{}, Vec3{1.0, 0.0, 0.0});          // at rest: no diurnal aberration
    const ConstantOrientation orientation(Mat3::identity(), Vec3{0.0, 0.0, 7.292115e-5});
    auto model_at = [&](double az_deg, double el_deg) {
        const LinearTrajectory target(t, frame.position + 1.0e6 * frame.direction(az_deg * kPi / 180.0, el_deg * kPi / 180.0), Vec3{});
        return model_azel(make_observation(t, AngleKind::AzEl, AngleReduction::ApparentRefracted, 0.0, 0.0, equator_site(), vacuum_atmosphere()), station, target, orientation);
    };
    struct Case { double az, el; };
    for (const Case& c : {Case{0.0, 45.0}, Case{90.0, 45.0}, Case{180.0, 45.0}, Case{270.0, 45.0}, Case{90.0, 30.0}, Case{225.0, 60.0}, Case{315.0, 20.0}, Case{0.0, 89.0}}) {
        INFO("azimuth " << c.az << " elevation " << c.el);
        auto m = model_at(c.az, c.el);
        REQUIRE(m.has_value());
        CHECK_THAT(wrap_to_pi(m->azimuth_rad() - c.az * kPi / 180.0), WithinAbs(0.0, 1.0e-12));
        CHECK_THAT(m->elevation_rad(), WithinAbs(c.el * kPi / 180.0, 1.0e-12));
        CHECK(m->azimuth_rad() >= 0.0);
        CHECK(m->azimuth_rad() < 2.0 * kPi);
    }
    SECTION("a target 1e-6 rad from the zenith: elevation 90 degrees less the offset, a finite partial of the expected size") {
        const double off = 1.0e-6;
        auto m = model_at(60.0, 90.0 - off * 180.0 / kPi);
        REQUIRE(m.has_value());
        CHECK_THAT(m->elevation_rad(), WithinAbs(kPi / 2.0 - off, 1.0e-12));
        for (std::size_t row = 0; row < 2; ++row)
            for (std::size_t k = 0; k < 6; ++k) CHECK(std::isfinite(m->partials().d[row][k]));
        const double az_partial = std::sqrt(m->partials().d[0][0] * m->partials().d[0][0] + m->partials().d[0][1] * m->partials().d[0][1] + m->partials().d[0][2] * m->partials().d[0][2]);
        CHECK_THAT(az_partial, WithinRel(1.0 / (1.0e6 * off), 1.0e-3));       // 1 / (rho sin z)
    }
    SECTION("at the zenith, and within 1e-9 rad of it, the azimuth is undefined: MEAS-F-026; at 2e-9 rad it is accepted") {
        const LinearTrajectory exact(t, frame.position + Vec3{1.0e6, 0.0, 0.0}, Vec3{});
        auto m0 = model_azel(make_observation(t, AngleKind::AzEl, AngleReduction::ApparentRefracted, 0.0, 0.0, equator_site(), vacuum_atmosphere()), station, exact, orientation);
        REQUIRE_FALSE(m0.has_value());
        CHECK(m0.error().id == "MEAS-F-026");
        const LinearTrajectory near(t, frame.position + Vec3{1.0e6, 0.0, 0.5e-9 * 1.0e6}, Vec3{});
        auto m1 = model_azel(make_observation(t, AngleKind::AzEl, AngleReduction::ApparentRefracted, 0.0, 0.0, equator_site(), vacuum_atmosphere()), station, near, orientation);
        REQUIRE_FALSE(m1.has_value());
        CHECK(m1.error().id == "MEAS-F-026");
        const LinearTrajectory beyond(t, frame.position + Vec3{1.0e6, 0.0, 2.0e-9 * 1.0e6}, Vec3{});
        CHECK(model_azel(make_observation(t, AngleKind::AzEl, AngleReduction::ApparentRefracted, 0.0, 0.0, equator_site(), vacuum_atmosphere()), station, beyond, orientation).has_value());
    }
    SECTION("the celestial pole of a right-ascension and declination observation refuses MEAS-F-026 in the same way") {
        const ConstantEarthMotion motion(Vec3{}, Vec3{});
        const LinearTrajectory pole(t, frame.position + Vec3{0.0, 0.0, 1.0e6}, Vec3{});
        auto m = model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::Geometric, 0.0, 0.0, equator_site()), station, pole, motion);
        REQUIRE_FALSE(m.has_value());
        CHECK(m.error().id == "MEAS-F-026");
        const LinearTrajectory south(t, frame.position + Vec3{0.0, 0.0, -1.0e6}, Vec3{});
        auto m2 = model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::Geometric, 0.0, 0.0, equator_site()), station, south, motion);
        REQUIRE_FALSE(m2.has_value());
        CHECK(m2.error().id == "MEAS-F-026");
        const LinearTrajectory near(t, frame.position + Vec3{1.0e5, 0.0, 1.0e6}, Vec3{});
        CHECK(model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::Geometric, 0.0, 0.0, equator_site()), station, near, motion).has_value());
    }
    SECTION("wrong kinds and a missing atmosphere refuse before anything is computed") {
        const LinearTrajectory target(t, frame.position + 1.0e6 * frame.direction(0.0, 0.7), Vec3{});
        const ConstantEarthMotion motion(Vec3{}, Vec3{});
        auto a = model_azel(make_observation(t, AngleKind::RaDec, AngleReduction::Geometric, 0.0, 0.0, equator_site()), station, target, orientation);
        REQUIRE_FALSE(a.has_value());
        CHECK(a.error().id == "MEAS-F-015");
        auto b = model_radec(make_observation(t, AngleKind::AzEl, AngleReduction::ApparentRefracted, 0.0, 0.0, equator_site(), standard_atmosphere()), station, target, motion);
        REQUIRE_FALSE(b.has_value());
        CHECK(b.error().id == "MEAS-F-015");
        auto c = model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::ApparentRefracted, 0.0, 0.0, equator_site()), station, target, motion);
        REQUIRE_FALSE(c.has_value());
        CHECK(c.error().id == "MEAS-F-015");
        auto d = model_azel(make_observation(t, AngleKind::AzEl, AngleReduction::ApparentRefracted, 0.0, 0.0, equator_site()), station, target, orientation);
        REQUIRE_FALSE(d.has_value());
        CHECK(d.error().id == "MEAS-F-022");
        auto e = model_azel(make_observation(t, AngleKind::AzEl, AngleReduction::ApparentRefracted, std::numeric_limits<double>::quiet_NaN(), 0.0, equator_site(), standard_atmosphere()), station, target, orientation);
        REQUIRE_FALSE(e.has_value());
        CHECK(e.error().id == "MEAS-F-018");
    }
}

TEST_CASE("MEAS-A-093  the same model with the production EopEarthOrientation and EarthFixedStation agrees with the closed-form substitutes where it must: a station on the rotation axis "
          "(no diurnal aberration) and one on the equator (a diurnal shift of v/c, east), to 1e-9 rad",
          "[measmod][angles]") {
    const EopEarthOrientation orientation(c04(), odl::eop::EopPolicy{}, leaps());
    const Epoch t = utc(2026, 1, 1, 2, 7, 56.8005871);
    auto sample = orientation.at(t);
    REQUIRE(sample.has_value());
    const Mat3 to_gcrs = sample->gcrs_to_itrs.transpose();
    const double el = 45.0 * kPi / 180.0;

    SECTION("a station on the rotation axis: the pole, up = the ITRS z axis, the target due 'north' of the conventional frame at longitude 0") {
        const double b_polar = 6378137.0 * (1.0 - 1.0 / 298.257223563);
        const OpticalSite site{OpticalStationNumber{9998}, Geodetic{kPi / 2.0, 0.0, 0.0}, "the North Pole, a test site"};
        const EarthFixedStation station(Vec3{0.0, 0.0, b_polar}, site.location, orientation);
        auto k = station.at(t);
        REQUIRE(k.has_value());
        CHECK(k->velocity_m_s.norm() < 1.0e-2);                // on the axis to within the polar motion: no diurnal aberration
        // at longitude 0 and latitude 90 degrees the local north is the ITRS -x axis, east +y, up +z
        const Vec3 dir_itrs = std::cos(el) * Vec3{-1.0, 0.0, 0.0} + std::sin(el) * Vec3{0.0, 0.0, 1.0};
        const LinearTrajectory target(t, k->position_m + 1.0e6 * to_gcrs.apply(dir_itrs), Vec3{});
        auto m = model_azel(make_observation(t, AngleKind::AzEl, AngleReduction::ApparentRefracted, 0.0, 0.0, site, vacuum_atmosphere()), station, target, orientation);
        REQUIRE(m.has_value());
        CHECK_THAT(wrap_to_pi(m->azimuth_rad()), WithinAbs(0.0, 1.0e-9));
        CHECK_THAT(m->elevation_rad(), WithinAbs(el, 1.0e-9));
        CHECK(m->applied().aberration_shift_rad < 1.0e-9);
    }
    SECTION("a station on the equator at longitude 0: the apparent direction is displaced east by v/c, v = omega a") {
        const OpticalSite site{OpticalStationNumber{9997}, Geodetic{0.0, 0.0, 0.0}, "an equatorial test site"};
        const EarthFixedStation station(Vec3{6378137.0, 0.0, 0.0}, site.location, orientation);
        auto k = station.at(t);
        REQUIRE(k.has_value());
        // the target at azimuth 0, elevation 45 degrees: up = ITRS +x, north = +z
        const Vec3 dir_itrs = std::cos(el) * Vec3{0.0, 0.0, 1.0} + std::sin(el) * Vec3{1.0, 0.0, 0.0};
        const LinearTrajectory target(t, k->position_m + 1.0e6 * to_gcrs.apply(dir_itrs), Vec3{});
        auto m = model_azel(make_observation(t, AngleKind::AzEl, AngleReduction::ApparentRefracted, 0.0, 0.0, site, vacuum_atmosphere()), station, target, orientation);
        REQUIRE(m.has_value());
        const double beta = 7.292115e-5 * 6378137.0 / kC;
        INFO("the production station's v/c: " << k->velocity_m_s.norm() / kC << ", the closed form's " << beta);
        CHECK_THAT(m->applied().aberration_shift_rad, WithinAbs(beta, 1.0e-9));
        CHECK_THAT(wrap_to_pi(m->azimuth_rad()), WithinAbs(beta / std::cos(el), 1.0e-9));
        CHECK_THAT(m->elevation_rad(), WithinAbs(el, 1.0e-9));
    }
}

// ---- MEAS-A-094 and -095 -----------------------------------------------------------------------------------------------------------------------------------------------

TEST_CASE("MEAS-A-094  the AngleObservation carries its reduction, frame, site and sigma; an observation built from a column-verified IOD line of the io fixtures (station from the caller's "
          "registry) has the decoded values and a UTC epoch to 1 ms; the Applied record of an angle model carries the reduction, the emission epoch and light time, the aberration and "
          "refraction applied, the atmosphere, the frame and the omitted terms",
          "[measmod][angles]") {
    const auto iod = iod_line('2', '5', "0917234+123456", "25", "2420", "20260115", "2213", "45678");     // the IOFM-A-010 fixture's own fields
    auto obs = angle_observation(iod, sites(), AngleReduction::Astrometric, std::nullopt, std::nullopt, leaps());
    REQUIRE(obs.has_value());
    CHECK(obs->reduction == AngleReduction::Astrometric);
    CHECK(obs->frame == AngleFrame::Icrf);
    CHECK(obs->kind == AngleKind::RaDec);
    CHECK(obs->site.number == OpticalStationNumber{2420});
    CHECK(obs->site.location.latitude_rad == 0.9);
    CHECK(obs->site.location.longitude_rad == 0.1);
    CHECK_FALSE(obs->atmosphere.has_value());
    CHECK(std::abs(obs->epoch.difference(utc(2026, 1, 15, 22, 13, 45.678)).to_seconds()) <= 1.0e-3);
    CHECK_THAT(obs->a_rad, WithinAbs((9.0 + 17.234 / 60.0) * 15.0 * kPi / 180.0, 1.0e-15));
    CHECK_THAT(obs->b_rad, WithinAbs((12.0 + 34.56 / 60.0) * kPi / 180.0, 1.0e-15));
    REQUIRE(obs->sigma_rad.has_value());
    CHECK_THAT(*obs->sigma_rad, WithinRel(2.0e-3 * kPi / 10800.0, 1.0e-14));

    const Epoch t = obs->epoch;
    const UniformStation station(t, Vec3{6378137.0, 0.0, 0.0}, Vec3{});
    const Vec3 v_earth{-1.2e4, 2.6e4, 1.1e4};
    const ConstantEarthMotion motion(Vec3{1.0e11, 0.0, 0.0}, v_earth);
    const LinearTrajectory target(t, Vec3{6378137.0, 0.0, 0.0} + 1.2e6 * unit(Vec3{0.2, 0.7, 0.5}), Vec3{2000.0, -3000.0, 5000.0});

    SECTION("Astrometric") {
        auto m = model_radec(*obs, station, target, motion);
        REQUIRE(m.has_value());
        const AngleApplied& a = m->applied();
        CHECK(a.reduction == AngleReduction::Astrometric);
        CHECK(a.frame == AngleFrame::Icrf);
        CHECK(a.light_time_s > 3.0e-3);
        CHECK(a.light_time_s < 5.0e-3);
        CHECK_THAT(a.range_m, WithinRel(a.light_time_s * kC, 1.0e-14));
        CHECK(a.passes >= 1);
        CHECK(std::abs(t.difference(m->emission_tt()).to_seconds() - a.light_time_s) <= 1.0e-15);
        CHECK(max_abs(a.aberration_beta - (1.0 / kC) * v_earth) <= 1.0e-18);
        CHECK(a.aberration_shift_rad > 1.0e-5);
        CHECK(a.aberration_shift_rad < 1.0e-4);
        CHECK(a.refraction_rad == 0.0);
        CHECK_FALSE(a.atmosphere.has_value());
        std::vector<std::string> names;
        for (const OmittedTerm& o : a.omitted) {
            names.push_back(o.name);
            CHECK_FALSE(o.magnitude.empty());
            CHECK_FALSE(o.source.empty());
        }
        for (const char* want : {"station_displacement", "gravitational_light_deflection", "solar_term_in_aberration", "aberration_cross_term"})
            CHECK(std::find(names.begin(), names.end(), want) != names.end());
        CHECK(std::find(names.begin(), names.end(), "refraction_model_error") == names.end());
        // the residual: observed minus modelled
        CHECK(m->observed_ra_rad() == obs->a_rad);
        CHECK(m->observed_dec_rad() == obs->b_rad);
        CHECK(m->residual_dec_rad() == obs->b_rad - m->dec_rad());
    }
    SECTION("Geometric: the aberration is not applied, and the record says so") {
        auto g = *obs;
        g.reduction = AngleReduction::Geometric;
        auto m = model_radec(g, station, target, motion);
        REQUIRE(m.has_value());
        CHECK(m->applied().reduction == AngleReduction::Geometric);
        CHECK(m->applied().aberration_shift_rad == 0.0);
        CHECK(m->applied().aberration_beta.norm() == 0.0);
        std::vector<std::string> names;
        for (const OmittedTerm& o : m->applied().omitted) names.push_back(o.name);
        CHECK(std::find(names.begin(), names.end(), "aberration_cross_term") == names.end());
    }
    SECTION("ApparentRefracted: the diurnal aberration, the refraction and the atmosphere are recorded") {
        auto azel = angle_observation(iod_line('5', ' ', "1803000+450000"), sites(), AngleReduction::ApparentRefracted, std::nullopt, standard_atmosphere(), leaps());
        REQUIRE(azel.has_value());
        CHECK(azel->frame == AngleFrame::Local);
        CHECK(azel->atmosphere.has_value());
        const EquatorFrame frame;
        const UniformStation st(t, frame.position, Vec3{0.0, 465.1, 0.0}, Vec3{1.0, 0.0, 0.0});
        const ConstantOrientation orientation(Mat3::identity(), Vec3{0.0, 0.0, 7.292115e-5});
        const LinearTrajectory tg(t, frame.position + 1.0e6 * frame.direction(0.5, 0.7), Vec3{});
        auto m = model_azel(*azel, st, tg, orientation);
        REQUIRE(m.has_value());
        const AngleApplied& a = m->applied();
        CHECK(a.reduction == AngleReduction::ApparentRefracted);
        CHECK(a.frame == AngleFrame::Local);
        REQUIRE(a.atmosphere.has_value());
        CHECK(a.atmosphere->atmosphere.pressure_hpa == 1013.25);
        CHECK(a.atmosphere->wavelength_um == 0.55);
        CHECK(a.refraction_a > 2.5e-4);
        CHECK(a.refraction_b < 0.0);
        CHECK_THAT(a.refraction_rad, WithinAbs(a.zenith_vacuum_rad - a.zenith_observed_rad, 1.0e-18));
        CHECK(a.refraction_rad > 0.0);
        CHECK_THAT(m->elevation_rad(), WithinAbs(kPi / 2.0 - a.zenith_observed_rad, 1.0e-15));
        CHECK_THAT(a.aberration_beta.y, WithinAbs(465.1 / kC, 1.0e-15));
        std::vector<std::string> names;
        for (const OmittedTerm& o : a.omitted) names.push_back(o.name);
        CHECK(std::find(names.begin(), names.end(), "refraction_model_error") != names.end());
        CHECK(std::find(names.begin(), names.end(), "aberration_cross_term") == names.end());
    }
}

TEST_CASE("MEAS-A-095  the residual is observed minus modelled per component, the right-ascension and azimuth difference wrapped to (-pi, pi]: 359.9 observed against 0.1 modelled is -0.2 "
          "degrees, 0.1 against 359.9 is +0.2; no cos(dec) scaling; wrap_to_pi at its boundaries",
          "[measmod][angles]") {
    const Epoch t = utc(2026, 1, 15, 22, 13, 45.678);
    const double deg = kPi / 180.0;
    const UniformStation station(t, Vec3{}, Vec3{});
    const ConstantEarthMotion motion(Vec3{}, Vec3{});
    const OpticalSite site = equator_site();
    const double dec = 60.0 * deg;
    auto residual_for = [&](double observed_ra_deg, double modelled_ra_deg, double observed_dec_deg) {
        const LinearTrajectory target(t, 1.0e6 * radec_vector(modelled_ra_deg * deg, dec), Vec3{});
        auto m = model_radec(make_observation(t, AngleKind::RaDec, AngleReduction::Geometric, observed_ra_deg * deg, observed_dec_deg * deg, site), station, target, motion);
        REQUIRE(m.has_value());
        CHECK_THAT(m->ra_rad(), WithinAbs(modelled_ra_deg * deg, 1.0e-14));
        return std::make_pair(m->residual_ra_rad(), m->residual_dec_rad());
    };
    SECTION("across zero") {
        CHECK_THAT(residual_for(359.9, 0.1, 60.0).first, WithinAbs(-0.2 * deg, 1.0e-14));
        CHECK_THAT(residual_for(0.1, 359.9, 60.0).first, WithinAbs(0.2 * deg, 1.0e-14));
        CHECK_THAT(residual_for(10.0, 20.0, 60.0).first, WithinAbs(-10.0 * deg, 1.0e-14));
        CHECK_THAT(residual_for(350.0, 10.0, 60.0).first, WithinAbs(-20.0 * deg, 1.0e-14));
    }
    SECTION("the declination residual is the plain difference, and the right-ascension one carries no cos(dec)") {
        auto r = residual_for(100.0, 100.0 - 0.01, 60.2);
        CHECK_THAT(r.second, WithinAbs(0.2 * deg, 1.0e-14));
        CHECK_THAT(r.first, WithinAbs(0.01 * deg, 1.0e-14));       // not 0.01 cos(60) degrees
    }
    SECTION("wrap_to_pi: (-pi, pi], pi included, -pi not") {
        CHECK(wrap_to_pi(0.0) == 0.0);
        CHECK(wrap_to_pi(kPi) == kPi);
        CHECK(wrap_to_pi(-kPi) == kPi);
        CHECK_THAT(wrap_to_pi(2.0 * kPi + 0.25), WithinAbs(0.25, 1.0e-15));
        CHECK_THAT(wrap_to_pi(-2.0 * kPi - 0.25), WithinAbs(-0.25, 1.0e-15));
        CHECK_THAT(wrap_to_pi(kPi + 0.1), WithinAbs(-kPi + 0.1, 1.0e-15));
        CHECK_THAT(wrap_to_pi(-kPi + 0.1), WithinAbs(-kPi + 0.1, 1.0e-15));
        CHECK_THAT(wrap_to_pi(-kPi - 0.1), WithinAbs(kPi - 0.1, 1.0e-15));
    }
}
