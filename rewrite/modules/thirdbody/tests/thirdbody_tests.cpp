// thirdbody_tests.cpp — SPEC-perturbations §4.6 and §8.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/core/units.hpp>

#include <odl/thirdbody/attraction.hpp>

#include <cmath>
#include <fstream>
#include <sstream>
#include <type_traits>

using namespace odl;
using namespace odl::thirdbody;
using Catch::Matchers::WithinRel;

namespace {

std::string slurp(const char* p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream os;
    os << f.rdbuf();
    return os.str();
}

const odl::time::LeapTable& leaps() {
    static const auto t = odl::time::LeapTable::parse(
        slurp(ODL_LEAP_SECOND_FILE), odl::time::LeapProvenance{"IERS Leap_Second.dat", "", ""});
    REQUIRE(t.has_value());
    return *t;
}

const GravitationalParameters& gm() {
    static const auto g = GravitationalParameters::load(ODL_GM_DE440_TPC, ODL_MANIFEST_CACHE_ROOT);
    REQUIRE(g.has_value());
    return *g;
}

const eph::Ephemeris& ephemeris() {
    static const auto e = eph::Ephemeris::open({ODL_DE440S_BSP}, {});
    REQUIRE(e.has_value());
    return *e;
}

odl::time::Epoch tdb_at(double jd) {
    auto e = odl::time::Epoch::from_two_part_jd(odl::time::TimeScale::TDB, jd, 0.0, leaps());
    REQUIRE(e.has_value());
    return *e;
}

}  // namespace

TEST_CASE("the GM file parses, and agrees with TN36-1 where the two overlap", "[thirdbody]") {
    const GravitationalParameters& g = gm();
    WARN("gm_de440.tpc: " << g.count() << " bodies parsed");
    CHECK(g.count() > 20);

    auto sun = g.gm_m3_s2(eph::Body::Sun);
    auto moon = g.gm_m3_s2(eph::Body::Moon);
    auto earth = g.gm_m3_s2(eph::Body::Earth);
    REQUIRE(sun.has_value());
    REQUIRE(moon.has_value());
    REQUIRE(earth.has_value());

    // The Sun's GM disagrees with TN36-1 by almost exactly L_B = 1.5505e-8, the
    // TDB/TCB rate — a time-scale convention, not an error, and the same family
    // as the TT/TCG GM trap SPEC-gravity GRAV-R-006 refuses. Recorded as a
    // measurement so that the next reader does not take it for a discrepancy.
    constexpr double kGmSunTn36 = 1.32712442099e20;
    constexpr double kLb = 1.550519768e-8;
    const double rel = (kGmSunTn36 - *sun) / kGmSunTn36;
    WARN("GM_sun: gm_de440.tpc " << *sun << ", TN36-1 " << kGmSunTn36
         << ", relative difference " << rel << " against L_B = " << kLb
         << " (ratio " << rel / kLb << ")");
    CHECK_THAT(rel, WithinRel(kLb, 0.01));

    // The Moon through TN36-1's mass ratio, which is an independent route.
    constexpr double kMoonEarthMassRatio = 0.0123000371;
    CHECK_THAT(*moon / *earth, WithinRel(kMoonEarthMassRatio, 1e-7));

    // The planets are SYSTEM BARYCENTRES, so Jupiter's barycentre GM exceeds the
    // planet's own: a swap to BODY599 would be caught here.
    auto jup = g.gm_m3_s2(eph::Body::JupiterBarycentre);
    REQUIRE(jup.has_value());
    CHECK(*jup > 1.2671e17);
    CHECK(*jup < 1.2672e17);
}

TEST_CASE("PERT-A-014: the indirect term is not optional", "[thirdbody]") {
    const frames::Position<frames::Frame::GCRS> sat{Vec3{7331e3, 0.0, 0.0}};
    const auto when = tdb_at(2458849.5);
    for (auto body : {eph::Body::Moon, eph::Body::Sun}) {
        auto mu = gm().gm_m3_s2(body);
        auto st = ephemeris().geocentric_state(body, when, leaps());
        REQUIRE(mu.has_value());
        REQUIRE(st.has_value());
        const Vec3 rb = odl::field_position_m_from_state_km(st->position());

        const Vec3 full = Attraction::pair_direct(sat.metres(), rb, *mu);
        const Vec3 d = rb - sat.metres();
        const double dn = d.norm();
        const Vec3 direct_only = (*mu / (dn * dn * dn)) * d;     // the indirect term dropped
        const double ratio = direct_only.norm() / full.norm();
        WARN("PERT-A-014 " << eph::name_of(body) << ": with the indirect term "
             << full.norm() << " m/s^2, without it " << direct_only.norm()
             << " m/s^2 — a factor of " << ratio);
        CHECK(ratio > 10.0);   // not a small correction; it dominates if dropped
    }
}

TEST_CASE("PERT-A-015: the cancellation, measured before the remedy is adopted",
          "[thirdbody]") {
    const frames::Position<frames::Frame::GCRS> sat{Vec3{7331e3, 1000e3, -2000e3}};
    const auto when = tdb_at(2458849.5);
    for (auto body : {eph::Body::Moon, eph::Body::Sun, eph::Body::JupiterBarycentre}) {
        auto mu = gm().gm_m3_s2(body);
        auto st = ephemeris().geocentric_state(body, when, leaps());
        REQUIRE(mu.has_value());
        REQUIRE(st.has_value());
        const Vec3 rb = odl::field_position_m_from_state_km(st->position());

        // The reference is the SAME expression in long double, which on this
        // platform carries 11 more bits of mantissa.  It is not an independent
        // formula, so what it measures is the cancellation and nothing else.
        const long double gx = static_cast<long double>(rb.x) - sat.metres().x;
        const long double gy = static_cast<long double>(rb.y) - sat.metres().y;
        const long double gz = static_cast<long double>(rb.z) - sat.metres().z;
        const long double dn = std::sqrt(gx * gx + gy * gy + gz * gz);
        const long double bn = std::sqrt(static_cast<long double>(rb.x) * rb.x
                                       + static_cast<long double>(rb.y) * rb.y
                                       + static_cast<long double>(rb.z) * rb.z);
        const long double kd = static_cast<long double>(*mu) / (dn * dn * dn);
        const long double kb = static_cast<long double>(*mu) / (bn * bn * bn);
        const Vec3 ref{static_cast<double>(kd * gx - kb * rb.x),
                       static_cast<double>(kd * gy - kb * rb.y),
                       static_cast<double>(kd * gz - kb * rb.z)};

        const double e_direct = (Attraction::pair_direct(sat.metres(), rb, *mu) - ref).norm()
                              / ref.norm();
        const double e_stable = (Attraction::pair_stable(sat.metres(), rb, *mu) - ref).norm()
                              / ref.norm();
        WARN("PERT-A-015 " << eph::name_of(body) << ": |a| = " << ref.norm()
             << " m/s^2; the written form loses " << e_direct
             << " relative, the rearranged one " << e_stable);
        CHECK(e_stable <= e_direct * 1.5 + 1e-17);
        CHECK(e_stable < 1e-13);
    }
}

TEST_CASE("PERT-A-016: the body list, and what cannot be left out of it", "[thirdbody]") {
    const frames::Position<frames::Frame::GCRS> sat{Vec3{7331e3, 0.0, 0.0}};
    const auto when = tdb_at(2458849.5);

    auto missing_moon = Attraction::acceleration(sat, {eph::Body::Sun}, ephemeris(), gm(), when,
                                                 leaps());
    REQUIRE_FALSE(missing_moon.has_value());
    CHECK(missing_moon.error().id == "PERT-F-013");
    CHECK(missing_moon.error().message.find("not optional") != std::string::npos);

    auto parts = Attraction::by_body(sat, {eph::Body::Sun, eph::Body::Moon,
                                           eph::Body::VenusBarycentre, eph::Body::JupiterBarycentre},
                                     ephemeris(), gm(), when, leaps());
    REQUIRE(parts.has_value());
    REQUIRE(parts->size() == 4);
    for (const auto& p : *parts) {
        WARN("PERT-A-016 " << eph::name_of(p.body) << ": " << p.a_m_s2.norm() << " m/s^2");
    }
    // The Sun and Moon dominate by four orders of magnitude, which is why
    // PERT-R-052 makes them compulsory and the planets optional.
    const double moon = (*parts)[1].a_m_s2.norm();
    const double venus = (*parts)[2].a_m_s2.norm();
    CHECK(moon / venus > 1e3);
    // WHETHER THE PLANETS MATTER DEPENDS ON THE DATE, and the first version of
    // this test asserted that they always do.  At this epoch Venus contributes
    // 5.3e-13 m/s^2, BELOW the 1.2174e-10 m/s^2 the degree-90 static field
    // accepts as truncation error.  At closest approach — 0.27 au rather than
    // the ~1 au of 2020-01-01 — the same term is about 7e-11 m/s^2, which is
    // within a factor of two of it.  So the honest statement is that a planetary
    // term is a function of the configuration, and the test measures rather than
    // asserts a size.
    WARN("PERT-A-016: at JD 2458849.5 Venus contributes " << venus
         << " m/s^2 against the static field's degree-90 truncation error of 1.2174e-10; at "
            "closest approach the same term is about 1e2 times larger");
    CHECK(venus > 0.0);
    CHECK(venus < 1e-10);

    auto total = Attraction::acceleration(sat, {eph::Body::Sun, eph::Body::Moon}, ephemeris(),
                                          gm(), when, leaps());
    REQUIRE(total.has_value());
    const Vec3 sum = (*parts)[0].a_m_s2 + (*parts)[1].a_m_s2;
    CHECK((total->metres_per_second_squared() - sum).norm() / sum.norm() < 1e-14);
}

TEST_CASE("PERT-F-010 / F-012: refusals", "[thirdbody]") {
    auto outside = GravitationalParameters::load("/etc/hostname", ODL_MANIFEST_CACHE_ROOT);
    REQUIRE_FALSE(outside.has_value());
    CHECK(outside.error().id == "PERT-F-010");

    auto ssb = gm().gm_m3_s2(eph::Body::SolarSystemBarycentre);
    REQUIRE_FALSE(ssb.has_value());
    CHECK(ssb.error().id == "PERT-F-012");
    CHECK(ssb.error().message.find("guessed GM") != std::string::npos);

    static_assert(!std::is_default_constructible_v<GravitationalParameters>);
    static_assert(!std::is_convertible_v<int, eph::Body>);
}
