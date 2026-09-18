// shadow_tests.cpp — L4 step 1's gate, conical half.
//
// SPEC-shadow §8.  THE GATE IS CLOSED-FORM GEOMETRY, NOT PUBLISHED TIMES.
// LI19's Table 2 prints 16 eclipse events for GRACE-A with penumbra entry/exit
// to the second, and the transit durations show all 30 are transverse crossings,
// so all 30 would be reachable with an ephemeris good to about a kilometre --
// but every route to one requires an account or a request form, and the standing
// prohibition covers both. The reason Table 2 is absent is an account, not the
// geometry.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/shadow/conical.hpp>

#include <cmath>
#include <algorithm>
#include <numbers>
#include <vector>

using namespace odl;
using namespace odl::shadow;
using frames::Frame;
using P = frames::Position<Frame::GCRS>;

namespace {

constexpr double kAu = 1.495978707e11;

/// A satellite at radius `r`, at GEOCENTRIC angle `angle` from the anti-Sun
/// direction. The Sun sits on +x, so the shadow axis is -x.
P satellite(double r, double angle) {
    return P{Vec3{-r * std::cos(angle), r * std::sin(angle), 0.0}};
}
P the_sun(double d = kAu) { return P{Vec3{d, 0.0, 0.0}}; }

/// THE GEOCENTRIC ANGLE IS NOT THE ANGLE THE MODEL USES, and conflating them is
/// how the first version of these tests failed against correct code. `conical`
/// works in the angle AT THE SATELLITE between the Earth's centre and the Sun's;
/// the placement above is an angle AT THE EARTH'S CENTRE. At LEO the Earth
/// subtends 1.055 rad, so the two are nowhere near equal, and a test that
/// compares a closed form at one against an integration at the other disagrees
/// by an amount that does NOT shrink when the grid is refined -- which is how it
/// was caught.
///
/// Re-derived here from the positions rather than exposed from the module, so
/// this stays an independent route to the same geometry.
struct Seen { double a_sun, a_earth, sep; };
Seen as_seen(const P& sun, const P& sat) {
    const Vec3 rs = sun.metres(), r = sat.metres();
    const Vec3 to_sun{rs.x - r.x, rs.y - r.y, rs.z - r.z};
    const double rn = r.norm(), ds = to_sun.norm();
    const double cos_sep = -(r.x * to_sun.x + r.y * to_sun.y + r.z * to_sun.z) / (rn * ds);
    return Seen{std::asin(kSunRadiusM / ds), std::asin(kEarthRadiusM / rn),
                std::acos(std::clamp(cos_sep, -1.0, 1.0))};
}

/// The two-disc overlap by NUMERICAL INTEGRATION -- an independent route to the
/// quantity conical.cpp computes with the closed-form segment formula.
double overlap_numerically(double a, double b, double d, int n = 4000) {
    // integrate over the solar disc: fraction of its area within `b` of a centre
    // offset by `d`
    double inside = 0.0, total = 0.0;
    for (int i = 0; i < n; ++i) {
        const double x = -a + 2.0 * a * (i + 0.5) / n;
        const double half = std::sqrt(std::max(0.0, a * a - x * x));
        for (int j = 0; j < n / 4; ++j) {
            const double y = -half + 2.0 * half * (j + 0.5) / (n / 4);
            const double w = (2.0 * a / n) * (2.0 * half / (n / 4));
            total += w;
            if ((x - d) * (x - d) + y * y <= b * b) inside += w;
        }
    }
    return inside;
}

}  // namespace

TEST_CASE("SHDW-A-001  the boundaries, exactly", "[shadow][gate]") {
    const double r = 7331.0e3;
    const double a_e = std::asin(kEarthRadiusM / r);
    const double a_s = std::asin(kSunRadiusM / (kAu + r));

    // well outside the penumbral cone
    auto lit = conical(the_sun(), satellite(r, a_e + a_s + 1e-3));
    REQUIRE(lit.has_value());
    CHECK(lit->fraction == 1.0);
    CHECK(lit->state == State::sunlight);

    // well inside the umbral cone
    auto dark = conical(the_sun(), satellite(r, 0.0));
    REQUIRE(dark.has_value());
    CHECK(dark->fraction == 0.0);
    CHECK(dark->state == State::umbra);

    // strictly between, on the penumbra
    auto mid = conical(the_sun(), satellite(r, a_e));
    REQUIRE(mid.has_value());
    CHECK(mid->fraction > 0.0);
    CHECK(mid->fraction < 1.0);
    CHECK(mid->state == State::penumbra);
}

TEST_CASE("SHDW-A-002  monotone across a transverse traversal", "[shadow][gate]") {
    const double r = 7331.0e3;
    const double a_e = std::asin(kEarthRadiusM / r);
    const double a_s = std::asin(kSunRadiusM / (kAu + r));
    const int n = 2001;
    double previous = 2.0;
    int samples = 0, strictly_between = 0;
    for (int i = 0; i < n; ++i) {
        const double sep = (a_e + 2.0 * a_s) * (n - 1 - i) / (n - 1);
        auto f = conical(the_sun(), satellite(r, sep));
        REQUIRE(f.has_value());
        CHECK(f->fraction <= previous + 1e-15);          // never increases
        previous = f->fraction;
        ++samples;
        if (f->fraction > 0.0 && f->fraction < 1.0) ++strictly_between;
    }
    INFO("samples " << samples << ", of which strictly between 0 and 1: " << strictly_between);
    CHECK(samples == n);                                  // the count, asserted
    CHECK(strictly_between > 10);                         // the penumbra was traversed
}

TEST_CASE("SHDW-A-003  the occulted-area ratio against an independent integration",
          "[shadow][gate]") {
    const double r = 7331.0e3;
    double worst = 0.0;
    int checked = 0;
    // sweep the GEOCENTRIC placement, then read off the geometry the model
    // actually sees and integrate at THAT
    for (int i = 1; i < 2000; ++i) {
        const double g = 1.0 + 0.0002 * i;
        const auto sat = satellite(r, g);
        auto f = conical(the_sun(), sat);
        REQUIRE(f.has_value());
        if (f->state != State::penumbra) continue;
        if (checked >= 4) break;
        const Seen v = as_seen(the_sun(), sat);
        const double numeric = 1.0 - overlap_numerically(v.a_sun, v.a_earth, v.sep)
                                     / (std::numbers::pi * v.a_sun * v.a_sun);
        worst = std::max(worst, std::abs(f->fraction - numeric));
        ++checked;
    }
    INFO("checked " << checked << " geometries, worst |closed form - numerical| " << worst);
    CHECK(checked == 4);
    CHECK(worst < 2e-3);        // the grid's own resolution, not the formula's
}

TEST_CASE("SHDW-A-004  the penumbra's angular width IS the solar angular diameter",
          "[shadow][gate]") {
    // THIS IS THE TEST A POINT-SOURCE IMPLEMENTATION FAILS. The Sun being a DISC
    // is one of the SECM's five choices; a point source gives a penumbra of zero
    // angular width, and nothing else in this gate would notice.
    const double r = 7331.0e3;
    const int n = 400001;
    // scan the geocentric placement across the terminator, and measure the width
    // in the SATELLITE-CENTRIC separation -- the variable the model works in
    double sep_umbra_edge = -1.0, sep_sunlight_edge = -1.0, a_s = 0.0;
    int in_umbra = 0, in_sunlight = 0;
    for (int i = 0; i < n; ++i) {
        const double g = 0.9 + 0.4 * i / (n - 1);
        const auto sat = satellite(r, g);
        auto f = conical(the_sun(), sat);
        REQUIRE(f.has_value());
        const Seen v = as_seen(the_sun(), sat);
        a_s = v.a_sun;
        if (f->fraction == 0.0) ++in_umbra;
        if (f->fraction == 1.0) ++in_sunlight;
        if (f->fraction > 0.0 && sep_umbra_edge < 0.0) sep_umbra_edge = v.sep;
        if (f->fraction < 1.0) sep_sunlight_edge = v.sep;
    }
    const double width = sep_sunlight_edge - sep_umbra_edge;
    INFO("penumbra angular width " << width << " rad, solar angular diameter "
         << 2.0 * a_s << " rad; scan held " << in_umbra << " umbra and " << in_sunlight
         << " sunlight samples either side of it");
    // THE SCAN MUST BRACKET THE TRANSITION, asserted rather than assumed. A range
    // lying wholly inside the penumbra would make `width` the SCAN's width and
    // could pass this case while measuring nothing -- which is the same fault,
    // one level up, as the range that lay wholly inside the umbra and reported a
    // width of 1.02 rad. Both edges must have real ground on the far side.
    CHECK(in_umbra > 0);
    CHECK(in_sunlight > 0);
    CHECK_THAT(width, Catch::Matchers::WithinRel(2.0 * a_s, 1e-3));
    CHECK(width > 0.0);          // which a point source could not produce
}

TEST_CASE("SHDW-A-005  the annular branch, SYNTHETIC — no orbit in this plan reaches it",
          "[shadow][gate]") {
    // SHDW-R-002. An annular eclipse of the Sun BY THE EARTH needs the satellite
    // beyond the umbra cone's apex at 1.3842e6 km. GNSS is at 1.9% of that. This
    // geometry is CONSTRUCTED, and the test's name says so, because a branch that
    // passes only because nothing reaches it must be distinguishable from one
    // that is never exercised by neglect.
    const double apex = umbra_apex_distance_m();
    INFO("umbra apex " << apex / 1e3 << " km; GNSS at 26560 km is "
         << 26560e3 / apex << " of it");
    CHECK_THAT(apex / 1e3, Catch::Matchers::WithinRel(1.3842e6, 1e-3));
    CHECK(26560e3 / apex < 0.02);

    const double r = 2.0e9;                        // 2e6 km, beyond the apex
    auto f = conical(the_sun(), satellite(r, 0.0));
    REQUIRE(f.has_value());
    INFO("at " << r / 1e3 << " km on the shadow axis: Fs = " << f->fraction);
    CHECK(f->state == State::annular);
    CHECK(f->fraction > 0.0);
    CHECK(f->fraction < 1.0);
}

TEST_CASE("SHDW-A-006  refusals fire, and do not fire on the adjacent input", "[shadow]") {
    auto origin = conical(the_sun(), P{Vec3{0.0, 0.0, 0.0}});
    REQUIRE_FALSE(origin.has_value());
    CHECK(origin.error().id == "SHDW-F-001");

    auto inside = conical(the_sun(), P{Vec3{0.0, 0.0, kEarthRadiusM * 0.5}});
    REQUIRE_FALSE(inside.has_value());
    CHECK(inside.error().id == "SHDW-F-002");

    auto no_sun = conical(P{Vec3{0.0, 0.0, 0.0}}, satellite(7331.0e3, 0.0));
    REQUIRE_FALSE(no_sun.has_value());
    CHECK(no_sun.error().id == "SHDW-F-001");

    // proven both ways: just outside the surface is accepted
    CHECK(conical(the_sun(), P{Vec3{0.0, 0.0, kEarthRadiusM * 1.001}}).has_value());
}
