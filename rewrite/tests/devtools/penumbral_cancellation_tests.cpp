// tests/devtools/penumbral_cancellation_tests.cpp — the tool behind SHDW-R-031 and SHDW-R-033 (plan L0 step 8, group C10): NumPy's arithmetic where the printed figures depend on its rounding, the brightness weight
// against a quadrature, the sweep of Fs against two SECOND METHODS written here apart from the tool (the lens area of two spherical caps for the uniform disc; the blocked arc of each azimuth in closed form
// for any brightness law), the Kepler propagator against its closed forms, the pieces of one traversal (orbit, closest approach, window, sample times, integrals) by hand-derived values, the printed text, the
// command line, and — on the real passages — SPEC-shadow's quotes at their stated precision, the tool's own output at its defaults against its first record, the second method at the sample positions of
// the five configurations the maintainer's checks of 2026-10-09 measured, and the convergence of the near-grazing row.
// (ctests `penumbral_cancellation.behaviour` and `penumbral_cancellation.real_tree`.)
//
// Every expectation is DERIVED BY HAND from the statements of penumbral_cancellation.py (read from git, never run: its output was never kept) or comes from a SECOND METHOD written here; none is taken from running
// the port to see what it says, except the FIRST RECORD (tests/devtools/penumbral_cancellation_recorded.hpp), which says so and is a regression record and not a proof.  Tolerances are stated where they stand and
// derived: from the measured agreement of the two methods (C10_penumbral_exploration_facts.txt, section 1) or from the order of the quadrature error of a midpoint rule on a function with square-root ends.
// NOT tested, by the maintainer's ruling of 2026-10-10: the two-decimal figures of the table of PROVENANCE 25.11 (the tool at its defaults prints 96.34 where the table has 96.35, 74.67 for 74.68, 48.27 for 48.32).

#include <catch2/catch_test_macros.hpp>

#include <odl/devkit/bytes.hpp>
#include <odl/devkit/pyfmt.hpp>
#include <odl/devkit/sha256.hpp>
#include <odl/devkit/tool.hpp>

#include "penumbral_cancellation.hpp"
#include "penumbral_cancellation_recorded.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace pc = odl::tools::penumbral_cancellation;
using odl::devkit::Streams;
using pc::Vec3;

namespace {

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- helpers

std::uint64_t bits_of(double v) { return std::bit_cast<std::uint64_t>(v); }

struct Run {
    int code = -1;
    std::string out;
    std::string err;
};

Run run_with(const pc::Settings& settings, const std::vector<std::string>& args) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = pc::run_on(settings, args, Streams{out, err});
    return Run{code, out.str(), err.str()};
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t nl = text.find('\n', start);
        if (nl == std::string::npos) {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

double dot_(const Vec3& a, const Vec3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vec3 cross_(const Vec3& a, const Vec3& b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
double norm_(const Vec3& a) { return std::sqrt(dot_(a, a)); }

const Vec3 kSun = pc::kSunPosition;
constexpr double kRLeo = 7331.0e3;                                   // the stated passage: LEO, r = 7331 km (SHDW-R-031)
const double kAEarthLeo = std::asin(pc::kREarthM / kRLeo);           // the eclipse cutoff angle a_e at that radius
const double kAs0 = std::asin(pc::kRSunM / pc::kAuM);                // the Sun's angular radius at one astronomical unit

// the baseline orbit's position at an angle theta (radians) from the antisolar point, in the xy plane: the Earth's centre is the origin, the Sun is on +x
Vec3 leo_at(double theta) { return {-kRLeo * std::cos(theta), kRLeo * std::sin(theta), 0.0}; }

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- second methods, written here apart from the tool

// Fs for u = 0 in CLOSED FORM, with no sweep at all: the Sun's disc (angular radius r1, centre in the direction c) and the occulter's disc (r2 = asin(re / |sat|), centre in the direction -sat) are two circular
// caps on the celestial sphere at angular separation d; the area of their lens follows from Gauss-Bonnet: 2 pi - 2 phi1 cos r1 - 2 phi2 cos r2 - 2 psi, with phi1, phi2, psi the angles of the spherical
// triangle (centre 1, centre 2, a crossing of the two boundary circles).  Fs is one minus the lens area over the Sun's cap, 2 pi (1 - cos r1).
double lens_fs_u0(const Vec3& sun, const Vec3& sat, double re) {
    const Vec3 to_sun = {sun[0] - sat[0], sun[1] - sat[1], sun[2] - sat[2]};
    const double ds = norm_(to_sun);
    const Vec3 c = {to_sun[0] / ds, to_sun[1] / ds, to_sun[2] / ds};
    const double r1 = std::asin(pc::kRSunM / ds);
    const double r2 = std::asin(re / norm_(sat));
    const double d = std::atan2(norm_(cross_(c, sat)), -dot_(c, sat));
    if (d >= r1 + r2) return 1.0;
    if (d <= r2 - r1) return 0.0;
    if (d <= r1 - r2) return 1.0 - (1.0 - std::cos(r2)) / (1.0 - std::cos(r1));
    const auto clamp1 = [](double x) { return std::max(-1.0, std::min(1.0, x)); };
    const double phi1 = std::acos(clamp1((std::cos(r2) - std::cos(r1) * std::cos(d)) / (std::sin(r1) * std::sin(d))));
    const double phi2 = std::acos(clamp1((std::cos(r1) - std::cos(r2) * std::cos(d)) / (std::sin(r2) * std::sin(d))));
    const double psi = std::acos(clamp1((std::cos(d) - std::cos(r1) * std::cos(r2)) / (std::sin(r1) * std::sin(r2))));
    const double lens_over_2pi = 1.0 - (phi1 * std::cos(r1) + phi2 * std::cos(r2) + psi) / std::numbers::pi;
    return 1.0 - lens_over_2pi / (1.0 - std::cos(r1));
}

// The tool's Fs with the SAME azimuths (the same frame, the same midpoints) but the blocked interval of each azimuth in closed form instead of a radial grid with bisection: a direction at angle al from the Sun's
// centre toward azimuth ph is blocked iff A cos(al) + B sin(al) <= -T, with A = c.sat, B = (cos ph e1 + sin ph e2).sat and T = sqrt(|sat|^2 - re^2), that is iff |al - delta| >= gamma (mod 2 pi) for
// delta = atan2(B, A) and gamma = acos(-T / R), R = sqrt(A^2 + B^2): the blocked arc is [delta + gamma, delta + 2 pi - gamma], of which the part inside [0, a_s] is read off.  The brightness weight W is the
// tool's (tested apart against a quadrature).
double fs_edges(const Vec3& sun, const Vec3& sat, double re, double u, int nphi, int force_seed = -1) {
    const Vec3 to_sun = {sun[0] - sat[0], sun[1] - sat[1], sun[2] - sat[2]};
    const double ds = norm_(to_sun);
    const Vec3 c = {to_sun[0] / ds, to_sun[1] / ds, to_sun[2] / ds};
    const pc::Sky sky(std::asin(pc::kRSunM / ds), u);
    // the frame: the unit vector along the smallest component of c as the seed (the tool's rule; `force_seed` >= 0 takes that axis instead, to show what another seed would give), e1 = seed x c normalised, e2 = c x e1
    const Vec3 rule_seed = (std::fabs(c[0]) <= std::fabs(c[1]) && std::fabs(c[0]) <= std::fabs(c[2])) ? Vec3{1.0, 0.0, 0.0}
                           : (std::fabs(c[1]) <= std::fabs(c[2]))                                      ? Vec3{0.0, 1.0, 0.0}
                                                                                                       : Vec3{0.0, 0.0, 1.0};
    const Vec3 seed = force_seed < 0 ? rule_seed : Vec3{force_seed == 0 ? 1.0 : 0.0, force_seed == 1 ? 1.0 : 0.0, force_seed == 2 ? 1.0 : 0.0};
    Vec3 e1 = cross_(seed, c);
    const double n1 = norm_(e1);
    e1 = {e1[0] / n1, e1[1] / n1, e1[2] / n1};
    const Vec3 e2 = cross_(c, e1);
    const double T = std::sqrt(dot_(sat, sat) - re * re);
    const double A = dot_(c, sat);
    const double s1 = dot_(e1, sat);
    const double s2 = dot_(e2, sat);
    const double two_pi = 2.0 * std::numbers::pi;
    const double a_s = sky.a_s;
    double sum = 0.0;
    for (int j = 0; j < nphi; ++j) {
        const double ph = (static_cast<double>(j) + 0.5) * (two_pi / nphi);
        const double B = std::cos(ph) * s1 + std::sin(ph) * s2;
        const double R = std::sqrt(A * A + B * B);
        if (R <= T) continue;
        const double gamma = std::acos(-T / R);
        const double delta = std::atan2(B, A);
        double lo = std::fmod(delta + gamma, two_pi);
        if (lo < 0.0) lo += two_pi;
        const double hi = lo + (two_pi - 2.0 * gamma);
        if (lo < a_s) sum += sky.W(std::min(hi, a_s)) - sky.W(lo);
        if (hi > two_pi) sum += sky.W(std::min(hi - two_pi, a_s)) - sky.W(0.0);
    }
    return 1.0 - (sum / nphi) / sky.W(a_s);
}

// W(a) = the integral from 0 to a of I(al) sin(al) d(al), I(al) = (1 - u) + u mu, mu = sqrt(c^2 - k^2) / S (c = cos al; the cosine of the angle on the Sun's surface, the linear law of limb darkening):
// composite Simpson after the substitution al = a (1 - t^2), which removes the square-root end of the integrand at al = a_s.
double w_quadrature(double a_s, double u, double a) {
    const double S = std::sin(a_s);
    const double k = std::cos(a_s);
    const auto f = [&](double t) {
        const double al = a * (1.0 - t * t);
        const double c = std::cos(al);
        const double mu = std::sqrt(std::max(0.0, (c - k) * (c + k))) / S;
        return ((1.0 - u) + u * mu) * std::sin(al) * 2.0 * a * t;
    };
    constexpr int n = 4000;   // intervals, even
    const double h = 1.0 / n;
    double s = f(0.0) + f(1.0);
    for (int i = 1; i < n; ++i) s += f(h * i) * ((i % 2) != 0 ? 4.0 : 2.0);
    return s * h / 3.0;
}

// A Moon-like occulter (radius 1737.4 km) whose apparent radius from the satellite is `rho_over_as` times the Sun's, its centre `theta_over_as` Sun radii from the antisolar direction: the satellite is far (hundreds of
// thousands of kilometres) and the occulter small, the geometries in which a ray from the Sun's centre meets the occulter's limb twice, once, or not at all.
struct Moon {
    double re;
    Vec3 sat;
};
Moon moon_geometry(double rho_over_as, double theta_over_as) {
    const double re = 1737.4e3;
    const double D = re / std::sin(rho_over_as * kAs0);
    const double theta = theta_over_as * kAs0;
    return Moon{re, {-D * std::cos(theta), D * std::sin(theta), 0.0}};
}

// the frame the tool builds for a Sun direction c: which axis seeds it (the smallest component)
int seed_axis(const Vec3& c) {
    if (std::fabs(c[0]) <= std::fabs(c[1]) && std::fabs(c[0]) <= std::fabs(c[2])) return 0;
    return std::fabs(c[1]) <= std::fabs(c[2]) ? 1 : 2;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- NumPy's arithmetic

TEST_CASE("pairwise_sum: numpy's blocks, by hand", "[penumbral_cancellation][behaviour]") {
    // fewer than eight: in order, from -0.0 (numpy starts there to preserve -0.0)
    CHECK(std::signbit(pc::pairwise_sum(std::vector<double>{})));
    CHECK(pc::pairwise_sum(std::vector<double>{}) == 0.0);
    CHECK(std::signbit(pc::pairwise_sum(std::vector<double>{-0.0})));
    CHECK(pc::pairwise_sum(std::vector<double>{0.1, 0.2, 0.3}) == 0.6000000000000001);   // ((0.1 + 0.2) + 0.3), each rounded
    CHECK(pc::pairwise_sum(std::vector<double>{0.3, 0.2, 0.1}) == 0.6);                  // ((0.3 + 0.2) + 0.1): the order is the array's

    // eight: ((r0 + r1) + (r2 + r3)) + ((r4 + r5) + (r6 + r7)).  1e16 + 1 is a tie between 1e16 and 1e16 + 2 and goes to the even one, 1e16, so the unit is lost there and kept in the pairs:
    // ((1e16 + 1) + 2) + (2 + 2) = (1e16 + 2) + 4 = 10000000000000006; a plain sum would give 1e16.
    std::vector<double> a8 = {1e16, 1, 1, 1, 1, 1, 1, 1};
    CHECK(pc::pairwise_sum(a8) == 10000000000000006.0);
    // nine: the eight, then the rest in order: 10000000000000006 + 1 is again a tie, to the even mantissa, 10000000000000008
    std::vector<double> a9 = {1e16, 1, 1, 1, 1, 1, 1, 1, 1};
    CHECK(pc::pairwise_sum(a9) == 10000000000000008.0);
    // sixteen: the eight accumulators take a second block: r0 = 1e16 (+1 lost), r1..r7 = 2: ((1e16 + 2) + (2 + 2)) + ((2 + 2) + (2 + 2)) = 10000000000000006 + 8
    std::vector<double> a16 = {1e16, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    CHECK(pc::pairwise_sum(a16) == 10000000000000014.0);
    // 130: more than a block of 128, so the array is halved at n/2 - (n/2) % 8 = 64: the left 64 give ((1e16 + 8) + 16) + 32 = 10000000000000056 (r0 stays 1e16, r1..r7 reach 8), the right 66 ones give
    // 64 + 1 + 1 = 66; together 10000000000000122.  Halving at 65 would give another number.
    std::vector<double> a130(130, 1.0);
    a130.at(0) = 1e16;
    CHECK(pc::pairwise_sum(a130) == 10000000000000122.0);
    // 128 is still one block (not more than 128): sixteen rounds of the eight accumulators: r0 = 1e16, r1..r7 = 16; ((1e16 + 16) + 32) + 64 = 10000000000000112
    std::vector<double> a128(128, 1.0);
    a128.at(0) = 1e16;
    CHECK(pc::pairwise_sum(a128) == 10000000000000112.0);
}

TEST_CASE("np_sum, np_mean: the identity and the division", "[penumbral_cancellation][behaviour]") {
    CHECK_FALSE(std::signbit(pc::np_sum(std::vector<double>{})));   // 0.0 + (-0.0) is +0.0: np.sum of nothing is +0.0
    CHECK(pc::np_sum(std::vector<double>{}) == 0.0);
    CHECK(pc::np_sum(std::vector<double>{-0.0}) == 0.0);
    CHECK_FALSE(std::signbit(pc::np_sum(std::vector<double>{-0.0})));
    CHECK(pc::np_sum(std::vector<double>{1.0, 2.0, 3.0, 4.0}) == 10.0);
    CHECK(pc::np_mean(std::vector<double>{1.0, 2.0, 3.0, 4.0}) == 2.5);
    CHECK(pc::np_mean(std::vector<double>{5.0}) == 5.0);
    CHECK(std::isnan(pc::np_mean(std::vector<double>{})));   // 0 / 0: numpy's mean of nothing is nan
    std::vector<double> a9 = {1e16, 1, 1, 1, 1, 1, 1, 1, 1};
    CHECK(pc::np_mean(a9) == 10000000000000008.0 / 9.0);   // the pairwise sum, then the division
}

TEST_CASE("np_trapezoid: the sum of d * (y1 + y0) / 2 and its refusals", "[penumbral_cancellation][behaviour]") {
    // y = x^2 at x = 0, 1, 2: (1 * (1 + 0) / 2) + (1 * (4 + 1) / 2) = 0.5 + 2.5
    CHECK(pc::np_trapezoid(std::vector<double>{0.0, 1.0, 4.0}, std::vector<double>{0.0, 1.0, 2.0}) == 3.0);
    // unequal steps: x = 0, 1, 3 and y = 1, 3, 2: (1 * (3 + 1) / 2) + (2 * (2 + 3) / 2) = 2 + 5
    CHECK(pc::np_trapezoid(std::vector<double>{1.0, 3.0, 2.0}, std::vector<double>{0.0, 1.0, 3.0}) == 7.0);
    // the sign of a descending abscissa is kept
    CHECK(pc::np_trapezoid(std::vector<double>{1.0, 1.0}, std::vector<double>{2.0, 0.0}) == -2.0);
    // fewer than two points: nothing to integrate
    CHECK(pc::np_trapezoid(std::vector<double>{}, std::vector<double>{}) == 0.0);
    CHECK(pc::np_trapezoid(std::vector<double>{7.0}, std::vector<double>{3.0}) == 0.0);
    CHECK_THROWS_AS(pc::np_trapezoid(std::vector<double>{1.0, 2.0}, std::vector<double>{0.0, 1.0, 2.0}), std::invalid_argument);
    CHECK_THROWS_AS(pc::np_trapezoid(std::vector<double>{1.0, 2.0, 3.0}, std::vector<double>{0.0, 1.0}), std::invalid_argument);
    try {
        (void)pc::np_trapezoid(std::vector<double>{1.0, 2.0}, std::vector<double>{0.0, 1.0, 2.0});
        FAIL("no exception");
    } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()) == "np_trapezoid: x and y differ in length");
    }
    // eight points are seven terms, and fewer than eight terms are summed IN ORDER from -0.0 -- not as eight accumulators with a zero in the eighth, which rounds differently.  A series of y is searched (a fixed
    // generator, x = 0 ... 7) for which the two ways differ in the last bit, so that this case can tell them apart; the sum must be the in-order one, bit for bit
    {
        std::vector<double> x8(8);
        for (std::size_t i = 0; i < x8.size(); ++i) x8.at(i) = static_cast<double>(i);
        std::vector<double> y8(8);
        double in_order = 0.0;
        double paired = 0.0;
        bool found = false;
        std::uint64_t state = 12345;
        for (int tries = 0; tries < 400 && !found; ++tries) {
            for (double& v : y8) {
                state = state * 6364136223846793005ULL + 1442695040888963407ULL;
                v = static_cast<double>(state >> 11) / 9007199254740992.0;   // 53 random bits, in [0, 1)
            }
            std::array<double, 7> t{};
            for (std::size_t i = 0; i < t.size(); ++i) t.at(i) = (1.0 * (y8.at(i + 1) + y8.at(i))) / 2.0;
            in_order = -0.0;
            for (const double v : t) in_order += v;
            paired = ((t.at(0) + t.at(1)) + (t.at(2) + t.at(3))) + ((t.at(4) + t.at(5)) + (t.at(6) + 0.0));
            found = bits_of(in_order) != bits_of(paired);
        }
        REQUIRE(found);
        CHECK(bits_of(pc::np_trapezoid(y8, x8)) == bits_of(in_order));
    }
    // the terms are summed pairwise (np.trapezoid sums the array of terms): nine terms of d = 1 and (y1 + y0) / 2 = 1, but the first is 1e16 + 2
    std::vector<double> y(10, 1.0);
    y.at(0) = 1e16;   // first term (1 * (1 + 1e16) / 2) = 5e15 (the tie goes to even: 1e16 + 1 -> 1e16), the others 1
    std::vector<double> x(10);
    for (std::size_t i = 0; i < x.size(); ++i) x.at(i) = static_cast<double>(i);
    CHECK(pc::np_trapezoid(y, x) == 5e15 + 8.0);   // terms: 5e15, then eight ones: the eight accumulators r0 = 5e15, r1..r7 = 1; ((5e15 + 1) + 2) + (2 + 2) = 5e15 + 7, then + 1 (the ninth) = 5e15 + 8
}

TEST_CASE("np_cumsum: the running sum in order", "[penumbral_cancellation][behaviour]") {
    CHECK(pc::np_cumsum(std::vector<double>{}).empty());
    const std::vector<double> c = pc::np_cumsum(std::vector<double>{1.0, 2.0, 3.0});
    REQUIRE(c.size() == 3);
    CHECK(c.at(0) == 1.0);
    CHECK(c.at(1) == 3.0);
    CHECK(c.at(2) == 6.0);
    const std::vector<double> d = pc::np_cumsum(std::vector<double>{0.1, 0.2, 0.3});   // in order, each rounded: 0.1, 0.30000000000000004, 0.6000000000000001
    REQUIRE(d.size() == 3);
    CHECK(d.at(0) == 0.1);
    CHECK(d.at(1) == 0.30000000000000004);
    CHECK(d.at(2) == 0.6000000000000001);
    const std::vector<double> one = pc::np_cumsum(std::vector<double>{-0.0});   // the first element is copied, not added to 0.0: -0.0 stays
    REQUIRE(one.size() == 1);
    CHECK(std::signbit(one.at(0)));
}

TEST_CASE("cross, dot3, norm3 on small integers", "[penumbral_cancellation][behaviour]") {
    const Vec3 ex = {1, 0, 0}, ey = {0, 1, 0}, ez = {0, 0, 1};
    CHECK(pc::cross(ex, ey) == ez);
    CHECK(pc::cross(ey, ez) == ex);
    CHECK(pc::cross(ez, ex) == ey);
    CHECK(pc::cross(ey, ex) == Vec3{0, 0, -1});
    const Vec3 a = {1, 2, 3}, b = {4, 5, 6};
    CHECK(pc::cross(a, b) == Vec3{-3, 6, -3});   // (2 * 6 - 3 * 5, 3 * 4 - 1 * 6, 1 * 5 - 2 * 4)
    CHECK(pc::cross(b, a) == Vec3{3, -6, 3});
    CHECK(pc::cross(a, a) == Vec3{0, 0, 0});
    CHECK(pc::dot3(a, b) == 32.0);   // 4 + 10 + 18
    CHECK(pc::dot3(a, Vec3{-1, 2, -1}) == 0.0);
    CHECK(pc::norm3(Vec3{3, 4, 12}) == 13.0);
    CHECK(pc::norm3(Vec3{0, 0, 0}) == 0.0);
    CHECK(pc::norm3(Vec3{-2, 3, 6}) == 7.0);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the brightness weight

TEST_CASE("Sky: W against a quadrature, and the uniform disc", "[penumbral_cancellation][behaviour]") {
    for (const double u : {0.0, 0.3, 0.6, 0.9, 1.0}) {   // u = 1: the whole disc is limb-darkened, the term (1 - u)(1 - cos a) is gone and only the F-difference stays
        const pc::Sky sky(kAs0, u);
        CHECK(sky.a_s == kAs0);
        CHECK(sky.u == u);
        CHECK(sky.S == std::sin(kAs0));
        CHECK(sky.k == std::cos(kAs0));
        CHECK(sky.W(0.0) == 0.0);   // nothing is blocked out to the Sun's centre
        for (const double fraction : {0.25, 0.5, 0.9, 1.0}) {
            const double a = fraction * kAs0;
            const double expected = w_quadrature(kAs0, u, a);
            // the closed form subtracts two nearly equal values (F(1) - F(c)); the relative agreement with the quadrature is 4e-9 at worst here for u > 0 (5e-9 at a quarter of the radius) and 3e-11 for u = 0
            INFO("u = " << u << ", a / a_s = " << fraction);
            CHECK(std::fabs(sky.W(a) - expected) <= (u == 0.0 ? 1e-9 : 1e-7) * expected);
        }
    }
    // u = 0: W(a) = 1 - cos(a), exactly the tool's expression
    const pc::Sky uniform(kAs0, 0.0);
    for (const double a : {1e-4, 2e-3, kAs0}) CHECK(uniform.W(a) == 1.0 - std::cos(a));
    // W grows with the angle
    const pc::Sky eddington(kAs0, pc::kEddingtonU);
    double previous = 0.0;
    for (int i = 1; i <= 20; ++i) {
        const double w = eddington.W(kAs0 * i / 20.0);
        CHECK(w > previous);
        previous = w;
    }
}

TEST_CASE("Sky: the disc mean of the linear law, 1 - u/3, to the sphericity of the Sun", "[penumbral_cancellation][behaviour]") {
    // PROVENANCE 25.10: the radial weight "checked against its own disc mean 1 - u/3 (agreeing to 4e-7, the residual being sphericity, since 1 - u/3 is the flat-disc limit)" -- quoted to one figure at u = 3/5
    const double ratio = pc::Sky(kAs0, pc::kEddingtonU).W(kAs0) / pc::Sky(kAs0, 0.0).W(kAs0);
    CHECK(std::fabs(ratio - (1.0 - pc::kEddingtonU / 3.0)) >= 3.5e-7);
    CHECK(std::fabs(ratio - (1.0 - pc::kEddingtonU / 3.0)) <= 4.5e-7);
    // and for other u the deficit scales with u and stays below the Sun's solid angle fraction, a_s^2 (the order of the curvature of the sky)
    for (const double u : {0.1, 0.5, 0.9}) {
        const double r = pc::Sky(kAs0, u).W(kAs0) / pc::Sky(kAs0, 0.0).W(kAs0);
        CHECK(std::fabs(r - (1.0 - u / 3.0)) < kAs0 * kAs0);
        CHECK(r < 1.0 - u / 3.0);   // the sphere is a little less bright on average than the flat disc: the deficit is negative
    }
    // the constants of the tool: Eddington's u = 3/5, the unit conversions it quotes
    CHECK(pc::kEddingtonU == 0.6);
    CHECK(pc::kAuM == 1.495978707e11);
    CHECK(pc::kRSunM == 6.957e8);
    CHECK(pc::kREarthM == 6378137.0);
    CHECK(pc::kGmEarth == 3.986004415e14);
    CHECK(pc::kSunPosition == Vec3{pc::kAuM, 0.0, 0.0});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- Fs

TEST_CASE("fs: full sunlight, the umbra and the refusal of nphi < 1", "[penumbral_cancellation][behaviour]") {
    for (const int nphi : {1, 2, 7, 450, 451}) {
        for (const double u : {0.0, 0.6}) {
            INFO("nphi = " << nphi << ", u = " << u);
            // on the sunward side nothing is blocked: no flux at all, Fs is exactly one
            CHECK(pc::fs(kSun, Vec3{kRLeo, 0.0, 0.0}, pc::kREarthM, u, nphi) == 1.0);
            CHECK(pc::fs(kSun, Vec3{0.0, kRLeo, 0.0}, pc::kREarthM, u, nphi) == 1.0);
            // behind the Earth the whole disc is blocked in every direction: Fs is zero to rounding (the mean of equal values over their common value)
            CHECK(std::fabs(pc::fs(kSun, leo_at(0.0), pc::kREarthM, u, nphi)) <= 1e-14);
        }
    }
    CHECK_THROWS_AS(pc::fs(kSun, leo_at(1.0), pc::kREarthM, 0.0, 0), std::invalid_argument);
    CHECK_THROWS_AS(pc::fs(kSun, leo_at(1.0), pc::kREarthM, 0.0, -5), std::invalid_argument);
    try {
        (void)pc::fs(kSun, leo_at(1.0), pc::kREarthM, 0.0, 0);
        FAIL("no exception");
    } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()) == "fs: nphi must be at least 1");
    }
}

TEST_CASE("fs: the baseline penumbra against the closed-form lens (u = 0)", "[penumbral_cancellation][behaviour]") {
    // positions 60.20 ... 60.74 degrees from the antisolar point: the Earth's limb crosses the Sun's disc (the Earth's angular radius at this height is 60.47 degrees, the Sun's 0.2664).  The sweep is a midpoint rule over
    // the azimuth whose integrand has square-root ends, so its error falls like nphi^-1.5 and is erratic: measured against the lens, at most 1.2e-3 at nphi 450 and 3.7e-6 at nphi 7200 on the worst samples of the
    // five passages (C10_penumbral_exploration_facts.txt, 1b and 1d).  The 28 positions of THIS case gave 1.85e-4 at nphi 450 and 2.18e-6 at nphi 7200 (measured once, apart from the tool, before the case was kept);
    // the tolerances are 3 and 5 times those: 6e-4 and 1e-5.
    double worst_450 = 0.0;
    double worst_7200 = 0.0;
    for (int i = 0; i <= 27; ++i) {
        const double theta = (60.20 + 0.02 * i) * std::numbers::pi / 180.0;
        const Vec3 sat = leo_at(theta);
        const double lens = lens_fs_u0(kSun, sat, pc::kREarthM);
        worst_450 = std::max(worst_450, std::fabs(pc::fs(kSun, sat, pc::kREarthM, 0.0, 450) - lens));
        worst_7200 = std::max(worst_7200, std::fabs(pc::fs(kSun, sat, pc::kREarthM, 0.0, 7200) - lens));
    }
    INFO("worst |fs - lens| at nphi 450: " << worst_450 << ", at nphi 7200: " << worst_7200);
    CHECK(worst_450 < 6e-4);
    CHECK(worst_7200 < 1e-5);
    CHECK(worst_7200 < worst_450);
}

TEST_CASE("fs: a Moon-like occulter, every branch of the sweep, against the lens and against the closed-form edges", "[penumbral_cancellation][behaviour]") {
    struct Case {
        const char* name;
        double rho, theta;   // the occulter's apparent radius and its offset from the antisolar direction, in Sun radii
        double expected_u0;  // -1: the lens decides (partial overlap)
    };
    const std::vector<Case> cases = {
        {"contained, the Sun's centre uncovered: a ray meets the limb twice", 0.3, 0.5, -1.0},
        {"partial, centre uncovered: one crossing", 0.6, 1.2, -1.0},
        {"contained, centre covered: one crossing at the far side of the occulter", 0.6, 0.3, -1.0},
        {"larger than the Sun, centre covered, partial", 2.0, 1.5, -1.0},
        {"larger than the Sun, centre uncovered, partial", 2.0, 2.5, -1.0},
        {"no overlap", 0.6, 1.7, 1.0},
        {"the Sun inside the occulter", 2.0, 0.5, 0.0},
    };
    for (const Case& c : cases) {
        const Moon m = moon_geometry(c.rho, c.theta);
        INFO(c.name);
        const double lens = lens_fs_u0(kSun, m.sat, m.re);
        if (c.expected_u0 >= 0.0) CHECK(lens == c.expected_u0);
        for (const int nphi : {450, 3600}) {
            INFO("nphi = " << nphi);
            const double p0 = pc::fs(kSun, m.sat, m.re, 0.0, nphi);
            const double p6 = pc::fs(kSun, m.sat, m.re, 0.6, nphi);
            // the two sweeps use the same azimuths: they agree to the rounding of the hit test, whose angular resolution is about 1e-16 / (apparent radius) = 4e-14 rad for these small occulters (3e-11 measured at worst)
            CHECK(std::fabs(p0 - fs_edges(kSun, m.sat, m.re, 0.0, nphi)) < 1e-9);
            CHECK(std::fabs(p6 - fs_edges(kSun, m.sat, m.re, 0.6, nphi)) < 1e-9);
            // and the uniform disc converges to the lens: at most 3.2e-5 at nphi 450 and 1.2e-6 at nphi 3600 measured for these geometries
            CHECK(std::fabs(p0 - lens) < (nphi == 450 ? 1e-4 : 5e-6));
        }
    }
}

TEST_CASE("fs: the second method at the baseline penumbra and with the limb darkened", "[penumbral_cancellation][behaviour]") {
    // the positions of the previous case, at the resolutions of the tool's calls; the tolerance is the one fixed in advance from the measured agreement over ALL samples of the five passages
    // (C10_proof_registration.txt, AMENDMENT 2, R4): 16 times the measured maximum, to one figure -- 2e-10 for u = 0.6 (measured 1.25e-11) and 2e-12 for u = 0 (measured 1.3e-13)
    double worst6 = 0.0;
    double worst0 = 0.0;
    for (int i = 0; i <= 27; ++i) {
        const Vec3 sat = leo_at((60.20 + 0.02 * i) * std::numbers::pi / 180.0);
        for (const int nphi : {150, 450}) {
            worst6 = std::max(worst6, std::fabs(pc::fs(kSun, sat, pc::kREarthM, 0.6, nphi) - fs_edges(kSun, sat, pc::kREarthM, 0.6, nphi)));
            worst0 = std::max(worst0, std::fabs(pc::fs(kSun, sat, pc::kREarthM, 0.0, nphi) - fs_edges(kSun, sat, pc::kREarthM, 0.0, nphi)));
        }
    }
    INFO("worst |port - edges|: u = 0.6: " << worst6 << ", u = 0: " << worst0);
    CHECK(worst6 < 2e-10);
    CHECK(worst0 < 2e-12);
}

TEST_CASE("fs: the three seeds of the frame give the same sweep, and the answer does not depend on the frame", "[penumbral_cancellation][behaviour]") {
    // (1) The baseline position at 60.47 degrees (half the Sun covered) with the axes permuted so that the SMALLEST component of the direction to the Sun, which seeds the tool's frame, is c[2], c[1] and c[0] in
    // turn.  A permutation of the axes turns the whole configuration and the seed with it, so the three sweeps are the SAME sweep up to rounding: they must agree to 1e-12 (measured 2e-16), and each converges to
    // the lens (measured 1.58e-6 at nphi 7200, the same for the three).  A seed rule that chose another axis would give another frame, another quadrature error, and break the agreement.
    const Vec3 sat0 = leo_at(60.47 * std::numbers::pi / 180.0);
    struct Turned {
        Vec3 sun;
        Vec3 sat;
        int expected_seed;
    };
    const std::vector<Turned> turned = {
        {kSun, sat0, 2},                                                // identity: c = (1, -4e-5, 0): the smallest component is the last
        {Vec3{0.0, 0.0, pc::kAuM}, Vec3{sat0[1], sat0[2], sat0[0]}, 1}, // (x, y, z) -> (y, z, x): c = (4e-5, 0, 1)
        {Vec3{0.0, 0.0, pc::kAuM}, Vec3{sat0[2], sat0[1], sat0[0]}, 0}, // (x, y, z) -> (z, y, x): c = (0, -4e-5, 1)
        // the three other orders of the components: the smallest is the second, the third, and the first again -- with the SECOND smallest in the last place (c = (0, 1, -4e-5): x is the seed although |c[1]| > |c[2]|)
        {kSun, Vec3{sat0[0], sat0[2], sat0[1]}, 1},                     // (x, y, z) -> (x, z, y): c = (1, 0, -4e-5)
        {Vec3{0.0, pc::kAuM, 0.0}, Vec3{sat0[1], sat0[0], sat0[2]}, 2}, // (x, y, z) -> (y, x, z): c = (-4e-5, 1, 0)
        {Vec3{0.0, pc::kAuM, 0.0}, Vec3{sat0[2], sat0[0], sat0[1]}, 0}, // (x, y, z) -> (z, x, y): c = (0, 1, -4e-5)
    };
    const double lens = lens_fs_u0(kSun, sat0, pc::kREarthM);
    std::vector<double> sweeps;
    for (const Turned& t : turned) {
        const Vec3 to_sun = {t.sun[0] - t.sat[0], t.sun[1] - t.sat[1], t.sun[2] - t.sat[2]};
        const double ds = norm_(to_sun);
        const Vec3 c = {to_sun[0] / ds, to_sun[1] / ds, to_sun[2] / ds};
        REQUIRE(seed_axis(c) == t.expected_seed);   // the case really takes the branch it is here for
        INFO("seed axis " << t.expected_seed);
        const double sweep = pc::fs(t.sun, t.sat, pc::kREarthM, 0.0, 7200);
        sweeps.push_back(sweep);
        CHECK(std::fabs(sweep - lens) < 1e-5);
        // and with the same frame the closed-form edges agree to the tolerance of the passages
        CHECK(std::fabs(pc::fs(t.sun, t.sat, pc::kREarthM, 0.6, 450) - fs_edges(t.sun, t.sat, pc::kREarthM, 0.6, 450)) < 2e-10);
    }
    REQUIRE(sweeps.size() == 6);
    for (std::size_t k = 1; k < sweeps.size(); ++k) CHECK(std::fabs(sweeps.at(0) - sweeps.at(k)) < 1e-12);

    // (2) Turning the satellite about the Sun's axis by an angle moves the azimuths of the sweep against the geometry: the sweeps differ at the level of the quadrature error (of order 1e-6 at nphi 7200 and 1e-4 at
    // nphi 450 here), all of them converge to the same lens, and the closed-form edges follow each one.
    std::vector<double> turned_sweeps;
    for (const double angle : {0.0, 0.7, 1.9, 3.5}) {
        const Vec3 sat = {sat0[0], sat0[1] * std::cos(angle), sat0[1] * std::sin(angle)};
        INFO("angle " << angle);
        const double sweep = pc::fs(kSun, sat, pc::kREarthM, 0.0, 7200);
        turned_sweeps.push_back(sweep);
        CHECK(std::fabs(sweep - lens) < 1e-5);
        CHECK(std::fabs(pc::fs(kSun, sat, pc::kREarthM, 0.6, 450) - fs_edges(kSun, sat, pc::kREarthM, 0.6, 450)) < 2e-10);
        CHECK(std::fabs(pc::fs(kSun, sat, pc::kREarthM, 0.0, 450) - fs_edges(kSun, sat, pc::kREarthM, 0.0, 450)) < 2e-12);
    }
    CHECK(*std::max_element(turned_sweeps.begin(), turned_sweeps.end()) - *std::min_element(turned_sweeps.begin(), turned_sweeps.end()) < 2e-5);
}

TEST_CASE("fs: the seed of the frame when two components of the direction to the Sun are equal (the rule is  <=  in both comparisons)", "[penumbral_cancellation][behaviour]") {
    // The tool seeds its frame with the axis of the smallest component of the direction c to the Sun, the first of equal ones in the order x, y, z.  The sweep's azimuths are fixed in that frame, so another seed is
    // another frame, another quadrature error (1e-4 at nphi 450 here), and the closed-form edges -- which use the same azimuths -- no longer agree to 1e-10.  Three geometries with an EXACT tie (the two equal
    // components are the same double: the same subtraction of the same numbers):
    //   (a) Sun on +z, the satellite at (s, s, z):  c = (-s, -s, AU - z) / |.|  with |c[0]| == |c[1]| < |c[2]|    -> the seed is x (the first comparison is  <=,  not  <)
    //   (b) Sun on +y, the satellite at (s, y, s):  c = (-s, AU - y, -s) / |.|  with |c[0]| == |c[2]| < |c[1]|    -> the seed is x (the second comparison is  <=,  not  <)
    //   (c) Sun on +x, the satellite at (x, s, s):  c = (AU - x, -s, -s) / |.|  with |c[1]| == |c[2]| < |c[0]|    -> the seed is y (the comparison of the second and the third is  <=,  not  <)
    // each at the angle 60.47 degrees from the antisolar point (half the Sun covered): s = r sin(theta) / sqrt(2), so that the satellite is at the baseline's distance from the shadow's axis.
    const double theta = 60.47 * std::numbers::pi / 180.0;
    const double s = kRLeo * std::sin(theta) / std::sqrt(2.0);
    const double depth = -kRLeo * std::cos(theta);
    struct Tie {
        const char* name;
        Vec3 sun;
        Vec3 sat;
        int expected_seed;
    };
    const std::vector<Tie> ties = {
        {"(a) x and y equal", Vec3{0.0, 0.0, pc::kAuM}, Vec3{s, s, depth}, 0},
        {"(b) x and z equal", Vec3{0.0, pc::kAuM, 0.0}, Vec3{s, depth, s}, 0},
        {"(c) y and z equal", Vec3{pc::kAuM, 0.0, 0.0}, Vec3{depth, s, s}, 1},
    };
    for (const Tie& t : ties) {
        INFO(t.name);
        const Vec3 to_sun = {t.sun[0] - t.sat[0], t.sun[1] - t.sat[1], t.sun[2] - t.sat[2]};
        const double ds = norm_(to_sun);
        const Vec3 c = {to_sun[0] / ds, to_sun[1] / ds, to_sun[2] / ds};
        REQUIRE(seed_axis(c) == t.expected_seed);
        const std::array<double, 3> mags = {std::fabs(c[0]), std::fabs(c[1]), std::fabs(c[2])};
        const double smallest = *std::min_element(mags.begin(), mags.end());
        int equal_smallest = 0;
        for (const double m : mags) {
            if (m == smallest) ++equal_smallest;
        }
        REQUIRE(equal_smallest == 2);   // the tie is exact, and it is between the two smallest
        // 149 and 451 azimuths, not 150 and 450: the three satellites stand on a diagonal (45 degrees between two axes), and a frame turned by 90 degrees about the Sun's axis puts the grid at the mirror image of its
        // phase against the shadow's axis when the number of azimuths is 2 mod 4, which gives the same sweep -- for 149 and 451 it does not
        for (const int nphi : {149, 451}) {
            INFO("nphi = " << nphi);
            CHECK(std::fabs(pc::fs(t.sun, t.sat, pc::kREarthM, 0.6, nphi) - fs_edges(t.sun, t.sat, pc::kREarthM, 0.6, nphi)) < 2e-10);
            CHECK(std::fabs(pc::fs(t.sun, t.sat, pc::kREarthM, 0.0, nphi) - fs_edges(t.sun, t.sat, pc::kREarthM, 0.0, nphi)) < 2e-12);
            // the case can tell the seeds apart: the other two axes give another sweep here, by more than 1e-7 (a vacuous case would not)
            for (int other = 0; other < 3; ++other) {
                if (other == t.expected_seed) continue;
                INFO("the seed would be axis " << other);
                CHECK(std::fabs(fs_edges(t.sun, t.sat, pc::kREarthM, 0.0, nphi, other) - fs_edges(t.sun, t.sat, pc::kREarthM, 0.0, nphi)) > 1e-7);
            }
        }
    }
}

TEST_CASE("fs: a crossing inside the first radial cell of the grid (the Sun's centre covered or uncovered by 0.002 of the Sun's radius)", "[penumbral_cancellation][behaviour]") {
    // The radial grid has 220 steps of the Sun's radius; its first cell is 0.0045 of it.  When the Sun's centre is within that distance of the occulter's limb, the ray from the centre toward (or away from) the
    // limb meets it between the samples 0 and 1 -- for most azimuths, since the distance to the limb hardly changes over the azimuths on the far side.  A Moon-like occulter 0.6 Sun radii in radius whose
    // centre stands 0.598 (the Sun's centre covered by 0.002) or 0.602 (uncovered by 0.002) Sun radii from the antisolar direction.
    // (The satellite is 6e8 m from the occulter, 0.4 % of an astronomical unit, so the angle from the Sun's centre to the occulter's is the offset less delta = atan2(D sin(theta), AU + D cos(theta)); the offset is
    // found by iterating  theta = separation + delta(theta), and the separation is then checked below with the tool's own Sun direction.)
    for (const double separation_over_as : {0.598, 0.602}) {
        const double D = 1737.4e3 / std::sin(0.6 * kAs0);
        double theta = separation_over_as * kAs0;
        for (int i = 0; i < 12; ++i) theta = separation_over_as * kAs0 + std::atan2(D * std::sin(theta), pc::kAuM + D * std::cos(theta));
        const double theta_over_as = theta / kAs0;
        const Moon m = moon_geometry(0.6, theta_over_as);
        const Vec3 to_sun = {kSun[0] - m.sat[0], kSun[1] - m.sat[1], kSun[2] - m.sat[2]};
        const double ds = norm_(to_sun);
        const Vec3 c = {to_sun[0] / ds, to_sun[1] / ds, to_sun[2] / ds};
        const double a_s = std::asin(pc::kRSunM / ds);
        const double d = std::atan2(norm_(cross_(c, m.sat)), -dot_(c, m.sat));   // the angle from the Sun's centre to the occulter's
        const double r2 = std::asin(m.re / norm_(m.sat));
        INFO("separation " << separation_over_as << " Sun radii, offset " << theta_over_as);
        REQUIRE(std::fabs(d - r2) < 0.0045 * a_s);                       // the limb is within the first cell of the centre
        REQUIRE(std::fabs(d - r2) > 0.0010 * a_s);                       // and not on top of it
        REQUIRE((d < r2) == (separation_over_as < 0.6));                 // covered, or not
        for (const int nphi : {450, 3600}) {
            INFO("nphi = " << nphi);
            CHECK(std::fabs(pc::fs(kSun, m.sat, m.re, 0.0, nphi) - fs_edges(kSun, m.sat, m.re, 0.0, nphi)) < 1e-9);
            CHECK(std::fabs(pc::fs(kSun, m.sat, m.re, 0.6, nphi) - fs_edges(kSun, m.sat, m.re, 0.6, nphi)) < 1e-9);
        }
    }
}

TEST_CASE("fs: a limb-darkened Sun gives more light than a uniform one while less than half is blocked and less once more is (antisymmetry about 50 %)", "[penumbral_cancellation][behaviour]") {
    // SHDW-R-030..032: the error of the uniform disc is antisymmetric about 50 % occultation, because a radially symmetric profile puts exactly half its flux either side of a central chord
    const auto du = [](double theta_deg) {
        const Vec3 sat = leo_at(theta_deg * std::numbers::pi / 180.0);
        return pc::fs(kSun, sat, pc::kREarthM, pc::kEddingtonU, 1800) - pc::fs(kSun, sat, pc::kREarthM, 0.0, 1800);
    };
    CHECK(du(60.30) < -0.015);   // 1 - Fs = 0.865: the bright centre is still visible less than the uniform disc says -- measured -0.0190
    CHECK(du(60.62) > 0.015);    // 1 - Fs = 0.149 -- measured +0.0190
    CHECK(std::fabs(du(60.47)) < 3e-3);   // near the middle of the passage -- measured +0.0013 at 1 - Fs = 0.485 (the zero is at 0.506)
    // the peak effect (SPEC-shadow's table: 1.97e-2) is the largest |du| along the passage
    double peak = 0.0;
    for (int i = 0; i <= 54; ++i) peak = std::max(peak, std::fabs(du(60.20 + 0.01 * i)));
    CHECK(peak > 1.96e-2);
    CHECK(peak < 1.99e-2);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- Kepler propagation

TEST_CASE("Orbit: a circular orbit is the closed form", "[penumbral_cancellation][behaviour]") {
    const Vec3 peri = {-1.0, 0.0, 0.0};
    const Vec3 q = {0.0, -1.0, 0.0};
    const pc::Orbit orb(kRLeo, 0.0, peri, q);
    const double n = std::sqrt(pc::kGmEarth / (kRLeo * kRLeo * kRLeo));
    CHECK(orb.a == kRLeo);
    CHECK(orb.e == 0.0);
    CHECK(orb.peri_hat == peri);
    CHECK(orb.q_hat == q);
    CHECK(std::fabs(orb.n_mean - n) < 1e-15 * n);
    CHECK(std::fabs(orb.period() - 2.0 * std::numbers::pi / n) < 1e-9);
    // r(t) = a (cos(n t) peri + sin(n t) q); a is 7e6 m and the doubles have 1.1e-16 relative: 1e-9 m is generous
    for (const double fraction : {0.0, 0.125, 0.25, 0.5, 0.7, 0.9, -0.3}) {
        const double t = fraction * orb.period();
        const Vec3 p = orb.pos(t);
        const double ang = orb.n_mean * t;   // the tool's own n (agreeing with the closed form to 1e-15, checked above): a 1e-16 relative difference in n would move a position by 7e-9 m at the end of the period
        CHECK(std::fabs(p[0] - kRLeo * (std::cos(ang) * peri[0] + std::sin(ang) * q[0])) < 1e-8);
        CHECK(std::fabs(p[1] - kRLeo * (std::cos(ang) * peri[1] + std::sin(ang) * q[1])) < 1e-8);
        CHECK(std::fabs(p[2]) < 1e-8);
        CHECK(std::fabs(norm_(p) - kRLeo) < 1e-8);
    }
    CHECK(orb.pos(0.0) == Vec3{-kRLeo, 0.0, 0.0});   // E = 0: r = a, cos(nu) = 1, sin(nu) = 0
}


// Kepler's equation E - e sin E = M solved by BISECTION, a method of its own (the tool's is Newton's, 80 steps); the position in the orbital plane in the perifocal form a (cos E - e), a sqrt(1 - e^2) sin E
Vec3 kepler_by_bisection(double a, double e, const Vec3& peri, const Vec3& q, double t) {
    const double n = std::sqrt(pc::kGmEarth / (a * a * a));
    double M = std::fmod(n * t, 2.0 * std::numbers::pi);
    if (M > std::numbers::pi) M -= 2.0 * std::numbers::pi;
    if (M < -std::numbers::pi) M += 2.0 * std::numbers::pi;
    double lo = -std::numbers::pi, hi = std::numbers::pi;
    for (int i = 0; i < 200; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (mid - e * std::sin(mid) < M) lo = mid; else hi = mid;
    }
    const double E = 0.5 * (lo + hi);
    const double x = a * (std::cos(E) - e);
    const double y = a * std::sqrt(1.0 - e * e) * std::sin(E);
    return {x * peri[0] + y * q[0], x * peri[1] + y * q[1], x * peri[2] + y * q[2]};
}


TEST_CASE("Orbit: eccentric orbits against a bisection solver, the apsides, the time symmetry and the conservation laws", "[penumbral_cancellation][behaviour]") {
    for (const double e : {0.3, 0.5, 0.85}) {
        const double a = (pc::kREarthM + 600.0e3) / (1.0 - e);
        const Vec3 peri = {0.6, 0.0, 0.8};   // unit; q perpendicular to it and to nothing else in particular
        const Vec3 q = {0.0, 1.0, 0.0};
        const pc::Orbit orb(a, e, peri, q);
        const double T = orb.period();
        INFO("e = " << e);
        // the apsides
        const Vec3 p0 = orb.pos(0.0);
        CHECK(std::fabs(norm_(p0) - a * (1.0 - e)) < 1e-6);
        const Vec3 pa = orb.pos(0.5 * T);
        CHECK(std::fabs(norm_(pa) - a * (1.0 + e)) < 1e-6 * (1.0 + e));
        CHECK(dot_(pa, peri) < 0.0);   // apoapsis lies opposite to periapsis
        // against the solver of its own method, all around the orbit (including negative times, where the tool starts Newton at E = M from the other side)
        for (const double fraction : {-0.45, -0.3, -0.1, -0.01, 0.01, 0.05, 0.2, 0.4, 0.499, 0.6, 0.8, 0.95}) {
            const Vec3 p = orb.pos(fraction * T);
            const Vec3 b = kepler_by_bisection(a, e, peri, q, fraction * T);
            CHECK(std::fabs(p[0] - b[0]) < 1e-5);
            CHECK(std::fabs(p[1] - b[1]) < 1e-5);
            CHECK(std::fabs(p[2] - b[2]) < 1e-5);
        }
        // time symmetry about periapsis, EXACTLY: sin is odd and cos even, and Newton's iteration from E = M keeps the symmetry bit for bit, so the radius is the same double at t and -t, and the components along
        // periapsis are the same doubles while those along q change sign (PROVENANCE 25.11: r(-t) = r(t) identically)
        for (const double fraction : {0.013, 0.1, 0.37, 0.49}) {
            const Vec3 plus = orb.pos(fraction * T);
            const Vec3 minus = orb.pos(-fraction * T);
            CHECK(bits_of(norm_(plus)) == bits_of(norm_(minus)));
            CHECK(std::fabs(dot_(plus, peri) - dot_(minus, peri)) < 1e-6);
            CHECK(std::fabs(dot_(plus, q) + dot_(minus, q)) < 1e-6);
        }
        // the angular momentum and the energy, from central differences of the positions (h = 1 s; the truncation is (h^2 / 6) r''' ~ 1e-12 relative)
        const double h_expected = std::sqrt(pc::kGmEarth * a * (1.0 - e * e));
        for (const double fraction : {0.05, 0.3, 0.6, 0.9}) {
            const double t = fraction * T;
            const Vec3 p1 = orb.pos(t + 1.0), m1 = orb.pos(t - 1.0), p2 = orb.pos(t + 2.0), m2 = orb.pos(t - 2.0);
            Vec3 v{};
            for (std::size_t k = 0; k < 3; ++k) v.at(k) = (8.0 * (p1.at(k) - m1.at(k)) - (p2.at(k) - m2.at(k))) / 12.0;   // the five-point stencil
            const Vec3 p = orb.pos(t);
            CHECK(std::fabs(norm_(cross_(p, v)) - h_expected) < 1e-7 * h_expected);
            const double r = norm_(p);
            const double v2 = dot_(v, v);
            CHECK(std::fabs(v2 - pc::kGmEarth * (2.0 / r - 1.0 / a)) < 1e-7 * v2);   // vis-viva
        }
    }
}

TEST_CASE("Orbit: a general plane (every component of the perifocal axes non-zero) and eccentricities up to 0.97, against the bisection solver", "[penumbral_cancellation][behaviour]") {
    // peri = (0.6, 0, 0.8) and q = (0.4, sqrt(0.75), -0.3) are perpendicular unit vectors with no zero component in q, so a wrong sign of any one component of the sum  cos(nu) peri + sin(nu) q  moves the position
    // by metres; and sqrt(1 - e^2) is 0.243 at e = 0.97 (the guard max(0, 1 - e^2) is far from active), the Newton iteration of the tool converging more slowly there than for the eccentricities of the study.
    const Vec3 peri = {0.6, 0.0, 0.8};
    const Vec3 q = {0.4, std::sqrt(0.75), -0.3};
    REQUIRE(std::fabs(dot_(peri, q)) < 1e-16);
    REQUIRE(std::fabs(norm_(q) - 1.0) < 1e-15);
    for (const double e : {0.0, 0.3, 0.85, 0.97}) {
        const double a = (pc::kREarthM + 600.0e3) / (1.0 - e);
        const pc::Orbit orb(a, e, peri, q);
        const double T = orb.period();
        INFO("e = " << e);
        for (const double fraction : {-0.45, -0.2, -0.05, -0.01, -0.002, 0.002, 0.01, 0.05, 0.2, 0.4, 0.499, 0.7}) {
            const Vec3 p = orb.pos(fraction * T);
            const Vec3 b = kepler_by_bisection(a, e, peri, q, fraction * T);
            INFO("fraction " << fraction);
            for (std::size_t k = 0; k < 3; ++k) CHECK(std::fabs(p.at(k) - b.at(k)) < 1e-5);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the pieces of one traversal

TEST_CASE("orbit_for and angle_from_antisolar: the geometry of beta and omega", "[penumbral_cancellation][behaviour]") {
    // beta = 0, omega = 0: the plane contains the Sun direction, periapsis is the antisolar point, q = N x peri = (0, 0, 1) x (-1, 0, 0) = (0, -1, 0)
    const pc::Orbit o0 = pc::orbit_for(kRLeo, 0.0, 0.0, 0.0);
    CHECK(o0.peri_hat == Vec3{-1.0, 0.0, 0.0});
    CHECK(o0.q_hat == Vec3{0.0, -1.0, 0.0});
    CHECK(o0.a == kRLeo);
    CHECK(o0.e == 0.0);
    CHECK(pc::angle_from_antisolar(o0, 0.0) == 0.0);   // c = -(-a) / a = 1: acos(1) = 0
    CHECK(std::fabs(pc::angle_from_antisolar(o0, 0.25 * o0.period()) - std::numbers::pi / 2.0) < 1e-9);
    CHECK(std::fabs(pc::angle_from_antisolar(o0, 0.5 * o0.period()) - std::numbers::pi) < 1e-7);   // acos near -1
    // the angle from the antisolar point grows as n t on the circular orbit in the plane of the Sun
    for (const double t : {1.0, 10.0, 100.0, 1000.0}) CHECK(std::fabs(pc::angle_from_antisolar(o0, t) - o0.n_mean * t) < 1e-12);
    // beta > 0: the plane is tilted by beta away from the Sun direction, so the smallest angle to the antisolar direction is beta and the position at periapsis is in the plane
    for (const double beta : {0.2, 0.7, 1.0}) {
        const pc::Orbit ob = pc::orbit_for(kRLeo, 0.0, beta, 0.0);
        const Vec3 normal = {std::sin(beta), 0.0, std::cos(beta)};
        CHECK(std::fabs(norm_(ob.peri_hat) - 1.0) < 1e-15);
        CHECK(std::fabs(norm_(ob.q_hat) - 1.0) < 1e-15);
        CHECK(std::fabs(dot_(ob.peri_hat, normal)) < 1e-15);
        CHECK(std::fabs(dot_(ob.q_hat, normal)) < 1e-15);
        CHECK(std::fabs(dot_(ob.peri_hat, ob.q_hat)) < 1e-15);
        CHECK(std::fabs(pc::angle_from_antisolar(ob, 0.0) - beta) < 1e-14);
        for (const double fraction : {0.05, 0.3, 0.5, 0.8}) CHECK(pc::angle_from_antisolar(ob, fraction * ob.period()) > beta);
    }
    // omega turns periapsis within the plane away from the eclipse-closest direction: at t = 0 the satellite sits omega away from the antisolar point (beta = 0)
    for (const double om : {45.0, 90.0, 135.0}) {
        const pc::Orbit oo = pc::orbit_for(kRLeo, 0.0, 0.0, om);
        CHECK(std::fabs(pc::angle_from_antisolar(oo, 0.0) - om * std::numbers::pi / 180.0) < 1e-12);
    }
}

TEST_CASE("locate_closest_approach: the minimum of the angle to the antisolar direction, by brute force", "[penumbral_cancellation][behaviour]") {
    // circular, in the plane of the Sun: the satellite is at the antisolar point at t = 0 (the ternary search stops within the flat floor of acos, a few 1e-5 s)
    CHECK(std::fabs(pc::locate_closest_approach(pc::orbit_for(kRLeo, 0.0, 0.0, 0.0))) < 1e-3);
    // tilted: the minimum angle is beta, reached at periapsis
    for (const double beta : {0.3, 0.9}) {
        const pc::Orbit ob = pc::orbit_for(kRLeo, 0.0, beta, 0.0);
        const double t0 = pc::locate_closest_approach(ob);
        CHECK(std::fabs(t0) < 1e-2);
        CHECK(std::fabs(pc::angle_from_antisolar(ob, t0) - beta) < 1e-12);
    }
    // eccentric, off the apsis: against a scan of 40,000 samples over one period (the scan's own minimum can only be larger or equal); the search finds a time within one scan step of the scan's
    for (const auto& [e, om] : {std::pair{0.5, 45.0}, std::pair{0.85, 90.0}}) {
        const double a = (pc::kREarthM + 600.0e3) / (1.0 - e);
        const pc::Orbit ob = pc::orbit_for(a, e, 0.0, om);
        const double T = ob.period();
        const double t0 = pc::locate_closest_approach(ob);
        double best = 1e9;
        double t_best = 0.0;
        constexpr int N = 40000;
        for (int i = 0; i < N; ++i) {
            const double t = -0.5 * T + T * i / (N - 1.0);
            const double an = pc::angle_from_antisolar(ob, t);
            if (an < best) {
                best = an;
                t_best = t;
            }
        }
        INFO("e = " << e << ", omega = " << om << ", t0 = " << t0 << ", scan minimum at " << t_best);
        CHECK(pc::angle_from_antisolar(ob, t0) <= best + 1e-12);
        CHECK(std::fabs(t0 - t_best) < 2.0 * T / N);
    }
}

TEST_CASE("find_window: the length of the penumbral passage from the geometry, and the two notes", "[penumbral_cancellation][behaviour]") {
    // circular LEO, beta = 0: the satellite moves through the angle theta from the antisolar point at the rate n; the umbra ends at theta = a_e - a_s and the penumbra at theta = a_e + a_s (the Earth's apparent
    // radius a_e = asin(Re / r) = 1.0557 rad, the Sun's 4.65e-3), so the passage lasts 2 a_s / n = 9.247 s and the window runs from 15 % of that before the umbra's edge to 15 % after the outer edge.  The tool
    // locates the edges where Fs crosses 1e-6 and 1 - 1e-6 (nphi_coarse 150), not at the geometric contacts: the tail of the lens area is of order 1.5 in the offset, so the difference is 4e-3 of the passage.
    const pc::Orbit orb = pc::orbit_for(kRLeo, 0.0, 0.0, 0.0);
    const double t0 = pc::locate_closest_approach(orb);
    const pc::StudyOptions options;
    const pc::Window w = pc::find_window(orb, t0, options);
    REQUIRE(w.ok);
    CHECK(w.note.empty());
    const double n = orb.n_mean;
    const double t_umbra = (kAEarthLeo - kAs0) / n;
    const double t_outer = (kAEarthLeo + kAs0) / n;
    const double span = t_outer - t_umbra;
    CHECK(std::fabs(span - 2.0 * kAs0 / n) < 1e-9);
    CHECK(std::fabs(w.dt_lo - (t_umbra - 0.15 * span)) < 0.1);
    CHECK(std::fabs(w.dt_hi - (t_outer + 0.15 * span)) < 0.1);
    CHECK(std::fabs((w.dt_hi - w.dt_lo) - 1.3 * span) < 0.01);   // "duration 12.0 s" (SHDW-R-031)
    CHECK(w.dt_lo < w.dt_hi);

    // no eclipse at the closest approach: beta well beyond the cutoff -- the note carries Fs to six decimals
    const pc::Orbit high = pc::orbit_for(kRLeo, 0.0, 1.2, 0.0);
    const pc::Window none = pc::find_window(high, pc::locate_closest_approach(high), options);
    CHECK_FALSE(none.ok);
    CHECK(none.note == "no eclipse at closest approach (Fs=1.000000)");
    // a window of no samples never reaches full sunlight (the scan has nothing in it)
    pc::StudyOptions empty;
    empty.coarse_n = 0;
    const pc::Window narrow = pc::find_window(orb, t0, empty);
    CHECK_FALSE(narrow.ok);
    CHECK(narrow.note == "window too narrow: never reaches full sunlight");
    // the window of a tilted orbit starts later and is longer (the penumbra is crossed obliquely): beta = 0.9 a_e
    const pc::Orbit tilt = pc::orbit_for(kRLeo, 0.0, 0.9 * kAEarthLeo, 0.0);
    const pc::Window wt = pc::find_window(tilt, pc::locate_closest_approach(tilt), options);
    REQUIRE(wt.ok);
    CHECK((wt.dt_hi - wt.dt_lo) > 2.0 * (w.dt_hi - w.dt_lo));   // measured 33.8 s against 12.0 s
}

TEST_CASE("find_window: the window's edges are where Fs crosses 1e-6 and 1 - 1e-6 -- also when the closest approach is just inside the umbra's edge", "[penumbral_cancellation][behaviour]") {
    const pc::StudyOptions options;
    const auto fu = [&](const pc::Orbit& orb, double t) { return pc::fs(kSun, orb.pos(t), pc::kREarthM, 0.0, options.nphi_coarse); };
    // the edges of a window, backed out of  dt_lo = u - 0.15 (e - u)  and  dt_hi = e + 0.15 (e - u):   u = (1.15 dt_lo + 0.15 dt_hi) / 1.3,  e = (1.15 dt_hi + 0.15 dt_lo) / 1.3
    const auto edges = [](const pc::Window& w) { return std::pair{(1.15 * w.dt_lo + 0.15 * w.dt_hi) / 1.3, (1.15 * w.dt_hi + 0.15 * w.dt_lo) / 1.3}; };
    // the baseline and two tilted circular orbits, the closest approach well inside the umbra: the lower edge u is where Fs has risen through 1e-6, the upper edge e where it reaches 1 - 1e-6
    // (the bisections are on the sampled Fs itself: to a time resolution of 1e-13 s, that is 1e-15 or less in Fs)
    for (const double beta_over_ae : {0.0, 0.5, 0.9}) {
        const pc::Orbit orb = pc::orbit_for(kRLeo, 0.0, beta_over_ae * kAEarthLeo, 0.0);
        const double t0 = pc::locate_closest_approach(orb);
        const pc::Window w = pc::find_window(orb, t0, options);
        INFO("beta / a_e = " << beta_over_ae);
        REQUIRE(w.ok);
        const auto [u, e] = edges(w);
        CHECK(fu(orb, t0) < 1e-6);   // in the umbra at the closest approach
        CHECK(std::fabs(fu(orb, t0 + u) - 1e-6) < 1e-12);
        CHECK(std::fabs(fu(orb, t0 + e) - (1.0 - 1e-6)) < 1e-12);
    }
    // The closest approach just INSIDE the umbra's edge: beta is found, by bisection on the tool's own Fs, for which Fs at the closest approach is 9.99e-7 -- under the threshold 1e-6 by a tenth of a percent -- while
    // it is over the threshold 0.1 s later (and a second later): the satellite moves away from the shadow's axis, the angle from it grows as (n t)^2 cot(beta) / 2, which is 2.9e-9 rad in 0.1 s, and Fs by 2.9
    // per radian there.  The window then STARTS (u) where Fs rises through 1e-6, a fraction of a second after the closest approach, and not at the closest approach (u = 0), which is what a window made from
    // Fs at another instant than the closest approach would give.
    const auto f0_of = [&](double beta) {
        const pc::Orbit o = pc::orbit_for(kRLeo, 0.0, beta, 0.0);
        return fu(o, pc::locate_closest_approach(o));
    };
    double lo = kAEarthLeo - kAs0 - 1e-4;   // deep in the umbra: Fs = 0 at the closest approach
    double hi = kAEarthLeo - kAs0 + 1e-3;   // well into the penumbra: Fs of the order of 1e-2
    REQUIRE(f0_of(lo) < 9.99e-7);
    REQUIRE(f0_of(hi) > 9.99e-7);
    for (int i = 0; i < 100; ++i) {
        const double mid = 0.5 * (lo + hi);
        if (f0_of(mid) < 9.99e-7) lo = mid; else hi = mid;
    }
    const pc::Orbit orb = pc::orbit_for(kRLeo, 0.0, lo, 0.0);
    const double t0 = pc::locate_closest_approach(orb);
    REQUIRE(fu(orb, t0) < 1e-6);
    REQUIRE(fu(orb, t0) > 9.9e-7);
    REQUIRE(fu(orb, t0 + 0.1) > 1e-6);
    REQUIRE(fu(orb, t0 + 1.0) > 1e-6);
    const pc::Window w = pc::find_window(orb, t0, options);
    REQUIRE(w.ok);
    const auto [u, e] = edges(w);
    CHECK(u > 0.01);   // not at the closest approach
    CHECK(u < 2.0);    // a fraction of a second to a second later
    CHECK(std::fabs(fu(orb, t0 + u) - 1e-6) < 1e-12);
    CHECK(std::fabs(fu(orb, t0 + e) - (1.0 - 1e-6)) < 1e-12);
}

TEST_CASE("sample_times: dt_lo + (dt_hi - dt_lo) i / (n - 1)", "[penumbral_cancellation][behaviour]") {
    const std::vector<double> t = pc::sample_times(-2.0, 6.0, 5);
    REQUIRE(t.size() == 5);
    CHECK(t.at(0) == -2.0);
    CHECK(t.at(1) == 0.0);
    CHECK(t.at(2) == 2.0);
    CHECK(t.at(3) == 4.0);
    CHECK(t.at(4) == 6.0);
    const std::vector<double> u = pc::sample_times(10.0, 11.0, 3);
    REQUIRE(u.size() == 3);
    CHECK(u.at(0) == 10.0);
    CHECK(u.at(1) == 10.5);
    CHECK(u.at(2) == 11.0);
    const std::vector<double> two = pc::sample_times(3.0, 4.0, 2);
    REQUIRE(two.size() == 2);
    CHECK(two.at(0) == 3.0);
    CHECK(two.at(1) == 4.0);
    // the product first, then the division, as the Python's `dt_lo + (dt_hi - dt_lo) * np.arange(n) / (n - 1)`: for a range of 1 s and 1201 samples the sample 7 is 7 / 1200 rounded once from 7 * 1.0
    const std::vector<double> v = pc::sample_times(0.0, 1.0, 1201);
    REQUIRE(v.size() == 1201);
    CHECK(v.at(7) == 7.0 / 1200.0);
    CHECK(v.at(1200) == 1.0);
    CHECK(pc::sample_times(0.0, 1.0, 0).empty());
}

TEST_CASE("summarize: the integrals of a sampled difference, by hand", "[penumbral_cancellation][behaviour]") {
    // du = 0, 2, 1, 0 at t = 0, 1, 2, 3: the trapezoids are 1, 1.5, 0.5, so net = abs = 3, the running integral 1, 2.5, 3 peaks at 3, cancel = 100 (1 - 3 / 3) = 0, swing = 3 / 3 = 1
    {
        const pc::Result r = pc::summarize(std::vector<double>{0.0, 2.0, 1.0, 0.0}, std::vector<double>{0.0, 1.0, 2.0, 3.0}, 3.0);
        CHECK(r.ok);
        CHECK(r.net == 3.0);
        CHECK(r.absint == 3.0);
        CHECK(r.worst_running == 3.0);
        CHECK(r.duration == 3.0);
        CHECK(r.cancel_pct == 0.0);
        CHECK(r.swing == 1.0);
        CHECK(r.note.empty());
    }
    // du = 0, 2, 0, -1, 0: the trapezoids are 1, 1, -0.5, -0.5: net 1, abs 3, running 1, 2, 1.5, 1 so the peak is 2; cancel = 100 (1 - 1/3), swing = 2
    {
        const pc::Result r = pc::summarize(std::vector<double>{0.0, 2.0, 0.0, -1.0, 0.0}, std::vector<double>{0.0, 1.0, 2.0, 3.0, 4.0}, 4.0);
        CHECK(r.net == 1.0);
        CHECK(r.absint == 3.0);
        CHECK(r.worst_running == 2.0);
        CHECK(r.cancel_pct == 100.0 * (1.0 - 1.0 / 3.0));
        CHECK(r.swing == 2.0);
    }
    // a perfect cancellation: du = 0, 2, 0, -2, 0 -> net 0 (<= 1e-300): the swing is infinite; the cancellation is exactly 100
    {
        const pc::Result r = pc::summarize(std::vector<double>{0.0, 2.0, 0.0, -2.0, 0.0}, std::vector<double>{0.0, 1.0, 2.0, 3.0, 4.0}, 4.0);
        CHECK(r.net == 0.0);
        CHECK(r.absint == 4.0);
        CHECK(r.worst_running == 2.0);
        CHECK(r.cancel_pct == 100.0);
        CHECK(std::isinf(r.swing));
        CHECK(r.swing > 0.0);
    }
    // the peak is of |running|: a negative excursion counts (du = 0, -2, 0 gives net -2, running -1, -2, abs 2)
    {
        const pc::Result r = pc::summarize(std::vector<double>{0.0, -2.0, 0.0}, std::vector<double>{0.0, 1.0, 2.0}, 2.0);
        CHECK(r.net == -2.0);
        CHECK(r.worst_running == 2.0);
        CHECK(r.swing == 1.0);
        CHECK(r.cancel_pct == 0.0);
    }
    // nothing at all: abs = 0, so the cancellation is nan; net = 0 so the swing is infinite
    {
        const pc::Result r = pc::summarize(std::vector<double>{0.0, 0.0, 0.0}, std::vector<double>{0.0, 1.0, 2.0}, 2.0);
        CHECK(r.net == 0.0);
        CHECK(r.absint == 0.0);
        CHECK(r.worst_running == 0.0);
        CHECK(std::isnan(r.cancel_pct));
        CHECK(std::isinf(r.swing));
    }
    // one sample: no interval -- every integral is zero
    {
        const pc::Result r = pc::summarize(std::vector<double>{5.0}, std::vector<double>{1.0}, 0.0);
        CHECK(r.ok);
        CHECK(r.net == 0.0);
        CHECK(r.absint == 0.0);
        CHECK(r.worst_running == 0.0);
    }
    CHECK_THROWS_AS(pc::summarize(std::vector<double>{1.0, 2.0}, std::vector<double>{0.0}, 1.0), std::invalid_argument);
    CHECK_THROWS_AS(pc::summarize(std::vector<double>{1.0}, std::vector<double>{0.0, 1.0}, 1.0), std::invalid_argument);
    try {
        (void)pc::summarize(std::vector<double>{1.0, 2.0}, std::vector<double>{0.0}, 1.0);
        FAIL("no exception");
    } catch (const std::invalid_argument& e) {
        CHECK(std::string(e.what()) == "summarize: du and tt differ in length");
    }
    // exactly two samples are one interval: the running integral is that one increment (du = 1, 3 over 2 s: 2 * (3 + 1) / 2 = 4; the negative of it is the same peak)
    {
        const pc::Result r = pc::summarize(std::vector<double>{1.0, 3.0}, std::vector<double>{0.0, 2.0}, 2.0);
        CHECK(r.net == 4.0);
        CHECK(r.absint == 4.0);
        CHECK(r.worst_running == 4.0);
        CHECK(r.cancel_pct == 0.0);
        CHECK(r.swing == 1.0);
        const pc::Result n = pc::summarize(std::vector<double>{-1.0, -3.0}, std::vector<double>{0.0, 2.0}, 2.0);
        CHECK(n.net == -4.0);
        CHECK(n.worst_running == 4.0);
        CHECK(n.swing == 1.0);
    }
}

TEST_CASE("study: the composition of its pieces, and its two notes", "[penumbral_cancellation][behaviour]") {
    pc::StudyOptions options;   // a small study: it must be the pieces in order, bit for bit
    options.nphi_coarse = 60;
    options.nphi_fine = 60;
    options.n_samples = 61;
    options.coarse_n = 40;
    const pc::Result r = pc::study(kRLeo, 0.0, 0.0, 0.0, options);
    REQUIRE(r.ok);
    const pc::Orbit orb = pc::orbit_for(kRLeo, 0.0, 0.0, 0.0);
    const double t0 = pc::locate_closest_approach(orb);
    const pc::Window w = pc::find_window(orb, t0, options);
    REQUIRE(w.ok);
    const std::vector<double> tt = pc::sample_times(w.dt_lo, w.dt_hi, options.n_samples);
    std::vector<double> du(tt.size());
    for (std::size_t i = 0; i < tt.size(); ++i) {
        const Vec3 pos = orb.pos(t0 + tt.at(i));
        du.at(i) = pc::fs(kSun, pos, pc::kREarthM, pc::kEddingtonU, options.nphi_fine) - pc::fs(kSun, pos, pc::kREarthM, 0.0, options.nphi_fine);
    }
    const pc::Result by_hand = pc::summarize(du, tt, w.dt_hi - w.dt_lo);
    CHECK(bits_of(r.net) == bits_of(by_hand.net));
    CHECK(bits_of(r.absint) == bits_of(by_hand.absint));
    CHECK(bits_of(r.worst_running) == bits_of(by_hand.worst_running));
    CHECK(bits_of(r.duration) == bits_of(by_hand.duration));
    CHECK(bits_of(r.cancel_pct) == bits_of(by_hand.cancel_pct));
    CHECK(bits_of(r.swing) == bits_of(by_hand.swing));
    // a physical sanity: the duration is the window's, and the cancellation is high for the baseline passage
    CHECK(std::fabs(r.duration - 12.02) < 0.05);
    CHECK(r.cancel_pct > 99.0);
    // the notes of find_window come through study unchanged
    const pc::Result none = pc::study(kRLeo, 0.0, 1.2, 0.0, options);
    CHECK_FALSE(none.ok);
    CHECK(none.note == "no eclipse at closest approach (Fs=1.000000)");
    pc::StudyOptions empty = options;
    empty.coarse_n = 0;
    const pc::Result narrow = pc::study(kRLeo, 0.0, 0.0, 0.0, empty);
    CHECK_FALSE(narrow.ok);
    CHECK(narrow.note == "window too narrow: never reaches full sunlight");
    // the defaults of the options
    const pc::StudyOptions defaults;
    CHECK(defaults.nphi_coarse == 150);
    CHECK(defaults.nphi_fine == 450);
    CHECK(defaults.n_samples == 401);
    CHECK(defaults.coarse_n == 300);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the printed text

TEST_CASE("report: the lines of a row and of a note", "[penumbral_cancellation][behaviour]") {
    pc::Result r;
    r.ok = true;
    r.duration = 12.018;
    r.net = -1.5389e-4;
    r.absint = 0.11423;
    r.worst_running = 0.057193;
    r.swing = 371.64;
    r.cancel_pct = 99.8653;
    std::ostringstream out;
    pc::report(out, "baseline", r);
    CHECK(out.str() == "[baseline]\n    duration 12.02 s   net -1.5389e-04 s   abs 1.1423e-01 s   peak running 5.7193e-02 s   swing 371.6x   cancels 99.87%\n");
    // the sign of net is always printed ("+.4e"), also for zero and for a positive value
    r.net = 7.1533e-1;
    std::ostringstream plus;
    pc::report(plus, "x", r);
    CHECK(contains(plus.str(), "net +7.1533e-01 s"));
    r.net = 0.0;
    std::ostringstream zero;
    pc::report(zero, "x", r);
    CHECK(contains(zero.str(), "net +0.0000e+00 s"));
    r.net = -0.0;
    std::ostringstream negzero;
    pc::report(negzero, "x", r);
    CHECK(contains(negzero.str(), "net -0.0000e+00 s"));
    // abs and the peak are not signed; an infinite swing prints as Python prints it, and nan likewise
    r.swing = std::numeric_limits<double>::infinity();
    r.cancel_pct = std::numeric_limits<double>::quiet_NaN();
    std::ostringstream special;
    pc::report(special, "x", r);
    CHECK(contains(special.str(), "swing infx   cancels nan%"));
    // a row that did not run: the label and the note on one line, two blanks between
    pc::Result bad;
    bad.ok = false;
    bad.note = "window too narrow: never reaches full sunlight";
    std::ostringstream note;
    pc::report(note, "e = 0.50, 45 deg from periapsis", bad);
    CHECK(note.str() == "[e = 0.50, 45 deg from periapsis]  window too narrow: never reaches full sunlight\n");
}


// a study that records its arguments and returns scripted results in call order
struct Scripted {
    std::vector<pc::Result> results;
    mutable std::vector<std::tuple<double, double, double, double, pc::StudyOptions>> calls;
    pc::Settings settings() {
        pc::Settings s;
        s.study = [this](double a, double e, double beta, double omega, const pc::StudyOptions& o) {
            calls.emplace_back(a, e, beta, omega, o);
            return results.at(calls.size() - 1);
        };
        return s;
    }
};

pc::Result row(double duration, double net, double absint, double peak, double swing, double cancel) {
    pc::Result r;
    r.ok = true;
    r.duration = duration;
    r.net = net;
    r.absint = absint;
    r.worst_running = peak;
    r.swing = swing;
    r.cancel_pct = cancel;
    return r;
}


TEST_CASE("run_on: the text of the whole study, from scripted rows, derived from the statements of main()", "[penumbral_cancellation][behaviour]") {
    // the 31 calls in order: 0 baseline; 1-12 tilt; 13-17 at an apse; 18-21 off an apse; 22-25 the refinement of the two extreme rows; 26-30 the baseline sweep (swing 367.6 370.8 371.7 375.7 375.6,
    // net -1.556 -1.543 -1.539 -1.522 -1.523 (e-4), abs 1.1427 1.1424 1.1423 1.1423 1.1423 (e-1): the table of PROVENANCE 25.10)
    Scripted script;
    for (int i = 0; i < 26; ++i) script.results.push_back(row(10.0 + i, -1.0e-4 * (i + 1), 0.1 + 0.01 * i, 0.05 + 0.001 * i, 100.0 + i, 99.0 - 0.01 * i));
    script.results.at(5) = [] { pc::Result r; r.ok = false; r.note = "no eclipse at closest approach (Fs=1.000000)"; return r; }();   // the 0.9 tilt row did not run
    script.results.at(0) = row(12.0181, -1.5572e-4, 0.114232, 0.057193, 367.3, 99.8653);
    const double swings[] = {367.6, 370.8, 371.7, 375.7, 375.6};
    const double nets[] = {-1.556e-4, -1.543e-4, -1.539e-4, -1.522e-4, -1.523e-4};
    const double abss[] = {0.11427, 0.11424, 0.11423, 0.11423, 0.11423};
    for (int i = 0; i < 5; ++i) script.results.push_back(row(12.02, nets[i], abss[i], 0.0572, swings[i], 99.87));
    REQUIRE(script.results.size() == 31);
    const Run run = run_with(script.settings(), {});
    CHECK(run.code == 0);
    CHECK(run.err.empty());
    REQUIRE(script.calls.size() == 31);

    // the arguments of each call
    const double r_peri_safe = pc::kREarthM + 600.0e3;
    const std::array<double, 12> fracs = {0.0, 0.3, 0.6, 0.8, 0.9, 0.95, 0.97, 0.98, 0.99, 0.995, 0.998, 0.9995};
    const auto call = [&](std::size_t i) { return script.calls.at(i); };
    CHECK(std::get<0>(call(0)) == kRLeo);
    CHECK(std::get<1>(call(0)) == 0.0);
    CHECK(std::get<2>(call(0)) == 0.0);
    CHECK(std::get<3>(call(0)) == 0.0);
    CHECK(std::get<4>(call(0)).nphi_coarse == 150);
    CHECK(std::get<4>(call(0)).nphi_fine == 450);
    CHECK(std::get<4>(call(0)).n_samples == 401);
    CHECK(std::get<4>(call(0)).coarse_n == 300);
    for (std::size_t i = 0; i < 12; ++i) {
        const auto c = call(1 + i);
        CHECK(std::get<0>(c) == kRLeo);
        CHECK(std::get<1>(c) == 0.0);
        CHECK(std::get<2>(c) == fracs.at(i) * kAEarthLeo);
        CHECK(std::get<3>(c) == 0.0);
        CHECK(std::get<4>(c).nphi_coarse == 400);
        CHECK(std::get<4>(c).nphi_fine == 450);
        CHECK(std::get<4>(c).n_samples == 1201);
        CHECK(std::get<4>(c).coarse_n == 300);
    }
    const std::array<double, 5> apse_e = {0.0, 0.3, 0.5, 0.7, 0.85};
    for (std::size_t i = 0; i < 5; ++i) {
        const auto c = call(13 + i);
        CHECK(std::get<0>(c) == kRLeo / (1.0 - apse_e.at(i)));
        CHECK(std::get<1>(c) == apse_e.at(i));
        CHECK(std::get<2>(c) == 0.0);
        CHECK(std::get<3>(c) == 0.0);
        CHECK(std::get<4>(c).nphi_coarse == 150);
        CHECK(std::get<4>(c).n_samples == 401);
    }
    const std::array<std::pair<double, double>, 4> off = {std::pair{0.5, 45.0}, std::pair{0.5, 90.0}, std::pair{0.85, 45.0}, std::pair{0.85, 90.0}};
    for (std::size_t i = 0; i < 4; ++i) {
        const auto c = call(18 + i);
        CHECK(std::get<0>(c) == r_peri_safe / (1.0 - off.at(i).first));
        CHECK(std::get<1>(c) == off.at(i).first);
        CHECK(std::get<2>(c) == 0.0);
        CHECK(std::get<3>(c) == off.at(i).second);
    }
    const std::array<std::tuple<double, int, int>, 4> refine = {std::tuple{0.995, 401, 200}, std::tuple{0.995, 801, 400}, std::tuple{0.9995, 401, 200}, std::tuple{0.9995, 801, 400}};
    for (std::size_t i = 0; i < 4; ++i) {
        const auto c = call(22 + i);
        CHECK(std::get<0>(c) == kRLeo);
        CHECK(std::get<1>(c) == 0.0);   // circular
        CHECK(std::get<2>(c) == std::get<0>(refine.at(i)) * kAEarthLeo);
        CHECK(std::get<3>(c) == 0.0);
        CHECK(std::get<4>(c).nphi_coarse == 200);
        CHECK(std::get<4>(c).n_samples == std::get<1>(refine.at(i)));
        CHECK(std::get<4>(c).nphi_fine == std::get<2>(refine.at(i)));
        CHECK(std::get<4>(c).coarse_n == 300);
    }
    const std::array<std::pair<int, int>, 5> sweep = {std::pair{201, 100}, std::pair{401, 200}, std::pair{801, 400}, std::pair{1601, 800}, std::pair{3201, 1600}};
    for (std::size_t i = 0; i < 5; ++i) {
        const auto c = call(26 + i);
        CHECK(std::get<0>(c) == kRLeo);
        CHECK(std::get<1>(c) == 0.0);
        CHECK(std::get<2>(c) == 0.0);
        CHECK(std::get<3>(c) == 0.0);
        CHECK(std::get<4>(c).nphi_coarse == 150);
        CHECK(std::get<4>(c).n_samples == sweep.at(i).first);
        CHECK(std::get<4>(c).nphi_fine == sweep.at(i).second);
    }

    // the text: every label and heading is the Python's print statement; the rows are report()'s
    const std::vector<std::string> lines = lines_of(run.out);
    REQUIRE(lines.size() >= 80);
    CHECK(lines.at(0) == "=== SHDW-R-031: the ONE stated passage ===");
    CHECK(lines.at(1) == "LEO, r=7331km, circular, beta=0 (shadow axis in the orbital plane)");
    CHECK(lines.at(2) == "");
    CHECK(lines.at(3) == "[baseline]");
    CHECK(lines.at(4) == "    duration 12.02 s   net -1.5572e-04 s   abs 1.1423e-01 s   peak running 5.7193e-02 s   swing 367.3x   cancels 99.87%");
    CHECK(lines.at(5) == "");
    CHECK(lines.at(6) == "=== SHDW-R-033, family 1: orbital-plane tilt toward the eclipse cutoff ===");
    CHECK(lines.at(7) == "[beta/a_e = 0.0000]");
    CHECK(lines.at(8) == "    duration 11.00 s   net -2.0000e-04 s   abs 1.1000e-01 s   peak running 5.1000e-02 s   swing 101.0x   cancels 98.99%");
    // the row of the 0.9 tilt (the fifth, call 5) did not run: label and note on one line
    CHECK(contains(run.out, "[beta/a_e = 0.9000]  no eclipse at closest approach (Fs=1.000000)\n"));
    for (const char* label : {"[beta/a_e = 0.3000]", "[beta/a_e = 0.6000]", "[beta/a_e = 0.8000]", "[beta/a_e = 0.9500]", "[beta/a_e = 0.9700]", "[beta/a_e = 0.9800]", "[beta/a_e = 0.9900]",
                              "[beta/a_e = 0.9950]", "[beta/a_e = 0.9980]", "[beta/a_e = 0.9995]"})
        CHECK(contains(run.out, std::string(label) + "\n"));
    CHECK(contains(run.out, "\n=== SHDW-R-033, family 2: eccentricity, eclipse AT an apse (should be UNCHANGED, exactly) ===\n"));
    for (const char* label : {"[e = 0.00, at periapsis]", "[e = 0.30, at periapsis]", "[e = 0.50, at periapsis]", "[e = 0.70, at periapsis]", "[e = 0.85, at periapsis]"})
        CHECK(contains(run.out, std::string(label) + "\n"));
    CHECK(contains(run.out, "\n=== SHDW-R-033, family 3: eccentricity, eclipse OFF an apse (genuine radial velocity) ===\n"));
    for (const char* label : {"[e = 0.50, 45 deg from periapsis]", "[e = 0.50, 90 deg from periapsis]", "[e = 0.85, 45 deg from periapsis]", "[e = 0.85, 90 deg from periapsis]"})
        CHECK(contains(run.out, std::string(label) + "\n"));
    CHECK(contains(run.out,
                   "\n=== resolution check on the two most extreme rows above (SHDW's own rule 5 diagnostic) ===\n"
                   "(kept modest deliberately: the point is to show the figure does not move across a\n"
                   " refinement, which two steps already demonstrate; the original investigation went\n"
                   " up to N=4801/nphi=1800 and is recorded in PROVENANCE.md SS25.11 for the full range)\n"
                   "  beta/a_e = 0.995:\n"
                   "[    N_samples=401 nphi=200]\n"));
    CHECK(contains(run.out, "  beta/a_e = 0.9995:\n[    N_samples=401 nphi=200]\n"));
    CHECK(contains(run.out, "[    N_samples=801 nphi=400]\n"));
    CHECK(contains(run.out,
                   "\n=== resolution check on the BASELINE row too (manager's finding, 2026-09-22) ===\n"
                   "this is the ROW THE EXTREME-ROW CHECK ABOVE DID NOT COVER, and it is the most\n"
                   "resolution-sensitive of any row in this file: cancel is 99.9%, so net is a ~0.1%\n"
                   "residual of two integrals each ~abs in size, and a tiny relative shift in either\n"
                   "integral is a large relative shift in their difference. swing = abs/net inherits\n"
                   "that sensitivity directly. abs and cancel_pct do NOT share it -- watch them hold\n"
                   "still while swing moves.\n"
                   "[  N_samples=201 nphi=100]\n"));
    for (const char* label : {"[  N_samples=401 nphi=200]", "[  N_samples=801 nphi=400]", "[  N_samples=1601 nphi=800]", "[  N_samples=3201 nphi=1600]"})
        CHECK(contains(run.out, std::string(label) + "\n"));

    // the statistics after the sweep, by hand from the scripted table: swings 367.6 .. 375.7 with mean (367.6 + 370.8 + 371.7 + 375.7 + 375.6) / 5 = 372.28 -> 100 * 8.1 / 372.28 = 2.1757 -> "2.2%";
    // nets span -1.556e-4 .. -1.522e-4: 100 * 0.034e-4 / |mean = -1.5366e-4| = 2.2127 -> "2.2%"; abs spans 0.11427 .. 0.11423: 100 * 0.00004 / 0.114240 = 0.03501 -> "0.04%"; the finest two agree to
    // 100 * |375.6 - 375.7| / 375.6 = 0.026624 -> "0.03%"; the converged estimate is the last value, 375.6 -> "~376x"
    CHECK(contains(run.out,
                   "  across these 5 resolutions: swing spans 367.6x to 375.7x (2.2% spread); net spans 2.2% spread; abs spans only 0.04% spread -- confirming net (and hence swing) is the sensitive one, not abs.\n"));
    CHECK(contains(run.out,
                   "  the FINEST two resolutions agree to 0.03%, well inside the spread of the coarser pairs -- that is convergence, not noise. Converged estimate: ~376x. This is closer to the ORIGINAL figure "
                   "this tool was built to check (376x) than to this tool's own default-resolution output (367x) -- the default was under-resolved, not the earlier figure wrong.\n"));
    CHECK(contains(run.out, "\n=== SUMMARY (the figures SPEC-shadow.md and PROVENANCE.md quote) ===\n"));
    CHECK(lines.back() ==
          "  baseline (beta=0):        cancels 99.87%, net -1.5572e-04 s, abs 1.1423e-01 s (stable to 4 figures), swing ~376x (from the finest-resolution pair above, NOT from this call's own "
          "default-resolution run of 367x -- see the baseline resolution check for why the two differ and which one to trust)");
    CHECK(run.out.back() == '\n');
}

TEST_CASE("run_on: the statistics under the baseline sweep, from a scripted table whose spreads are large (the factor 100 of a percentage shows)", "[penumbral_cancellation][behaviour]") {
    // swings 10 10 10 10 90 (mean 26), nets -1 -1 -1 -1 -5 (e-4; mean -1.8e-4), abs 0.10 0.10 0.10 0.10 0.15 (mean 0.11): by hand,
    //   swing spread  100 * (90 - 10) / 26             = 307.69   -> "307.7"
    //   net spread    100 * (-1e-4 - -5e-4) / 1.8e-4   = 222.22   -> "222.2"
    //   abs spread    100 * (0.15 - 0.10) / 0.11       = 45.4545  -> "45.45"
    //   the finest two agree to  100 * |90 - 10| / 90  = 88.8889  -> "88.89";  the converged estimate is the last swing, 90 -> "~90x"
    // (a percentage computed with 100.1 instead of 100 would print 308.0, 222.4, 45.50 and 88.98)
    Scripted script;
    for (int i = 0; i < 26; ++i) script.results.push_back(row(10.0 + i, -1.0e-4 * (i + 1), 0.1 + 0.01 * i, 0.05 + 0.001 * i, 100.0 + i, 99.0 - 0.01 * i));
    const double swings[] = {10.0, 10.0, 10.0, 10.0, 90.0};
    const double nets[] = {-1.0e-4, -1.0e-4, -1.0e-4, -1.0e-4, -5.0e-4};
    const double abss[] = {0.10, 0.10, 0.10, 0.10, 0.15};
    for (int i = 0; i < 5; ++i) script.results.push_back(row(12.0, nets[i], abss[i], 0.05, swings[i], 99.0));
    const Run run = run_with(script.settings(), {});
    CHECK(run.code == 0);
    CHECK(contains(run.out, "  across these 5 resolutions: swing spans 10.0x to 90.0x (307.7% spread); net spans 222.2% spread; abs spans only 45.45% spread -- confirming net (and hence swing) is the sensitive one, not abs.\n"));
    CHECK(contains(run.out, "  the FINEST two resolutions agree to 88.89%, well inside the spread of the coarser pairs -- that is convergence, not noise. Converged estimate: ~90x. This is closer to the ORIGINAL figure"));
    CHECK(contains(run.out, "swing ~90x (from the finest-resolution pair above"));
}

TEST_CASE("run_on: the command line, the help, and an error the tool did not anticipate", "[penumbral_cancellation][behaviour]") {
    const std::string usage = "usage: penumbral_cancellation [-h]\n";
    // help: -h or --help, standard output, exit 0, nothing on standard error; no study is run
    Scripted none;
    for (const char* flag : {"-h", "--help"}) {
        const Run run = run_with(none.settings(), {flag});
        CHECK(run.code == 0);
        CHECK(run.err.empty());
        CHECK(run.out.rfind(usage, 0) == 0);
        CHECK(contains(run.out, "\nThe tool behind SHDW-R-031 and SHDW-R-033 (SPEC-shadow 3.4, 8 Coverage): Fs of a limb-darkened Sun against a uniform one along ONE stated eclipse passage, with an actual two-body Kepler propagator,\n"));
        CHECK(contains(run.out, "\noptions:\n  -h, --help   show this help and exit\n"));
        CHECK(contains(run.out, "\nexit codes: 0 done   2 an argument error   70 an error the tool did not anticipate\n"));
        CHECK(run.out.back() == '\n');
    }
    CHECK(none.calls.empty());
    // the exact text of the help
    const Run help = run_with(none.settings(), {"-h"});
    CHECK(help.out ==
          usage + "\n"
          "The tool behind SHDW-R-031 and SHDW-R-033 (SPEC-shadow 3.4, 8 Coverage): Fs of a limb-darkened Sun against a uniform one along ONE stated eclipse passage, with an actual two-body Kepler propagator,\n"
          "for the baseline, a tilt of the orbital plane toward the eclipse cutoff, and eccentric orbits at and off an apse, with the resolution checks and the SUMMARY that SPEC-shadow and PROVENANCE quote.\n"
          "Takes a few minutes.  Not a gate (SHDW-Q-005).\n"
          "\n"
          "options:\n"
          "  -h, --help   show this help and exit\n"
          "\n"
          "exit codes: 0 done   2 an argument error   70 an error the tool did not anticipate\n");
    // any other argument is refused: usage and the error on standard error, nothing on standard output, exit 2; only the FIRST argument decides
    for (const std::vector<std::string>& args : {std::vector<std::string>{"--bogus"}, std::vector<std::string>{"x"}, std::vector<std::string>{"--bogus", "-h"}, std::vector<std::string>{"-hh"}, std::vector<std::string>{""}}) {
        const Run run = run_with(none.settings(), args);
        CHECK(run.code == 2);
        CHECK(run.out.empty());
        CHECK(run.err == usage + "penumbral_cancellation: error: unrecognized arguments: " + args.front() + "\n");
    }
    CHECK(run_with(none.settings(), {"-h", "--bogus"}).code == 0);   // the loop stops at the first argument
    CHECK(none.calls.empty());
    // an error nobody anticipated: the study throws; the header lines already written stay on standard output, the message goes to standard error, exit 70
    pc::Settings throwing;
    throwing.study = [](double, double, double, double, const pc::StudyOptions&) -> pc::Result { throw std::runtime_error("boom"); };
    const Run run = run_with(throwing, {});
    CHECK(run.code == 70);
    CHECK(run.out == "=== SHDW-R-031: the ONE stated passage ===\nLEO, r=7331km, circular, beta=0 (shadow axis in the orbital plane)\n\n");
    CHECK(run.err == "penumbral_cancellation: internal error: boom\n");
    // the default settings run the real study
    CHECK_FALSE(static_cast<bool>(pc::default_settings().study));
    CHECK(std::string(pc::kTool) == "penumbral_cancellation");
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- the real passages


struct DefaultRun {
    int code = -1;
    std::string out;
    std::string err;
};

// the tool at its defaults, once for all the cases of this executable that need it (about 20 s in RelWithDebInfo)
const DefaultRun& default_run() {
    static const DefaultRun run = [] {
        std::ostringstream out;
        std::ostringstream err;
        const int code = pc::run({}, Streams{out, err});
        return DefaultRun{code, out.str(), err.str()};
    }();
    return run;
}

struct Printed {
    std::string label;
    double duration, net, absint, peak, swing, cancel;
};

double number_after(const std::string& line, const std::string& key) {
    const std::size_t at = line.find(key);
    REQUIRE(at != std::string::npos);
    return std::strtod(line.c_str() + at + key.size(), nullptr);
}

// every "[label]" with the figures of the line after it, in the order of the output
std::vector<Printed> printed_rows(const std::string& text) {
    const std::vector<std::string> lines = lines_of(text);
    std::vector<Printed> rows;
    for (std::size_t i = 0; i + 1 < lines.size(); ++i) {
        const std::string& line = lines.at(i);
        if (line.size() > 2 && line.front() == '[' && line.back() == ']' && lines.at(i + 1).rfind("    duration ", 0) == 0) {
            const std::string& f = lines.at(i + 1);
            rows.push_back(Printed{line.substr(1, line.size() - 2), number_after(f, "duration "), number_after(f, " net "), number_after(f, " abs "), number_after(f, "peak running "),
                                   number_after(f, "swing "), number_after(f, "cancels ")});
        }
    }
    return rows;
}


TEST_CASE("the recorded text is intact", "[penumbral_cancellation][behaviour]") {
    const std::string recorded = odl::devtools_testing::kRecordedPenumbralDefaultRun;
    CHECK(recorded.size() == odl::devtools_testing::kRecordedPenumbralBytes);
    CHECK(lines_of(recorded).size() == odl::devtools_testing::kRecordedPenumbralLines);
    CHECK(odl::devkit::sha256_hex(odl::devkit::as_bytes(recorded)) == odl::devtools_testing::kRecordedPenumbralSha256);
    CHECK(recorded.back() == '\n');
    CHECK(recorded.rfind("=== SHDW-R-031: the ONE stated passage ===\n", 0) == 0);
}

TEST_CASE("the tool at its defaults prints its first record, byte for byte", "[penumbral_cancellation][real_tree]") {
    // C10_proof_registration.txt, AMENDMENT 2, R3: a regression record from now on, NOT a proof against the Python.  Six variants of the arithmetic (fused multiply-add, correctly rounded trigonometry, three
    // ulp perturbations of sin and cos) printed the same bytes when it was made, so it does not rest on one libm.
    const DefaultRun& run = default_run();
    CHECK(run.code == 0);
    CHECK(run.err.empty());
    const std::string recorded = odl::devtools_testing::kRecordedPenumbralDefaultRun;
    CHECK(run.out.size() == recorded.size());
    if (run.out != recorded) {
        const std::vector<std::string> a = lines_of(run.out);
        const std::vector<std::string> b = lines_of(recorded);
        for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
            if (a.at(i) != b.at(i)) {
                FAIL("first difference at line " << (i + 1) << ":\n  printed:  " << a.at(i) << "\n  recorded: " << b.at(i));
            }
        }
    }
    CHECK(run.out == recorded);
}

TEST_CASE("SPEC-shadow's quotes hold at their stated precision on the tool's own output", "[penumbral_cancellation][real_tree]") {
    const DefaultRun& run = default_run();
    REQUIRE(run.code == 0);
    const std::vector<Printed> rows = printed_rows(run.out);
    REQUIRE(rows.size() == 31);   // 0 baseline; 1-12 tilt; 13-17 apse; 18-21 off apse; 22-25 refinement; 26-30 the baseline sweep
    CHECK(rows.at(0).label == "baseline");
    CHECK(rows.at(1).label == "beta/a_e = 0.0000");
    CHECK(rows.at(4).label == "beta/a_e = 0.8000");
    CHECK(rows.at(9).label == "beta/a_e = 0.9900");
    CHECK(rows.at(10).label == "beta/a_e = 0.9950");
    CHECK(rows.at(12).label == "beta/a_e = 0.9995");
    CHECK(rows.at(13).label == "e = 0.00, at periapsis");
    CHECK(rows.at(17).label == "e = 0.85, at periapsis");
    CHECK(rows.at(18).label == "e = 0.50, 45 deg from periapsis");
    CHECK(rows.at(21).label == "e = 0.85, 90 deg from periapsis");
    CHECK(rows.at(26).label == "  N_samples=201 nphi=100");
    CHECK(rows.at(30).label == "  N_samples=3201 nphi=1600");

    // SHDW-R-031: "duration 12.0 s", "an absolute integral of 1.142 x 10^-1 s, stable to four figures across a resolution sweep", "the running integral swings to ~ 5.72 x 10^-2 s mid-passage, ~ 376x the net",
    // "the swing ratio itself spans 368x-376x and is still moving by 0.03 % between the two finest resolutions" -- each at half a unit of its last stated digit
    CHECK(rows.at(0).duration >= 11.95);
    CHECK(rows.at(0).duration <= 12.05);
    double abs_min = 1e9, abs_max = 0.0, abs_sum = 0.0, swing_min = 1e9, swing_max = 0.0;
    for (std::size_t i = 26; i <= 30; ++i) {
        const Printed& p = rows.at(i);
        abs_min = std::min(abs_min, p.absint);
        abs_max = std::max(abs_max, p.absint);
        abs_sum += p.absint;
        swing_min = std::min(swing_min, p.swing);
        swing_max = std::max(swing_max, p.swing);
        CHECK(p.peak >= 0.05715);   // 5.72e-2
        CHECK(p.peak <= 0.05725);
        if (i >= 27) {
            CHECK(p.absint >= 0.11415);   // 1.142e-1 at the four finest resolutions
            CHECK(p.absint <= 0.11425);
        }
    }
    CHECK((abs_max - abs_min) / (abs_sum / 5.0) <= 5e-4);   // and the five within 0.05 % ("0.04 % spread")
    CHECK(swing_min >= 367.5);                             // 368x
    CHECK(swing_max <= 376.5);                             // 376x
    CHECK(rows.at(29).swing >= 375.5);                     // the two finest: "about 376x"
    CHECK(rows.at(29).swing <= 376.5);
    CHECK(rows.at(30).swing >= 375.5);
    CHECK(rows.at(30).swing <= 376.5);
    const double finest = 100.0 * std::fabs(rows.at(30).swing - rows.at(29).swing) / rows.at(30).swing;
    CHECK(finest >= 0.025);   // 0.03 %
    CHECK(finest <= 0.035);
    // the summary line quotes the converged estimate as ~376x
    CHECK(contains(run.out, "swing ~376x (from the finest-resolution pair above"));

    // SHDW-R-033, tilt: 99.87 % at beta = 0, 99.49 % at 0.8 a_e, 88.7 % at 0.99, 74.7 % at 0.995, 48.3 % at 0.9995
    CHECK(std::fabs(rows.at(1).cancel - 99.87) <= 0.005 + 1e-9);
    CHECK(std::fabs(rows.at(4).cancel - 99.49) <= 0.005 + 1e-9);
    CHECK(std::fabs(rows.at(9).cancel - 88.7) <= 0.05 + 1e-9);
    CHECK(std::fabs(rows.at(10).cancel - 74.7) <= 0.05 + 1e-9);
    CHECK(std::fabs(rows.at(12).cancel - 48.3) <= 0.05 + 1e-9);
    // SHDW-R-033, eccentricity at an apse: 99.84-99.90 %; off an apse: 99.37-99.84 %
    for (std::size_t i = 13; i <= 17; ++i) {
        CHECK(rows.at(i).cancel >= 99.84);
        CHECK(rows.at(i).cancel <= 99.90);
    }
    for (std::size_t i = 18; i <= 21; ++i) {
        CHECK(rows.at(i).cancel >= 99.37);
        CHECK(rows.at(i).cancel <= 99.84);
    }
}

TEST_CASE("the peak effect of the Sun's brightness profile along the stated passage is 1.97e-2 (SPEC-shadow's table)", "[penumbral_cancellation][real_tree]") {
    const pc::Orbit orb = pc::orbit_for(kRLeo, 0.0, 0.0, 0.0);
    const double t0 = pc::locate_closest_approach(orb);
    const pc::Window w = pc::find_window(orb, t0, pc::StudyOptions{});
    REQUIRE(w.ok);
    const std::vector<double> tt = pc::sample_times(w.dt_lo, w.dt_hi, 801);
    double peak = 0.0;
    double one_minus_fs_at_peak = 0.0;
    for (const double t : tt) {
        const Vec3 pos = orb.pos(t0 + t);
        const double f0 = pc::fs(kSun, pos, pc::kREarthM, 0.0, 450);
        const double d = std::fabs(pc::fs(kSun, pos, pc::kREarthM, pc::kEddingtonU, 450) - f0);
        if (d > peak) {
            peak = d;
            one_minus_fs_at_peak = 1.0 - f0;
        }
    }
    // "1.97 x 10^-2": half a unit of the last stated digit; PROVENANCE 25.10: "Peak at 1 - Fs = 0.818" (three figures: 0.8185 .. 0.8195 would be the stated precision of 0.819; the sampled peak sits at 0.8187)
    CHECK(peak >= 1.965e-2);
    CHECK(peak <= 1.975e-2);
    CHECK(std::fabs(one_minus_fs_at_peak - 0.818) < 0.005);
}

TEST_CASE("the second method at the sample positions of the five configurations the maintainer's checks measured", "[penumbral_cancellation][real_tree]") {
    // C10_proof_registration.txt, AMENDMENT 2, R4.  The five configurations: the baseline row of the 801 x 400 sweep, the three hard tilt rows (0.97, 0.995, 0.9995) at the family-1 call, and the 0.9995 row of
    // the 401 x 200 refinement.  Every sample of each was compared in the facts file (largest |port - analytic|: 1.25e-11 for u = 0.6, 1.3e-13 for u = 0); here a subset of the samples (each 20th, 30th, 30th,
    // 30th, 10th), the positions from the pieces of the tool; tolerance 16 times the measured maximum, to one figure: 2e-10 and 2e-12.
    struct Config {
        const char* name;
        double frac;
        pc::StudyOptions options;
        std::size_t stride;
    };
    pc::StudyOptions base;
    base.nphi_coarse = 150;
    base.nphi_fine = 400;
    base.n_samples = 801;
    pc::StudyOptions tilt;
    tilt.nphi_coarse = 400;
    tilt.nphi_fine = 450;
    tilt.n_samples = 1201;
    pc::StudyOptions refined;
    refined.nphi_coarse = 200;
    refined.nphi_fine = 200;
    refined.n_samples = 401;
    const std::vector<Config> configs = {{"baseline 801x400", 0.0, base, 20}, {"tilt 0.97", 0.97, tilt, 30}, {"tilt 0.995", 0.995, tilt, 30}, {"tilt 0.9995", 0.9995, tilt, 30}, {"0.9995 refined 401x200", 0.9995, refined, 10}};
    for (const Config& c : configs) {
        const pc::Orbit orb = pc::orbit_for(kRLeo, 0.0, c.frac * kAEarthLeo, 0.0);
        const double t0 = pc::locate_closest_approach(orb);
        const pc::Window w = pc::find_window(orb, t0, c.options);
        REQUIRE(w.ok);
        const std::vector<double> tt = pc::sample_times(w.dt_lo, w.dt_hi, c.options.n_samples);
        double worst6 = 0.0, worst0 = 0.0, worst_du = 0.0;
        std::size_t compared = 0;
        std::size_t in_penumbra = 0;
        for (std::size_t i = 0; i < tt.size(); i += c.stride) {
            const Vec3 pos = orb.pos(t0 + tt.at(i));
            const double p6 = pc::fs(kSun, pos, pc::kREarthM, pc::kEddingtonU, c.options.nphi_fine);
            const double p0 = pc::fs(kSun, pos, pc::kREarthM, 0.0, c.options.nphi_fine);
            const double e6 = fs_edges(kSun, pos, pc::kREarthM, pc::kEddingtonU, c.options.nphi_fine);
            const double e0 = fs_edges(kSun, pos, pc::kREarthM, 0.0, c.options.nphi_fine);
            worst6 = std::max(worst6, std::fabs(p6 - e6));
            worst0 = std::max(worst0, std::fabs(p0 - e0));
            worst_du = std::max(worst_du, std::fabs((p6 - p0) - (e6 - e0)));
            ++compared;
            if (e0 > 1e-6 && e0 < 1.0 - 1e-6) ++in_penumbra;
        }
        INFO(c.name << ": " << compared << " positions, " << in_penumbra << " in the penumbra; worst |port - edges| u = 0.6: " << worst6 << ", u = 0: " << worst0 << ", du: " << worst_du);
        CHECK(compared >= 40);
        CHECK(in_penumbra >= 25);   // the positions really are across the passage (the window is 15 % wider on each side than the penumbra: about three quarters of them are inside it)
        CHECK(worst6 < 2e-10);
        CHECK(worst0 < 2e-12);
        CHECK(worst_du < 2e-10);
    }
}

TEST_CASE("the near-grazing row converges to 48.29 as the azimuthal resolution grows", "[penumbral_cancellation][real_tree]") {
    // C10_proof_registration.txt, AMENDMENT 2, R5, written before this case existed.  The 0.9995 row at the family-1 call (nphi_coarse 400, n_samples 1201, coarse_n 300) at nphi_fine 450, 900, 1800; the
    // converged value 48.29 is that of the closed-form-edge method at 32 times the resolution (48.2927, C10_penumbral_exploration_facts.txt, S1).  PREDICTION from the facts file (S4): the distances
    // d(n) = |cancels(nphi_fine = n) - 48.29| are 0.024, 0.0076, 0.0033.  THRESHOLDS fixed in advance: d(450) > 0.015, d(900) < 0.012, d(1800) < 0.006, and d(450) > d(900) > d(1800).
    pc::StudyOptions options;
    options.nphi_coarse = 400;
    options.n_samples = 1201;
    options.coarse_n = 300;
    double d[3];
    const int nphi[3] = {450, 900, 1800};
    for (int k = 0; k < 3; ++k) {
        options.nphi_fine = nphi[k];
        const pc::Result r = pc::study(kRLeo, 0.0, 0.9995 * kAEarthLeo, 0.0, options);
        REQUIRE(r.ok);
        d[k] = std::fabs(r.cancel_pct - 48.29);
        INFO("nphi_fine " << nphi[k] << ": cancels " << r.cancel_pct << ", distance from 48.29: " << d[k]);
    }
    CHECK(d[0] > 0.015);
    CHECK(d[1] < 0.012);
    CHECK(d[2] < 0.006);
    CHECK(d[0] > d[1]);
    CHECK(d[1] > d[2]);
}

}  // namespace
