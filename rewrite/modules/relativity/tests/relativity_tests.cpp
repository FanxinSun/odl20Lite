// relativity_tests.cpp — SPEC-perturbations §4.5 and §8.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/core/units.hpp>

#include <odl/relativity/correction.hpp>

#include <cmath>
#include <numbers>
#include <type_traits>

using namespace odl;
using namespace odl::relativity;
using Catch::Matchers::WithinRel;
using frames::Frame;

namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kSecondsPerJulianYear = 365.25 * 86400.0;
constexpr double kRadToMas = 180.0 / kPi * 3600.0 * 1000.0;

/// An epoch is needed to build a State and nothing here depends on it, so every
/// state uses the same one and the tests say so rather than implying otherwise.
const odl::time::Epoch& any_epoch() {
    static const odl::time::Epoch e = [] {
        auto r = odl::time::Epoch::from_gps_week(2000, 0.0);
        REQUIRE(r.has_value());
        return *r;
    }();
    return e;
}

/// A circular orbit of radius `a_m`, inclined `inc_deg`, at argument `u` from
/// the ascending node.  km and km/s, because that is what a State carries.
frames::State<Frame::GCRS> circular(double a_m, double inc_deg, double u) {
    const double v = std::sqrt(Correction::kGmEarth / a_m);
    const double i = inc_deg * kPi / 180.0;
    const Vec3 r{a_m * std::cos(u), a_m * std::sin(u) * std::cos(i),
                 a_m * std::sin(u) * std::sin(i)};
    const Vec3 vv{-v * std::sin(u), v * std::cos(u) * std::cos(i), v * std::cos(u) * std::sin(i)};
    return frames::State<Frame::GCRS>{any_epoch(), odl::km_from_metres(r), odl::km_from_metres(vv)};
}

/// The Earth about the Sun: a circular heliocentric orbit, which is all the
/// de Sitter term needs and is accurate to the eccentricity.
frames::State<Frame::BCRS> earth_about_sun() {
    const double R = 1.495978707e11;
    const double V = std::sqrt(Correction::kGmSun / R);
    return frames::State<Frame::BCRS>{any_epoch(), odl::km_from_metres(Vec3{R, 0.0, 0.0}),
                                      odl::km_from_metres(Vec3{0.0, V, 0.0})};
}

Vec3 term_of(const frames::State<Frame::GCRS>& s, Term t,
             PpnParameters ppn = PpnParameters::general_relativity()) {
    auto parts = Correction::by_term(s, earth_about_sun(), Terms::only(t), ppn);
    REQUIRE(parts.has_value());
    REQUIRE(parts->size() == 1);
    return parts->front().a_m_s2;
}

}  // namespace

TEST_CASE("PERT-A-010: the three terms are the sizes TN36-10 §10.3 states", "[relativity]") {
    struct Row { double a_km; const char* what; double lo; double hi; };
    for (double a_km : {7331.0, 42164.0}) {
        const auto s = circular(a_km * 1e3, 55.0, 0.7);
        const double newtonian = Correction::kGmEarth / (a_km * 1e3 * a_km * 1e3);
        const double sch = term_of(s, Term::Schwarzschild).norm() / newtonian;
        const double lt = term_of(s, Term::LenseThirring).norm() / newtonian;
        const double ds = term_of(s, Term::DeSitter).norm() / newtonian;
        WARN("PERT-A-010 at " << a_km << " km: Schwarzschild " << sch << ", Lense-Thirring " << lt
             << ", de Sitter " << ds << " of the main Newtonian acceleration");
        // "a few parts in 1e10 (high orbits) to 1e9 (low orbits)"
        CHECK(sch > 1e-10);
        CHECK(sch < 1e-8);
        // "about 1e-11 to 1e-12"
        CHECK(lt > 1e-13);
        CHECK(lt < 1e-10);
        CHECK(ds > 1e-13);
        CHECK(ds < 1e-10);
        // and the ordering the source asserts
        CHECK(sch > lt);
        CHECK(sch > ds);
    }
    // "The Lense-Thirring terms are less important than the geodesic terms for
    // orbits higher than Lageos (altitude above 6000 km) and more important for
    // orbits lower than Lageos."  Lageos is a = 12270 km.
    const auto low = circular(7331e3, 55.0, 0.7);
    const auto high = circular(26600e3, 55.0, 0.7);
    CHECK(term_of(low, Term::LenseThirring).norm() > term_of(low, Term::DeSitter).norm());
    CHECK(term_of(high, Term::LenseThirring).norm() < term_of(high, Term::DeSitter).norm());
}

TEST_CASE("PERT-A-011: as precession rates, against the published mas/yr", "[relativity]") {
    // DE SITTER.  The term is (1+2gamma)(W x rdot) with W the geodesic
    // precession vector, and an acceleration of the form A x v is the
    // first-order signature of a frame precessing at A/2.  So the rate follows
    // from the term itself: take a velocity perpendicular to W, read |A| = |a|/|v|,
    // and halve it.  Height-independence is then structural rather than sampled,
    // which is why TN36-10 states it that way.
    double first = 0.0;
    for (double a_km : {6778.0, 7331.0, 12270.0, 26600.0, 42164.0}) {
        const auto s = circular(a_km * 1e3, 0.0, 0.0);      // v along +y, W along +z
        const double v = s.velocity().norm() * 1000.0;
        const double rate_rad_s = term_of(s, Term::DeSitter).norm() / v / 2.0;
        const double mas_per_year = rate_rad_s * kSecondsPerJulianYear * kRadToMas;
        INFO("a = " << a_km << " km");
        CHECK_THAT(mas_per_year, WithinRel(19.0, 0.05));
        if (first == 0.0) {
            first = mas_per_year;
            WARN("PERT-A-011 de Sitter: " << mas_per_year
                 << " mas/yr, against TN36-10's 19 mas/yr, independent of height");
        } else {
            CHECK_THAT(mas_per_year, WithinRel(first, 1e-12));   // INDEPENDENT of height
        }
    }

    // LENSE-THIRRING, as the nodal precession, averaged over one circular orbit
    // from the module's own acceleration through the Gauss planetary equation
    //     dOmega/dt = r sin(u) a_W / (h sin i)
    // with a_W the component along the orbit normal.
    for (auto [a_km, want] : {std::pair{42164.0, 0.8}, std::pair{6778.0, 180.0}}) {
        const double a = a_km * 1e3;
        const double inc = 55.0, i = inc * kPi / 180.0;
        const double h = std::sqrt(Correction::kGmEarth * a);
        const Vec3 normal{0.0, -std::sin(i), std::cos(i)};
        constexpr int kSteps = 720;
        double sum = 0.0;
        for (int k = 0; k < kSteps; ++k) {
            const double u = 2.0 * kPi * (k + 0.5) / kSteps;
            const Vec3 acc = term_of(circular(a, inc, u), Term::LenseThirring);
            sum += a * std::sin(u) * acc.dot(normal) / (h * std::sin(i));
        }
        const double mas_per_year = (sum / kSteps) * kSecondsPerJulianYear * kRadToMas;
        WARN("PERT-A-011 Lense-Thirring at a = " << a_km << " km: " << mas_per_year
             << " mas/yr, against TN36-10's " << want);
        INFO("a = " << a_km << " km, " << kSteps << " points around the orbit");
        CHECK_THAT(mas_per_year, WithinRel(want, 0.30));    // the source states one digit
    }
}

TEST_CASE("PERT-A-012: beta and gamma are live, not decoration", "[relativity]") {
    const auto s = circular(7331e3, 55.0, 0.7);
    const auto gr = PpnParameters::general_relativity();
    auto off = PpnParameters::of(1.0, 1.1);
    REQUIRE(off.has_value());
    CHECK(gr.is_general_relativity());
    CHECK_FALSE(off->is_general_relativity());
    const double a_gr = term_of(s, Term::Schwarzschild, gr).norm();
    const double a_off = term_of(s, Term::Schwarzschild, *off).norm();
    CHECK(std::abs(a_off - a_gr) / a_gr > 0.01);
    // gamma also enters Lense-Thirring and de Sitter, and beta only Schwarzschild.
    CHECK(term_of(s, Term::DeSitter, *off).norm() != term_of(s, Term::DeSitter, gr).norm());
    auto beta_off = PpnParameters::of(1.1, 1.0);
    REQUIRE(beta_off.has_value());
    CHECK(term_of(s, Term::DeSitter, *beta_off).norm() == term_of(s, Term::DeSitter, gr).norm());
}

TEST_CASE("PERT-A-013: R is heliocentric where r is geocentric", "[relativity]") {
    // Substituting the satellite's geocentric vector for the Earth's
    // heliocentric one changes the de Sitter term by orders of magnitude, which
    // is what makes the distinction worth a requirement rather than a comment.
    const auto s = circular(7331e3, 0.0, 0.0);
    const double correct = term_of(s, Term::DeSitter).norm();
    const frames::State<Frame::BCRS> wrong{any_epoch(), s.position(), s.velocity()};
    auto parts = Correction::by_term(s, wrong, Terms::only(Term::DeSitter));
    REQUIRE(parts.has_value());
    const double substituted = parts->front().a_m_s2.norm();
    WARN("PERT-A-013: substituting the geocentric vector for the heliocentric one changes the "
         "de Sitter term by a factor of " << substituted / correct);
    CHECK(substituted / correct > 1e6);
}

TEST_CASE("PERT-A-019: refusals and structure", "[relativity]") {
    const auto s = circular(7331e3, 55.0, 0.7);
    auto bad = PpnParameters::of(std::nan(""), 1.0);
    REQUIRE_FALSE(bad.has_value());
    CHECK(bad.error().id == "PERT-F-009");

    const frames::State<Frame::GCRS> origin{any_epoch(), Vec3{0, 0, 0}, Vec3{1, 0, 0}};
    auto r = Correction::acceleration(origin, earth_about_sun());
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().id == "PERT-F-001");

    const frames::State<Frame::BCRS> no_sun{any_epoch(), Vec3{0, 0, 0}, Vec3{0, 0, 0}};
    auto r2 = Correction::acceleration(s, no_sun);
    REQUIRE_FALSE(r2.has_value());
    CHECK(r2.error().message.find("heliocentric") != std::string::npos);

    // PERT-R-072: three named flags, not an integer.
    static_assert(!std::is_convertible_v<int, Terms>);
    static_assert(!std::is_convertible_v<Terms, int>);
    static_assert(!std::is_default_constructible_v<PpnParameters>);
    // The sum is the sum of the parts.
    auto all = Correction::acceleration(s, earth_about_sun());
    REQUIRE(all.has_value());
    const Vec3 sum = term_of(s, Term::Schwarzschild) + term_of(s, Term::LenseThirring)
                   + term_of(s, Term::DeSitter);
    CHECK((all->metres_per_second_squared() - sum).norm() < 1e-24);
}
