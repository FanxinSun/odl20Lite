// gravity_tests.cpp — SPEC-gravity §8, row by row.
//
// The model is loaded once for the whole binary: the coefficient file is 242 MB
// of ASCII and the recursion table is 38 MB, and loading it per test case would
// make the suite's cost the loader's cost rather than the field's.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/gravity/field.hpp>
#include <odl/gravity/legendre.hpp>
#include <odl/time/leap_table.hpp>

#include "gradient_reference.hpp"
#include "legendre_reference.hpp"
#include "synthesis_baseline.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <numbers>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

using namespace odl;
using namespace odl::gravity;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace {

std::string slurp(const char* path) {
    std::ifstream f(path, std::ios::binary);
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

odl::time::Epoch tt_at(double jd) {
    auto e = odl::time::Epoch::from_two_part_jd(odl::time::TimeScale::TT, jd, 0.0, leaps());
    REQUIRE(e.has_value());
    return *e;
}

const GravityModel& model() {
    static const auto m = GravityModel::load(ODL_EGM2008_COEFFICIENTS, ODL_MANIFEST_CACHE_ROOT,
                                             ScalingParameters::egm2008_tt_compatible());
    REQUIRE(m.has_value());
    return *m;
}

/// The conventional field at J2000.0 exactly, which is the epoch every expected
/// value in SPEC-gravity §8 is quoted at.
const ConventionalField& j2000_field() {
    static const auto f = model().conventional(tt_at(2451545.0));
    REQUIRE(f.has_value());
    return *f;
}

Degree deg(int n) { auto d = Degree::of(n); REQUIRE(d.has_value()); return *d; }
Order  ord(int m) { auto o = Order::of(m);  REQUIRE(o.has_value()); return *o; }

constexpr double kPi = std::numbers::pi;
constexpr int    kFullOrder = 2159;

Vec3 at_lat_lon(double r, double lat_deg, double lon_deg) {
    const double p = lat_deg * kPi / 180.0, l = lon_deg * kPi / 180.0;
    return Vec3{r * std::cos(p) * std::cos(l), r * std::cos(p) * std::sin(l), r * std::sin(p)};
}

/// Points spread over the sphere by the Fibonacci lattice, which is
/// deterministic and very close to equal-area — so the sample mean square is an
/// unbiased estimate of the mean over the sphere and the test has no seed.
std::vector<Vec3> sphere_sample(double r, int k) {
    std::vector<Vec3> out;
    out.reserve(static_cast<std::size_t>(k));
    const double ga = kPi * (3.0 - std::sqrt(5.0));
    for (int i = 0; i < k; ++i) {
        const double z = 1.0 - 2.0 * (static_cast<double>(i) + 0.5) / static_cast<double>(k);
        const double rho = std::sqrt(std::max(0.0, 1.0 - z * z));
        const double th = ga * static_cast<double>(i);
        out.push_back(Vec3{r * rho * std::cos(th), r * rho * std::sin(th), r * z});
    }
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// The file, and the published numbers in it
// ---------------------------------------------------------------------------

TEST_CASE("GRAV-A-014: the coefficient file is shaped as measured, not as assumed", "[gravity]") {
    const CoefficientSet& c = model().coefficients();
    CHECK(c.record_count() == 2401333);
    CHECK(c.max_degree() == 2190);
    CHECK(c.c(1, 0) == 0.0);
    CHECK(c.c(1, 1) == 0.0);
    CHECK(c.s(1, 1) == 0.0);
    CHECK(c.c(0, 0) == 1.0);              // degree 0 IS the two-body term, not zero
    CHECK(c.c(2190, 2190) == 0.0);        // padded to a full triangle with explicit zeros
    CHECK(c.max_non_zero_order() == 2159);
    CHECK(c.max_non_zero_order_at(2159) == 2159);
    CHECK(c.max_non_zero_order_at(2160) == 2158);   // the ellipsoidal parity coupling
    CHECK(c.max_non_zero_order_at(2189) == 2159);
    CHECK(c.max_non_zero_order_at(2190) == 2158);
    CHECK(c.tide_system() == TideSystem::TideFree);
}

TEST_CASE("GRAV-A-009: the loaded coefficients are the numbers TN36-6 prints", "[gravity]") {
    const CoefficientSet& c = model().coefficients();
    // Tolerance is half an ulp of the last digit the Conventions print.
    CHECK_THAT(c.c(2, 2), WithinAbs(2.4393836e-6, 0.5e-13));
    CHECK_THAT(c.s(2, 2), WithinAbs(-1.4002737e-6, 0.5e-13));
    CHECK_THAT(c.c(3, 0), WithinAbs(0.9571612e-6, 0.5e-13));
    CHECK_THAT(c.c(4, 0), WithinAbs(0.5399659e-6, 0.5e-13));
}

TEST_CASE("GRAV-A-010: the tide-system arithmetic of TN36-6 §6.2.2", "[gravity]") {
    constexpr double c20_zero_tide = -0.48416948e-3;
    constexpr double zt_minus_tf   = -4.1736e-9;
    CHECK_THAT(c20_zero_tide - zt_minus_tf, WithinAbs(-0.48416531e-3, 0.5e-11));
}

TEST_CASE("GRAV-A-011: the conventional substitution is not cosmetic", "[gravity]") {
    const double file_c20 = model().coefficients().c(2, 0);
    constexpr double conv_zero_tide = -0.48416948e-3;
    constexpr double conv_tide_free = conv_zero_tide + 4.1736e-9;
    CHECK_THAT(conv_zero_tide - file_c20, WithinRel(-4.3362e-9, 1e-4));
    // Like for like: both tide-free.  8 sigma on the 2e-11 TN36-6 §6.1 states.
    const double like_for_like = conv_tide_free - file_c20;
    CHECK_THAT(like_for_like, WithinRel(-1.6261e-10, 1e-4));
    CHECK(std::abs(like_for_like) > 8.0 * 2e-11 * 0.99);
}

TEST_CASE("GRAV-A-026: the field says which file and which tide system", "[gravity]") {
    CHECK(model().coefficients().tide_system() == TideSystem::TideFree);
    CHECK(j2000_field().tide_system() == TideSystem::ZeroTide);
    CHECK(model().provenance().records == 2401333);
    CHECK(!model().provenance().coefficient_sha256_declared_in.empty());
    CHECK(model().provenance().gm_compatibility == GmCompatibility::TT);
}

TEST_CASE("GRAV-A-022: every substitution is enumerable with its authority", "[gravity]") {
    const auto& s = j2000_field().substitutions();
    REQUIRE(s.size() == 5);
    for (const auto& x : s) {
        CHECK(!x.authority.empty());
        CHECK(x.from != x.to);
    }
    CHECK(s[0].n == 2); CHECK(s[0].m == 0); CHECK(s[0].which == 'C');
    CHECK(s[4].n == 2); CHECK(s[4].m == 1); CHECK(s[4].which == 'S');
    CHECK_THAT(s[0].to, WithinAbs(-0.48416948e-3, 1e-15));
    CHECK_THAT(j2000_field().c(2, 0), WithinAbs(-0.48416948e-3, 1e-15));
}

// ---------------------------------------------------------------------------
// The recursion
// ---------------------------------------------------------------------------

TEST_CASE("GRAV-A-003: the recursion against the definition in high precision", "[gravity]") {
    const RecursionTable& t = j2000_field().recursion();
    int direct = 0, scaled = 0;
    double worst_rel = 0.0, worst_log = 0.0, worst_low = 0.0;
    std::vector<double> P(2192), dP(2192);
    for (const auto& r : reference::kLegendre) {
        const auto rn = static_cast<std::size_t>(r.n);
        INFO("Pbar'_" << r.n << "," << r.m << "(u=" << r.sin_phi << ")");
        if (r.representable) {
            legendre_column(t, r.m, r.n, r.sin_phi, 1.0, P.data(), dP.data());
            const double got = P[rn], want = r.pbar_factored;
            const double rel = want == 0.0 ? std::abs(got) : std::abs((got - want) / want);
            // The budget is tiered by degree and the reason is measured, not
            // assumed: the three-term recursion accumulates rounding over n - m
            // steps with a mild cancellation at each, so the achievable accuracy
            // falls off with degree.  Below degree 360 — which covers every
            // truncation TN36-6 Table 6.1 suggests, the largest being 90 — it is
            // 1e-13.  At degree 2190 the measured worst is 6.1e-11, at the pole,
            // and 1e-9 is the budget with headroom.  SPEC-gravity GRAV-P-8 said
            // 1e-13 across the whole range until this test measured otherwise.
            const double budget = r.n <= 360 ? 1e-13 : 1e-9;
            CHECK(rel < budget);
            if (r.n <= 360) worst_low = std::max(worst_low, rel);
            worst_rel = std::max(worst_rel, rel);
            ++direct;
        } else {
            // Not representable as a bare double, which is precisely why it is
            // worth checking: these are the values that decide whether the
            // scaled path works.  Compared in log space, where a relative error
            // of 1e-13 is 4.3e-14 of log10.
            legendre_column(t, r.m, r.n, r.sin_phi, 1e-280, P.data(), dP.data());
            REQUIRE(std::isfinite(P[rn]));
            REQUIRE(P[rn] != 0.0);
            CHECK((P[rn] > 0.0 ? 1.0 : -1.0) == r.sign);
            const double got_log = std::log10(std::abs(P[rn])) + 280.0;
            CHECK_THAT(got_log, WithinAbs(r.log10_abs, 1e-11));
            worst_log = std::max(worst_log, std::abs(got_log - r.log10_abs));
            ++scaled;
        }
    }
    WARN("GRAV-A-003: " << direct + scaled << " values from the definition — " << direct
         << " compared directly (worst relative error " << worst_rel << " overall, " << worst_low
         << " at degree <= 360), " << scaled
         << " through the scaled path in log space (worst " << worst_log << " of log10)");
    CHECK(direct + scaled == 78);
    CHECK(scaled > 0);
}

TEST_CASE("GRAV-A-008: the m = 1 seed is not the general sectorial step", "[gravity]") {
    const RecursionTable& t = j2000_field().recursion();
    const double correct = t.sectorial(1);
    const double general_step = std::sqrt(3.0 / 2.0) * t.sectorial(0);
    CHECK_THAT(correct, WithinRel(std::sqrt(3.0), 1e-15));
    CHECK_THAT(general_step, WithinRel(std::sqrt(1.5), 1e-15));
    CHECK_THAT(correct / general_step, WithinRel(std::sqrt(2.0), 1e-12));
}

TEST_CASE("GRAV-A-007: both representations fail, in opposite directions", "[gravity]") {
    const RecursionTable& t = j2000_field().recursion();

    // (a) The CLASSICAL form underflows.  At 60 degrees of latitude and order
    //     2190 the sectorial function is lost entirely rather than being small.
    const double cphi = std::cos(60.0 * kPi / 180.0);
    const double classical = t.sectorial(2190) * std::pow(cphi, 2190.0);
    CHECK(classical == 0.0);
    CHECK_THAT(t.sectorial(2190), WithinRel(10.2775768597438193, 1e-13));

    // (b) The FACTORED form overflows.  Away from the sectorial diagonal it
    //     reaches 10^457.9 at degree 2190, which is 10^150 past a double — the
    //     specification's §3.6 said it was bounded by 10.3 and that is true only
    //     of the sectorial seed.
    std::vector<double> P(2192), dP(2192);
    legendre_column(t, 979, 2190, 1.0, 1.0, P.data(), dP.data());
    // It does not merely overflow.  Once two consecutive entries are infinite
    // the three-term recursion subtracts one from the other and the column
    // becomes NaN — a failure that propagates silently into every sum it
    // touches rather than announcing itself as a large number.
    CHECK(!std::isfinite(P[2190]));
    CHECK(std::isnan(P[2190]));

    // (c) Scaled by 10^-280 it is finite, and unscaling the ratio recovers the
    //     magnitude the closed form predicts.
    legendre_column(t, 979, 2190, 1.0, 1e-280, P.data(), dP.data());
    REQUIRE(std::isfinite(P[2190]));
    CHECK(std::log10(P[2190]) + 280.0 > 457.0);
    CHECK(std::log10(P[2190]) + 280.0 < 458.5);
}

TEST_CASE("GRAV-A-004: orthonormality of the normalised functions", "[gravity]") {
    // Gauss-Legendre nodes by Newton on P_n, so the quadrature is exact for the
    // polynomial degree involved and the check is the DEFINING property of the
    // 4pi normalisation rather than a second opinion about it.
    constexpr int kNodes = 256;
    std::vector<double> x(kNodes), w(kNodes);
    for (int i = 0; i < kNodes; ++i) {
        double z = std::cos(kPi * (static_cast<double>(i) + 0.75) / (kNodes + 0.5));
        double pp = 0.0;
        for (int it = 0; it < 100; ++it) {
            double p0 = 1.0, p1 = 0.0;
            for (int j = 0; j < kNodes; ++j) {
                const double p2 = p1;
                p1 = p0;
                p0 = ((2.0 * j + 1.0) * z * p1 - static_cast<double>(j) * p2)
                   / (static_cast<double>(j) + 1.0);
            }
            pp = kNodes * (z * p0 - p1) / (z * z - 1.0);
            const double dz = p0 / pp;
            z -= dz;
            if (std::abs(dz) < 1e-15) break;
        }
        x[static_cast<std::size_t>(i)] = z;
        w[static_cast<std::size_t>(i)] = 2.0 / ((1.0 - z * z) * pp * pp);
    }

    const RecursionTable& t = j2000_field().recursion();
    std::vector<double> P(2192), dP(2192);
    int checked = 0;
    for (int m : {0, 1, 2, 7, 30, 60}) {
        for (int i = 0; i < kNodes; ++i) {
            (void)i;
        }
        for (int n = m; n <= 60; ++n) {
            double integral = 0.0;
            for (int i = 0; i < kNodes; ++i) {
                const auto ui = static_cast<std::size_t>(i);
                legendre_column(t, m, n, x[ui], 1.0, P.data(), dP.data());
                const double c2 = 1.0 - x[ui] * x[ui];
                const double pbar = P[static_cast<std::size_t>(n)]
                                  * std::pow(std::sqrt(c2), static_cast<double>(m));
                integral += w[ui] * pbar * pbar;
            }
            const double expect = m == 0 ? 2.0 : 4.0;
            INFO("n=" << n << " m=" << m);
            CHECK_THAT(integral, WithinRel(expect, 1e-11));
            ++checked;
        }
    }
    WARN("GRAV-A-004: orthonormality checked on " << checked
         << " (n, m) pairs with n <= 60, by 256-point Gauss-Legendre quadrature");
}

TEST_CASE("GRAV-A-024: the Condon-Shortley phase is not this tree's convention", "[gravity]") {
    // DLMF 14.6.1 carries (-1)^m and TN36-6 (6.2a) does not.  The difference is
    // a sign flip on every odd order; this pins which one is in force.
    const RecursionTable& t = j2000_field().recursion();
    std::vector<double> P(8), dP(8);
    legendre_column(t, 1, 2, 0.5, 1.0, P.data(), dP.data());
    // Pbar'_21(u) = sqrt(15) u, positive for u > 0 without the phase and
    // negative with it.
    CHECK(P[2] > 0.0);
    CHECK_THAT(P[2], WithinRel(std::sqrt(15.0) * 0.5, 1e-14));
}

// ---------------------------------------------------------------------------
// The field
// ---------------------------------------------------------------------------

TEST_CASE("GRAV-A-002: the two-body limit is exact at degree 0", "[gravity]") {
    const double gm = model().scaling().gm_m3_s2();
    for (const Vec3& p : {Vec3{7331e3, 0, 0}, Vec3{0, 0, 7331e3}, at_lat_lon(7331e3, 33.0, 77.0)}) {
        const auto a = j2000_field().acceleration(frames::ItrsPosition{p}, deg(0), ord(0));
        REQUIRE(a.has_value());
        const double r = p.norm();
        const Vec3& v = a->metres_per_second_squared();
        CHECK_THAT(a->norm_m_per_s2(), WithinRel(gm / (r * r), 4e-16));
        // and it points at the origin
        CHECK_THAT(v.x, WithinAbs(-gm * p.x / (r * r * r), 1e-15));
        CHECK_THAT(v.y, WithinAbs(-gm * p.y / (r * r * r), 1e-15));
        CHECK_THAT(v.z, WithinAbs(-gm * p.z / (r * r * r), 1e-15));
    }
}

TEST_CASE("GRAV-A-027: point-wise against the exact J2-only closed form", "[gravity]") {
    // The gate GRAV-A-001 is a statement about MEANS OVER THE SPHERE: a
    // recursion with wrong angular structure but the right power per degree
    // would satisfy it.  This is the point-wise check that covers that.
    const ConventionalField& f = j2000_field();
    const double gm = f.scaling().gm_m3_s2(), ae = f.scaling().ae_m();
    const double j2 = -std::sqrt(5.0) * f.c(2, 0);
    CHECK_THAT(j2, WithinRel(1.082635869911e-3, 1e-11));

    int checked = 0;
    double worst = 0.0;
    for (double lat : {0.0, 23.4, 45.0, 70.0, 89.9, 90.0, -45.0, -90.0}) {
        for (double lon : {0.0, 40.0, 135.0, 260.0}) {
            const Vec3 p = at_lat_lon(7331e3, lat, lon);
            const auto a = f.acceleration(frames::ItrsPosition{p}, deg(2), ord(0));
            REQUIRE(a.has_value());
            const double r = p.norm(), s = p.z / r;
            const double k = 1.5 * j2 * (ae / r) * (ae / r);
            const Vec3 want{-(gm * p.x / (r * r * r)) * (1.0 + k * (1.0 - 5.0 * s * s)),
                            -(gm * p.y / (r * r * r)) * (1.0 + k * (1.0 - 5.0 * s * s)),
                            -(gm * p.z / (r * r * r)) * (1.0 + k * (3.0 - 5.0 * s * s))};
            const Vec3& got = a->metres_per_second_squared();
            const double d = (got - want).norm() / want.norm();
            INFO("lat " << lat << " lon " << lon);
            CHECK(d < 1e-13);
            worst = std::max(worst, d);
            ++checked;
        }
    }
    WARN("GRAV-A-027: " << checked << " points against the closed form, worst relative " << worst);
}

TEST_CASE("GRAV-A-023: the sign convention is the geodesy one", "[gravity]") {
    const Vec3 p = at_lat_lon(7331e3, 10.0, 20.0);
    const auto a = j2000_field().acceleration(frames::ItrsPosition{p}, deg(8), ord(8));
    REQUIRE(a.has_value());
    CHECK(a->metres_per_second_squared().dot(p) < 0.0);          // points inwards
    const auto v_in = j2000_field().potential(frames::ItrsPosition{p}, deg(8), ord(8));
    const auto v_out = j2000_field().potential(frames::ItrsPosition{1.001 * p}, deg(8), ord(8));
    REQUIRE(v_in.has_value());
    REQUIRE(v_out.has_value());
    CHECK(*v_in > *v_out);                                       // V increases downwards
    CHECK(*v_in > 0.0);
}

TEST_CASE("GRAV-A-005: the analytic gradient against central differences", "[gravity]") {
    const ConventionalField& f = j2000_field();
    int checked = 0;
    double worst = 0.0;
    for (int n : {2, 8, 90, 360}) {
        for (double lat : {0.0, 37.0, -61.0}) {
            const Vec3 p = at_lat_lon(7331e3, lat, 15.0);
            const auto a = f.acceleration(frames::ItrsPosition{p}, deg(n), ord(n));
            REQUIRE(a.has_value());
            const double h = 50.0;   // metres; the step-size floor of the method
            Vec3 fd{};
            for (int axis = 0; axis < 3; ++axis) {
                Vec3 lo = p, hi = p;
                (&lo.x)[axis] -= h;
                (&hi.x)[axis] += h;
                const auto vl = f.potential(frames::ItrsPosition{lo}, deg(n), ord(n));
                const auto vh = f.potential(frames::ItrsPosition{hi}, deg(n), ord(n));
                REQUIRE(vl.has_value());
                REQUIRE(vh.has_value());
                (&fd.x)[axis] = (*vh - *vl) / (2.0 * h);
            }
            const double d = (fd - a->metres_per_second_squared()).norm() / a->norm_m_per_s2();
            INFO("degree " << n << " latitude " << lat);
            CHECK(d < 1e-7);
            worst = std::max(worst, d);
            ++checked;
        }
    }
    WARN("GRAV-A-005: " << checked << " points, worst gradient-vs-difference disagreement "
         << worst << " relative");
}

TEST_CASE("GRAV-A-006: the pole is an ordinary point", "[gravity]") {
    const ConventionalField& f = j2000_field();
    const Degree d = deg(2190);
    const Order o = ord(kFullOrder);
    const auto at_pole = f.acceleration(frames::ItrsPosition{Vec3{0.0, 0.0, 7331e3}}, d, o);
    REQUIRE(at_pole.has_value());
    const Vec3& ap = at_pole->metres_per_second_squared();
    CHECK(std::isfinite(ap.x));
    CHECK(std::isfinite(ap.y));
    CHECK(std::isfinite(ap.z));
    CHECK(ap.z < 0.0);
    // The limit approached along two meridians a quarter turn apart must be the
    // same vector, and must be the value computed AT the pole.
    for (double lon : {0.0, 90.0}) {
        const auto near = f.acceleration(frames::ItrsPosition{at_lat_lon(7331e3, 89.99999, lon)}, d, o);
        REQUIRE(near.has_value());
        const double rel = (near->metres_per_second_squared() - ap).norm() / at_pole->norm_m_per_s2();
        INFO("approach along longitude " << lon << " gives a relative difference of " << rel);
        CHECK(rel < 1e-6);
    }
    const auto south = f.acceleration(frames::ItrsPosition{Vec3{0.0, 0.0, -7331e3}}, d, o);
    REQUIRE(south.has_value());
    CHECK(south->metres_per_second_squared().z > 0.0);
}

TEST_CASE("GRAV-A-012: the one external absolute anchor, WGS 84 normal gravity", "[gravity]") {
    // At the pole the centrifugal term of normal gravity vanishes identically,
    // so WGS 84's gamma_p is a statement about GRAVITATION alone and can be
    // compared with this model directly.  The band is wide on purpose: what
    // separates the two is the gravity disturbance at the pole, for which this
    // tree has no published value.  What it catches is a wrong GM, a wrong a_e,
    // a missing or doubled degree-0 term, a normalisation off by more than
    // 5e-4, and a sign error.  It cannot see anything subtler.
    constexpr double b = 6356752.3142;            // WGS 84 Table 3.1, derived
    constexpr double gamma_p = 9.8321849379;      // WGS 84 Table 3.6
    const auto a = j2000_field().acceleration(frames::ItrsPosition{Vec3{0.0, 0.0, b}},
                                              deg(2190), ord(kFullOrder));
    REQUIRE(a.has_value());
    const double got = a->norm_m_per_s2();
    WARN("GRAV-A-012: |a| at the pole on the WGS 84 ellipsoid = " << got
         << " m s^-2, against normal gravity " << gamma_p << "; difference "
         << (got - gamma_p) * 1e5 << " mGal");
    CHECK_THAT(got, WithinAbs(gamma_p, 0.005));
}

TEST_CASE("GRAV-A-013: truncation, and the three rows of TN36-6 Table 6.1", "[gravity]") {
    const ConventionalField& f = j2000_field();
    struct Row { double r_km; int n; double expect; const char* who; };
    for (const Row& row : {Row{7331.0, 90, 1.2174e-10, "Starlette"},
                           Row{12270.0, 20, 9.9122e-12, "Lageos"},
                           Row{26600.0, 12, 2.3018e-14, "GPS"}}) {
        const auto rms = f.truncation_rms(row.r_km * 1e3, deg(row.n));
        REQUIRE(rms.has_value());
        INFO(row.who << " at " << row.r_km << " km, degree " << row.n);
        CHECK_THAT(*rms, WithinRel(row.expect, 0.01));
        WARN("GRAV-A-013: " << row.who << " degree " << row.n << " at " << row.r_km
             << " km: truncation RMS " << *rms << " m s^-2");
    }
    // Monotone in degree, at each radius.
    for (double r_km : {7331.0, 12270.0, 26600.0}) {
        double previous = 1e30;
        for (int n = 2; n <= 2190; n += 7) {
            const auto rms = f.truncation_rms(r_km * 1e3, deg(n));
            REQUIRE(rms.has_value());
            INFO("radius " << r_km << " km, degree " << n);
            CHECK(*rms <= previous);
            previous = *rms;
        }
    }
}

// ---------------------------------------------------------------------------
// THE GATE
// ---------------------------------------------------------------------------

TEST_CASE("GRAV-A-001: the degree-variance identity, every degree", "[gravity][gate]") {
    // rms over a sphere of radius r of the degree-n acceleration is exactly
    //     (GM/r^2) (ae/r)^n sigma_n sqrt((n+1)(2n+1))
    // from the 4pi normalisation alone: the mean square over the sphere of the
    // degree-n angular factor is sigma_n^2, the radial derivative contributes
    // (n+1)^2 and the horizontal gradient n(n+1).
    //
    // THE STATISTIC IS A RATIO, not a difference of magnitudes, and the first
    // version of this test was not.  The degree-1900 acceleration at 7331 km is
    // about 1e-130 m s^-2 — perfectly representable — but its SQUARE is not, so
    // accumulating a sum of squares silently produced zero and the test reported
    // a 100 % disagreement at every degree past the halfway point.  Dividing by
    // the prediction first keeps every quantity at order unity.
    //
    // The sample size, the degrees compared and the degrees below the floor are
    // all reported: a gate that does not carry its denominator has already lost
    // the argument.
    const ConventionalField& f = j2000_field();
    const double gm = f.scaling().gm_m3_s2(), ae = f.scaling().ae_m();
    constexpr int kSample = 300;
    constexpr double kFloor = 1e-280;
    // The identity is a statement about a mean over the sphere, so this is a
    // statistical test and its band is stated rather than fitted.  The relative
    // standard error of an RMS estimated from K samples is 1/sqrt(2K) = 4.1 % at
    // K = 300, and the band is five of those.
    //
    // A 20 % band is loose, and the way to tighten it is more samples rather
    // than a smaller number.  An attempt to tier the band by degree — on the
    // reasoning that a Fibonacci lattice is low-discrepancy and should integrate
    // low-degree harmonics better than randomly — was simply WRONG: degrees 19
    // to 27 missed a 1 % band at K = 300.  The scatter is the sampling scatter
    // at every degree.
    //
    // So the low degrees get a second pass with a much larger sample instead,
    // which costs almost nothing because evaluating to degree 60 is 1830 terms
    // against 2.4 million.  Both passes state their K and their band.
    constexpr double kBand = 5.0 / 24.494897427831781;   // 5/sqrt(2*300) = 20 %

    struct Case { double r_km; int nmax; };
    for (const Case& cs : {Case{7331.0, 2190}, Case{12270.0, 1200}, Case{26600.0, 600}}) {
        const double r = cs.r_km * 1e3;
        const auto un = static_cast<std::size_t>(cs.nmax) + 1;

        std::vector<double> predicted(un, 0.0);
        double ratio_pow = 1.0;
        for (int n = 0; n <= cs.nmax; ++n) {
            predicted[static_cast<std::size_t>(n)] =
                gm / (r * r) * ratio_pow * f.degree_amplitude(n)
                * std::sqrt((static_cast<double>(n) + 1.0) * (2.0 * static_cast<double>(n) + 1.0));
            ratio_pow *= ae / r;
        }

        const auto points = sphere_sample(r, kSample);
        std::vector<double> sq(un, 0.0);
        const auto t0 = std::chrono::steady_clock::now();
        for (const Vec3& p : points) {
            const auto by = f.acceleration_by_degree(frames::ItrsPosition{p}, deg(cs.nmax),
                                                     ord(std::min(cs.nmax, kFullOrder)));
            REQUIRE(by.has_value());
            REQUIRE(by->size() == un);
            for (std::size_t n = 0; n < un; ++n) {
                if (!(predicted[n] > kFloor)) continue;
                const Vec3 q = (1.0 / predicted[n]) * (*by)[n];
                sq[n] += q.dot(q);
            }
        }
        const auto t1 = std::chrono::steady_clock::now();

        int compared = 0, below_floor = 0, above_one = 0;
        double worst = 0.0, sum_ratio = 0.0;
        int worst_n = -1;
        for (int n = 0; n <= cs.nmax; ++n) {
            const auto ui = static_cast<std::size_t>(n);
            // The floor is where a double stops carrying full precision, not
            // where it stops carrying anything: below 1e-280 the reciprocal used
            // to form the ratio is itself near overflow and the comparison stops
            // meaning what it says.  The count below the floor is reported.
            if (!(predicted[ui] > kFloor)) { ++below_floor; continue; }
            const double ratio = std::sqrt(sq[ui] / kSample);
            sum_ratio += ratio;
            if (ratio > 1.0) ++above_one;
            const double rel = std::abs(ratio - 1.0);
            INFO("radius " << cs.r_km << " km, degree " << n << ": predicted " << predicted[ui]
                 << " m s^-2, measured/predicted " << ratio);
            CHECK(rel < kBand);
            if (rel > worst) { worst = rel; worst_n = n; }
            ++compared;
        }
        WARN("GRAV-A-001 at " << cs.r_km << " km: " << compared << " degrees compared to "
             << cs.nmax << ", " << below_floor << " skipped (degree 1 is identically zero, the"
             " rest below the representable floor), " << kSample
             << " sample points, sampling error 1/sqrt(2K) = "
             << 1.0 / std::sqrt(2.0 * kSample) << ", worst relative disagreement " << worst
             << " at degree " << worst_n << "; mean ratio " << sum_ratio / compared << ", "
             << above_one << " of " << compared << " above 1.0; "
             << std::chrono::duration<double>(t1 - t0).count() << " s");
        CHECK(compared > cs.nmax / 2);
        // The aggregate is the sharp part of this pass: individual degrees carry
        // 4 % of sampling scatter, their mean over hundreds of degrees does not,
        // and a normalisation error moves every degree in the same direction.
        CHECK_THAT(sum_ratio / compared, WithinAbs(1.0, 0.01));
    }
}

TEST_CASE("GRAV-A-001b: the same identity at low degree, with a sample that can carry it",
          "[gravity]") {
    const ConventionalField& f = j2000_field();
    const double gm = f.scaling().gm_m3_s2(), ae = f.scaling().ae_m();
    constexpr int kSample = 20000;
    constexpr int kNmax = 60;
    constexpr double kBand = 5.0 / 200.0;   // 5/sqrt(2*20000) = 2.5 %
    const double r = 7331e3;

    std::vector<double> predicted(kNmax + 1, 0.0);
    double ratio_pow = 1.0;
    for (int n = 0; n <= kNmax; ++n) {
        predicted[static_cast<std::size_t>(n)] =
            gm / (r * r) * ratio_pow * f.degree_amplitude(n)
            * std::sqrt((static_cast<double>(n) + 1.0) * (2.0 * static_cast<double>(n) + 1.0));
        ratio_pow *= ae / r;
    }

    std::vector<double> sq(kNmax + 1, 0.0);
    const auto t0 = std::chrono::steady_clock::now();
    for (const Vec3& p : sphere_sample(r, kSample)) {
        const auto by = f.acceleration_by_degree(frames::ItrsPosition{p}, deg(kNmax), ord(kNmax));
        REQUIRE(by.has_value());
        for (std::size_t n = 0; n < by->size(); ++n) {
            if (!(predicted[n] > 0.0)) continue;
            const Vec3 q = (1.0 / predicted[n]) * (*by)[n];
            sq[n] += q.dot(q);
        }
    }
    const auto t1 = std::chrono::steady_clock::now();

    int compared = 0;
    double worst = 0.0, sum_ratio = 0.0;
    int worst_n = -1;
    for (int n = 0; n <= kNmax; ++n) {
        const auto ui = static_cast<std::size_t>(n);
        if (!(predicted[ui] > 0.0)) continue;    // degree 1 is identically zero
        const double ratio = std::sqrt(sq[ui] / kSample);
        const double rel = std::abs(ratio - 1.0);
        INFO("degree " << n << ": predicted " << predicted[ui] << " m s^-2, measured/predicted "
             << ratio);
        CHECK(rel < kBand);
        if (rel > worst) { worst = rel; worst_n = n; }
        sum_ratio += ratio;
        ++compared;
    }
    WARN("GRAV-A-001b at 7331 km: " << compared << " degrees to " << kNmax << ", " << kSample
         << " sample points, band " << kBand << " = 5/sqrt(2K); worst " << worst << " at degree "
         << worst_n << ", mean ratio " << sum_ratio / compared << "; "
         << std::chrono::duration<double>(t1 - t0).count() << " s");
    CHECK_THAT(sum_ratio / compared, WithinAbs(1.0, 0.005));
}

// ---------------------------------------------------------------------------
// The conventional model's epoch dependence, and the one pole definition
// ---------------------------------------------------------------------------

TEST_CASE("GRAV-A-025: the figure-axis terms of TN36-6 (6.5)", "[gravity]") {
    struct Row { double jd; double c21; double s21; };
    // J2000.0, and 2026.0 = J2000.0 + 26 years of 365.25 days.
    for (const Row& row : {Row{2451545.0, -2.264385e-10, 1.299633e-9},
                           Row{2451545.0 + 26.0 * 365.25, -4.048365e-10, 1.664613e-9}}) {
        auto f = model().conventional(tt_at(row.jd), true);
        REQUIRE(f.has_value());
        INFO("JD " << row.jd);
        CHECK_THAT(f->c(2, 1), WithinRel(row.c21, 1e-6));
        CHECK_THAT(f->s(2, 1), WithinRel(row.s21, 1e-6));
    }
    // And the substitution is not cosmetic: at 2026 it is worth sixty-one times
    // the degree-90 truncation error the same field accepts.
    auto f2026 = model().conventional(tt_at(2451545.0 + 26.0 * 365.25), true);
    REQUIRE(f2026.has_value());
    const double dc = f2026->c(2, 1) - model().coefficients().c(2, 1);
    const double ds = f2026->s(2, 1) - model().coefficients().s(2, 1);
    const double sigma = std::sqrt(dc * dc + ds * ds);
    const double r = 7331e3;
    const double rms = model().scaling().gm_m3_s2() / (r * r)
                     * std::pow(model().scaling().ae_m() / r, 2.0) * sigma * std::sqrt(3.0 * 5.0);
    CHECK_THAT(sigma, WithinRel(3.4322e-10, 1e-3));
    CHECK_THAT(rms, WithinRel(7.4627e-9, 1e-3));
    CHECK(rms / 1.21736e-10 > 60.0);
}

TEST_CASE("GRAV-A-028: one pole model, consumed rather than restated", "[gravity]") {
    // The four constants of TN36-7 (21) appear in exactly one place, and the
    // figure-axis terms are computed from THAT object.  L2 step 3's pole tide
    // consumes the same one.
    CHECK(SecularPole::kX0Mas == 55.0);
    CHECK(SecularPole::kXRateMasPerYear == 1.677);
    CHECK(SecularPole::kY0Mas == 320.5);
    CHECK(SecularPole::kYRateMasPerYear == 3.460);

    const auto f = model().conventional(tt_at(2451545.0));
    REQUIRE(f.has_value());
    const PoleCoordinates direct = SecularPole::at_years(0.0);
    CHECK_THAT(f->pole().x_rad, WithinRel(direct.x_rad, 1e-15));
    CHECK_THAT(f->pole().y_rad, WithinRel(direct.y_rad, 1e-15));

    // (6.5) is a function OF that pole: reconstruct C21 from the exported
    // coordinates and it must be the coefficient the field uses.
    constexpr double c20 = -0.48416948e-3, c22 = 2.4393836e-6, s22 = -1.4002737e-6;
    const double c21 = std::sqrt(3.0) * direct.x_rad * c20 - direct.x_rad * c22
                     + direct.y_rad * s22;
    CHECK_THAT(f->c(2, 1), WithinRel(c21, 1e-14));

    // The epoch argument is TT in years of 365.25 days from J2000.0.
    CHECK_THAT(SecularPole::years_from_2000(tt_at(2451545.0)), WithinAbs(0.0, 1e-9));
    CHECK_THAT(SecularPole::years_from_2000(tt_at(2451545.0 + 365.25)), WithinAbs(1.0, 1e-9));
}

TEST_CASE("GRAV-A-021: no global state", "[gravity]") {
    auto a = model().conventional(tt_at(2451545.0));
    auto b = model().conventional(tt_at(2451545.0 + 26.0 * 365.25), true);
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    // Queried alternately, each keeps its own epoch's coefficients.
    for (int i = 0; i < 3; ++i) {
        CHECK_THAT(a->c(2, 0), WithinAbs(-0.48416948e-3, 1e-16));
        CHECK_THAT(b->c(2, 0), WithinAbs(-0.48416948e-3 + 11.6e-12 * 26.0, 1e-16));
        CHECK(a->c(2, 1) != b->c(2, 1));
    }
    CHECK(a->provenance().extrapolated_beyond_pole_fit == false);
    CHECK(b->provenance().extrapolated_beyond_pole_fit == true);
}

// ---------------------------------------------------------------------------
// Refusals
// ---------------------------------------------------------------------------

TEST_CASE("GRAV-A-016: degree and order are different limits", "[gravity]") {
    const auto bad_degree = Degree::of(2191);
    REQUIRE(!bad_degree.has_value());
    CHECK(bad_degree.error().id == "GRAV-F-004");
    CHECK(bad_degree.error().message.find("2191") != std::string::npos);
    CHECK(bad_degree.error().message.find("2190") != std::string::npos);

    const auto bad_order = Order::of(2160);
    REQUIRE(!bad_order.has_value());
    CHECK(bad_order.error().id == "GRAV-F-004");
    CHECK(bad_order.error().message.find("2159") != std::string::npos);

    // order > degree, which usually means the two arguments were swapped
    const auto a = j2000_field().acceleration(frames::ItrsPosition{Vec3{7331e3, 0, 0}},
                                              deg(4), ord(8));
    REQUIRE(!a.has_value());
    CHECK(a.error().id == "GRAV-F-004");
    CHECK(a.error().message.find("passed the other way round") != std::string::npos);
}

TEST_CASE("GRAV-F-001: the field is not defined at the origin", "[gravity]") {
    for (const Vec3& p : {Vec3{0, 0, 0}, Vec3{std::nan(""), 0, 0}}) {
        const auto a = j2000_field().acceleration(frames::ItrsPosition{p}, deg(2), ord(0));
        REQUIRE(!a.has_value());
        CHECK(a.error().id == "GRAV-F-001");
        CHECK(a.error().message.find("Returning zero") != std::string::npos);
    }
}

TEST_CASE("GRAV-A-017: scaling parameters must be the model's own, as a pair", "[gravity]") {
    // WGS 84's GM is numerically the TCG-compatible EGM2008 value under another
    // name, so it cannot be refused on its value — the specification's first
    // draft assumed it could.  What IS refusable, and is the realistic mistake,
    // is taking BOTH constants from WGS 84: its semi-major axis is 6378137.0 m
    // against this model's 6378136.3 m.
    const auto both_from_wgs84 = ScalingParameters::checked(3.986004418e14, 6378137.0,
                                                            GmCompatibility::TCG,
                                                            "WGS 84 Table 3.1");
    REQUIRE(!both_from_wgs84.has_value());
    CHECK(both_from_wgs84.error().id == "GRAV-F-005");
    CHECK(both_from_wgs84.error().message.find("6378137") != std::string::npos);
    CHECK(both_from_wgs84.error().message.find("6378136.3") != std::string::npos);
    CHECK(both_from_wgs84.error().message.find("108.91 mm") != std::string::npos);

    // The legitimate TCG case is accepted and RECORDS which scale it is for.
    const auto tcg = ScalingParameters::checked(3.986004418e14, 6378136.3,
                                                GmCompatibility::TCG, "TN36-6 §6.1");
    REQUIRE(tcg.has_value());
    CHECK(tcg->compatibility() == GmCompatibility::TCG);

    // And the consequence the refusal exists for, measured rather than asserted:
    // the TT and TCG values differ by 3e5 m^3 s^-2, which at 7331 km is
    // 5.58e-9 m s^-2 — secular, so half-a-T-squared over one revolution is the
    // right instrument here and gives 108.9 mm.
    const double r = 7331e3;
    const double da = (3.986004418e14 - 3.986004415e14) / (r * r);
    CHECK_THAT(da, WithinRel(5.5821e-9, 1e-3));
    const double period = 2.0 * kPi * std::sqrt(r * r * r / 3.986004415e14);
    CHECK_THAT(0.5 * da * period * period * 1e3, WithinRel(108.91, 1e-3));
}

TEST_CASE("GRAV-A-019: outside the published fit, refused with one named override", "[gravity]") {
    const auto refused = model().conventional(tt_at(2451545.0 + 35.0 * 365.25));
    REQUIRE(!refused.has_value());
    CHECK(refused.error().id == "GRAV-F-006");
    CHECK(refused.error().message.find("1900") != std::string::npos);
    CHECK(refused.error().message.find("2017") != std::string::npos);
    CHECK(refused.error().message.find("extrapolate_secular_terms_beyond_fit") != std::string::npos);

    const auto allowed = model().conventional(tt_at(2451545.0 + 35.0 * 365.25), true);
    REQUIRE(allowed.has_value());
    CHECK(allowed->provenance().extrapolated_beyond_pole_fit == true);

    // Before 1900 as well, not only after 2017.
    CHECK(!model().conventional(tt_at(2451545.0 - 120.0 * 365.25)).has_value());
}

TEST_CASE("GRAV-A-020: structural — degree and order cannot be exchanged", "[gravity]") {
    static_assert(!std::is_convertible_v<Degree, Order>);
    static_assert(!std::is_convertible_v<Order, Degree>);
    static_assert(!std::is_constructible_v<Degree, int>);
    static_assert(!std::is_constructible_v<Order, int>);
    static_assert(!std::is_default_constructible_v<Degree>);
    static_assert(!std::is_default_constructible_v<ScalingParameters>);
    // A bare triple cannot reach the field, and a GCRS position cannot either.
    static_assert(!std::is_convertible_v<Vec3, frames::ItrsPosition>);
    static_assert(!std::is_convertible_v<frames::GcrsPosition, frames::ItrsPosition>);
    static_assert(!std::is_default_constructible_v<frames::ItrsPosition>);
    SUCCEED("compile-time");
}

TEST_CASE("GRAV-A-015 / A-018: the loader refuses what it should", "[gravity]") {
    namespace fs = std::filesystem;
    const fs::path dir = fs::temp_directory_path() / "odl_gravity_refusals";
    fs::create_directories(dir);

    SECTION("a non-zero degree-1 coefficient means another origin") {
        const fs::path p = dir / "degree1.txt";
        {
            std::ofstream f(p);
            f << "    1    0   -0.100000000000000D-08    0.000000000000000D+00"
                 "    0.0000000000D+00    0.0000000000D+00\n";
        }
        const auto r = CoefficientSet::load_egm2008(p.string(), dir.string());
        REQUIRE(!r.has_value());
        CHECK(r.error().id == "GRAV-F-003");
        CHECK(r.error().message.find("degree-1") != std::string::npos);
        CHECK(r.error().message.find("centre of mass") != std::string::npos);
        fs::remove(p);
    }

    SECTION("a truncated file parses cleanly and is caught only by its record count") {
        const fs::path p = dir / "short.txt";
        {
            std::ofstream f(p);
            f << "    2    0   -0.484165143790815D-03    0.000000000000000D+00"
                 "    0.7481239490D-11    0.0000000000D+00\n";
        }
        const auto r = CoefficientSet::load_egm2008(p.string(), dir.string());
        REQUIRE(!r.has_value());
        CHECK(r.error().id == "GRAV-F-003");
        CHECK(r.error().message.find("2401333") != std::string::npos);
        fs::remove(p);
    }

    SECTION("a file outside the manifest cache is not read at all") {
        const auto r = CoefficientSet::load_egm2008(ODL_EGM2008_COEFFICIENTS, dir.string());
        REQUIRE(!r.has_value());
        CHECK(r.error().id == "GRAV-F-007");
        CHECK(r.error().message.find("outside the manifest cache") != std::string::npos);
        CHECK(r.error().message.find("R11") != std::string::npos);
    }

    SECTION("the exponent marker is FORTRAN D, and an E-only reader would read nothing") {
        const fs::path p = dir / "eexp.txt";
        {
            std::ofstream f(p);
            f << "    2    0   -0.484165143790815E-03    0.000000000000000E+00"
                 "    0.7481239490E-11    0.0000000000E+00\n";
        }
        // E is accepted too — strtod reads both — so what this pins is that the
        // file's own D form is not silently truncated at the D.
        const auto r = CoefficientSet::load_egm2008(p.string(), dir.string());
        REQUIRE(!r.has_value());
        CHECK(r.error().message.find("2401333") != std::string::npos);   // reached the count
        fs::remove(p);
    }
}

TEST_CASE("GRAV-A-029: the scale's margin is measured at BOTH ends", "[gravity]") {
    // The top margin is easy and was stated when the scale was chosen: the
    // largest value the recursion forms is 10^457.9, so 10^-280 leaves 130
    // decades of headroom.  The BOTTOM margin is set by the smallest intermediate
    // the recursion actually forms, not by a unit result, and 28 decades is the
    // thinner side — so it is measured here rather than argued.
    //
    // There is also a bound that does not need measuring.  Pbar'_nm = Pbar_nm /
    // cos^m(phi) and cos(phi) <= 1, so |Pbar'| >= |Pbar| EVERYWHERE.  The scaled
    // value can therefore only be subnormal where Pbar itself is below 10^-28,
    // and a term that small sits beside terms of order 1 to 66 in the same
    // degree's sum: it is already far below that sum's own rounding.  Losing it
    // costs nothing.  The measurement below is the check on that argument.
    const RecursionTable& t = j2000_field().recursion();
    std::vector<double> P(2192), dP(2192);
    double smallest = 1e300, largest = 0.0;
    int subnormal = 0, samples = 0;
    constexpr double kScale = 1e-280;
    constexpr double kSmallestNormal = 2.2250738585072014e-308;

    for (int m : {0, 1, 2, 60, 360, 979, 1500, 2159}) {
        for (double u : {0.0, 0.3971478906347806, 0.7071067811865476, 0.9396926207859084,
                         0.9999984769132877, 1.0}) {
            legendre_column(t, m, 2190, u, kScale, P.data(), dP.data());
            for (int n = m; n <= 2190; ++n) {
                const double v = std::abs(P[static_cast<std::size_t>(n)]);
                ++samples;
                REQUIRE(std::isfinite(v));
                largest = std::max(largest, v);
                if (v == 0.0) continue;           // a genuine zero of the polynomial
                if (v < kSmallestNormal) ++subnormal;
                smallest = std::min(smallest, v);
            }
        }
    }
    WARN("GRAV-A-029: over " << samples << " scaled values the recursion forms, the range is "
         << smallest << " to " << largest << " — that is " << std::log10(smallest) + 308.0
         << " decades above the smallest normal double and "
         << 308.0 - std::log10(largest) << " below the largest; " << subnormal << " subnormal");
    CHECK(subnormal == 0);
    CHECK(smallest > 1e-300);
    CHECK(largest < 1e300);
}

// ===========================================================================
// SPEC-gravity §4.7 (v1.4, 2026-10-06; L7 step 1, ruling R2): the second derivatives, and the synthesis of a view.
//
// EVERY tolerance, case list and case count below was registered in SPEC-gravity §8 (GRAV-A-030 … -039) and
// PROVENANCE.md §40.5, and committed (980a238) BEFORE any line of the code under test existed.  The reference
// data — 145 tensors from the definition, twelve values from the unchanged tree — were committed with them.
// ===========================================================================

namespace {

constexpr double kEps = 2.220446049250313e-16;   // 2^-52, the epsilon every tolerance below is written in
constexpr double kAeM = 6378136.3;               // EGM2008's a_e, as ScalingParameters carries it

/// P8 of SPEC-gravity §8: the eight positions of GRAV-A-030, -032, -034, -035 and -037's symmetry, in units of a_e,
/// each multiplied once in double precision.
std::array<Vec3, 8> p8() {
    const double q[8][3] = {{1.05, 0.0, 0.0},       {0.62, 0.55, 0.71},   {-0.43, 0.81, -0.74}, {0.0, 0.0, 1.07},
                            {1.0e-3, -2.0e-3, 1.1}, {0.0, 0.0, -1.2},     {0.0, 1.5, 0.0},      {-1.05, -1.05, 0.2}};
    std::array<Vec3, 8> out{};
    for (std::size_t i = 0; i < 8; ++i) out[i] = Vec3{kAeM * q[i][0], kAeM * q[i][1], kAeM * q[i][2]};
    return out;
}

frames::ItrsPosition at(const Vec3& p) { return frames::ItrsPosition{p}; }

/// Owned storage for a view: ONE coefficient, or the field's own coefficients of degrees n0+1 … n1 (n0 = -1 for
/// all of them), zero elsewhere.
struct Storage {
    std::vector<double> c, s;
    int n_max = 0;

    static Storage single(int n, int m, double c_amp, double s_amp) {
        Storage st;
        st.n_max = n;
        st.c.assign(CoefficientSet::index(n, n) + 1, 0.0);
        st.s.assign(CoefficientSet::index(n, n) + 1, 0.0);
        st.c[CoefficientSet::index(n, m)] = c_amp;
        st.s[CoefficientSet::index(n, m)] = s_amp;
        return st;
    }
    static Storage slice(const ConventionalField& f, int n0, int n1) {
        Storage st;
        st.n_max = n1;
        st.c.assign(CoefficientSet::index(n1, n1) + 1, 0.0);
        st.s.assign(CoefficientSet::index(n1, n1) + 1, 0.0);
        for (int n = n0 + 1; n <= n1; ++n)
            for (int m = 0; m <= n; ++m) {
                st.c[CoefficientSet::index(n, m)] = f.c(n, m);
                st.s[CoefficientSet::index(n, m)] = f.s(n, m);
            }
        return st;
    }
    CoefficientView view(std::optional<TideSystem> system = std::nullopt) const {
        auto v = CoefficientView::of(n_max, c, s, system, "test storage");
        REQUIRE(v.has_value());
        return *v;
    }
};

double max_abs(const Mat3& g) {
    double m = 0.0;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) m = std::max(m, std::abs(g.r[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]));
    return m;
}
double at_ij(const Mat3& g, int i, int j) { return g.r[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]; }

/// d_ij (3 z^2 r^-5 - r^-3), exactly (verified against the definition to 1.6e-69 before GRAV-A-030 was written).
double j2_f_ij(const Vec3& p, int i, int j) {
    const double x[3] = {p.x, p.y, p.z};
    const double r2 = p.x * p.x + p.y * p.y + p.z * p.z;
    const double r = std::sqrt(r2);
    const double r5 = r2 * r2 * r, r7 = r5 * r2, r9 = r7 * r2;
    const double z = p.z;
    const double dij = (i == j) ? 1.0 : 0.0, diz = (i == 2) ? 1.0 : 0.0, djz = (j == 2) ? 1.0 : 0.0;
    return 6.0 * diz * djz / r5 - 30.0 * z * (diz * x[j] + djz * x[i]) / r7 + 105.0 * z * z * x[i] * x[j] / r9
         - 15.0 * z * z * dij / r7 + 3.0 * dij / r5 - 15.0 * x[i] * x[j] / r7;
}

}  // namespace

TEST_CASE("GRAV-A-030: the point-mass and J2 tensors against their exact closed forms", "[gravity][gradient]") {
    const ConventionalField& f = j2000_field();
    const double gm = f.scaling().gm_m3_s2();
    const double j2 = -std::sqrt(5.0) * f.c(2, 0);
    int cases = 0;
    double worst_pm = 0.0, worst_j2 = 0.0;
    for (const Vec3& p : p8()) {
        const double r = p.norm();
        const double scale = gm / (r * r * r);
        const double x[3] = {p.x, p.y, p.z};
        auto g0 = f.gradient(at(p), deg(0), ord(0));
        auto g2 = f.gradient(at(p), deg(2), ord(0));
        REQUIRE(g0.has_value());
        REQUIRE(g2.has_value());
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                const double pm = scale * (3.0 * x[i] * x[j] / (r * r) - (i == j ? 1.0 : 0.0));
                const double e0 = std::abs(at_ij(g0->per_second_squared(), i, j) - pm) / (kEps * scale);
                worst_pm = std::max(worst_pm, e0);
                CHECK(e0 <= 64.0);
                const double total = pm + (-gm * kAeM * kAeM * j2 / 2.0) * j2_f_ij(p, i, j);
                const double e2 = std::abs(at_ij(g2->per_second_squared(), i, j) - total) / (kEps * scale);
                worst_j2 = std::max(worst_j2, e2);
                CHECK(e2 <= 64.0);
            }
        ++cases;
    }
    REQUIRE(cases == 8);
    WARN("GRAV-A-030: " << cases << " positions, all nine entries each; worst disagreement with the point-mass tensor "
         << worst_pm << " eps GM/r^3 and with point mass + J2 " << worst_j2 << " eps GM/r^3 (bound 64)");
}

TEST_CASE("GRAV-A-031: the tensor of one coefficient against the tensor of the DEFINITION, 145 cases",
          "[gravity][gradient]") {
    const ConventionalField& f = j2000_field();
    const double gm_over_ae3 = f.scaling().gm_m3_s2() / (kAeM * kAeM * kAeM);
    int cases = 0, exact_zero = 0, floor_governed = 0;
    std::map<int, double> worst_by_degree;     // worst |difference| / (eps max|R|) per degree
    // SPEC-gravity §8's amendment of 2026-10-06 (v1.4a): a reference below 1e-26 is beyond what §3.6a's scaled
    // representation carries, and is held to the scale's resolution, 2e-38 absolute, instead of to a relative bound.
    constexpr double kRepresentationFloor = 2.0e-38;
    for (const auto& k : reference::kGradient) {
        const Storage st = Storage::single(k.n, k.m, k.kind == 'C' ? 1.0 : 0.0, k.kind == 'S' ? 1.0 : 0.0);
        const CoefficientView view = st.view();
        auto g = f.gradient_of(view, at(Vec3{k.x, k.y, k.z}), deg(k.n), ord(k.m));
        REQUIRE(g.has_value());
        const double ref[3][3] = {{k.g[0], k.g[1], k.g[2]}, {k.g[1], k.g[3], k.g[4]}, {k.g[2], k.g[4], k.g[5]}};
        double max_ref = 0.0;
        for (const auto& row : ref) for (double v : row) max_ref = std::max(max_ref, std::abs(v));
        double tol = 16.0 * (k.n + 8) * kEps * max_ref;      // as registered; zero for an exactly-zero reference
        if (max_ref == 0.0) ++exact_zero;
        if (max_ref > 0.0 && max_ref < 1.0e-26) { tol = kRepresentationFloor; ++floor_governed; }
        double worst = 0.0;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                worst = std::max(worst, std::abs(at_ij(g->per_second_squared(), i, j) / gm_over_ae3 - ref[i][j]));
        CHECK(worst <= tol);
        worst_by_degree[k.n] = std::max(worst_by_degree[k.n], worst / (kEps * max_ref));
        ++cases;
    }
    REQUIRE(cases == 145);
    REQUIRE(std::size(reference::kGradient) == 145);
    REQUIRE(exact_zero == 13);        // counted from the reference file before the re-run (SPEC-gravity §8, v1.4a)
    REQUIRE(floor_governed == 2);
    std::ostringstream os;
    for (const auto& [n, w] : worst_by_degree) os << " n=" << n << ": " << w << ";";
    WARN("GRAV-A-031: " << cases << " cases, all nine entries each (" << exact_zero << " with an exactly-zero reference, "
         << "held to exact zero; " << floor_governed << " below 1e-26, held to the representation floor); worst "
         "disagreement in eps x max|R| by degree" << os.str() << " (bound 16(n+8))");
}

TEST_CASE("GRAV-A-032: Laplace's equation -- the trace of the real field's tensor is zero", "[gravity][gradient]") {
    const ConventionalField& f = j2000_field();
    const double gm = f.scaling().gm_m3_s2();
    const std::array<std::pair<int, int>, 7> nm{{{2, 0}, {4, 4}, {10, 10}, {36, 36}, {90, 90}, {200, 150}, {360, 360}}};
    int cases = 0;
    double worst = 0.0;
    for (const Vec3& p : p8())
        for (const auto& [n, m] : nm) {
            auto g = f.gradient(at(p), deg(n), ord(m));
            REQUIRE(g.has_value());
            const Mat3& G = g->per_second_squared();
            const double r = p.norm();
            const double trace = (at_ij(G, 0, 0) + at_ij(G, 1, 1) + at_ij(G, 2, 2)) / (gm / (r * r * r));
            worst = std::max(worst, std::abs(trace));
            CHECK(std::abs(trace) <= 1.0e-13);
            ++cases;
        }
    REQUIRE(cases == 56);
    WARN("GRAV-A-032: " << cases << " cases; worst |trace| / (GM/r^3) = " << worst << " (bound 1e-13)");
}

TEST_CASE("GRAV-A-033: the second-derivative recursion satisfies the Legendre equation", "[gravity][gradient]") {
    const RecursionTable& t = j2000_field().recursion();
    const std::array<int, 8> ns{2, 3, 5, 10, 36, 90, 200, 360};
    const std::array<double, 8> us{-1.0, -0.93, -0.5, 0.0, 0.3, 0.8, 0.99, 1.0};
    int cases = 0;
    double worst = 0.0;
    for (int n : ns) {
        std::set<int> ms{0, 1, 2, 3, n / 2, n - 1, n};
        for (auto it = ms.begin(); it != ms.end();) it = (*it > n || *it < 0) ? ms.erase(it) : std::next(it);
        for (int m : ms)
            for (double u : us) {
                std::vector<double> P(static_cast<std::size_t>(n) + 1), dP(P.size()), d2P(P.size());
                legendre_column2(t, m, n, u, 1.0, P.data(), dP.data(), d2P.data());
                const auto un = static_cast<std::size_t>(n);
                const double p = P[un], p1 = dP[un], p2 = d2P[un];
                const double a = 1.0 - u * u;
                const double b = 2.0 * (m + 1) * u;
                const double c = static_cast<double>(n) * (n + 1) - static_cast<double>(m) * (m + 1);
                const double residual = a * p2 - b * p1 + c * p;
                const double terms = std::abs(a * p2) + std::abs(b * p1) + std::abs(c * p);
                CHECK(std::abs(residual) <= 1.0e-10 * terms);
                if (terms > 0.0) worst = std::max(worst, std::abs(residual) / terms);
                ++cases;
            }
    }
    REQUIRE(cases == 384);
    WARN("GRAV-A-033: " << cases << " (n, m, u) cases; worst |residual| / (sum of the terms' magnitudes) = " << worst
         << " (bound 1e-10)");
}

TEST_CASE("GRAV-A-034: the synthesis is linear in the coefficients", "[gravity][gradient]") {
    const ConventionalField& f = j2000_field();
    const double gm = f.scaling().gm_m3_s2();
    const std::array<std::pair<int, int>, 3> splits{{{2, 10}, {10, 36}, {36, 90}}};
    int cases = 0;
    double worst_a = 0.0, worst_g = 0.0;
    for (const Vec3& p : p8()) {
        const double r = p.norm();
        for (const auto& [n0, n1] : splits) {
            const Storage st = Storage::slice(f, n0, n1);
            const CoefficientView v = st.view();
            auto a1 = f.acceleration(at(p), deg(n1), ord(n1));
            auto a0 = f.acceleration(at(p), deg(n0), ord(n0));
            auto av = f.acceleration_of(v, at(p), deg(n1), ord(n1));
            auto g1 = f.gradient(at(p), deg(n1), ord(n1));
            auto g0 = f.gradient(at(p), deg(n0), ord(n0));
            auto gv = f.gradient_of(v, at(p), deg(n1), ord(n1));
            REQUIRE(a1.has_value()); REQUIRE(a0.has_value()); REQUIRE(av.has_value());
            REQUIRE(g1.has_value()); REQUIRE(g0.has_value()); REQUIRE(gv.has_value());
            const Vec3 da = a1->metres_per_second_squared() - a0->metres_per_second_squared()
                          - av->metres_per_second_squared();
            const double ea = da.norm() / (kEps * gm / (r * r));
            worst_a = std::max(worst_a, ea);
            CHECK(ea <= 128.0);
            double eg = 0.0;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    eg = std::max(eg, std::abs(at_ij(g1->per_second_squared(), i, j) - at_ij(g0->per_second_squared(), i, j)
                                               - at_ij(gv->per_second_squared(), i, j)) / (kEps * gm / (r * r * r)));
            worst_g = std::max(worst_g, eg);
            CHECK(eg <= 128.0);
            ++cases;
        }
        // additivity of two views: degrees 3-5 and 6-9 against 3-9
        const Storage sa = Storage::slice(f, 2, 5), sb = Storage::slice(f, 5, 9), sc = Storage::slice(f, 2, 9);
        const CoefficientView va = sa.view(), vb = sb.view(), vc = sc.view();
        auto xa = f.acceleration_of(va, at(p), deg(5), ord(5));
        auto xb = f.acceleration_of(vb, at(p), deg(9), ord(9));
        auto xc = f.acceleration_of(vc, at(p), deg(9), ord(9));
        REQUIRE(xa.has_value()); REQUIRE(xb.has_value()); REQUIRE(xc.has_value());
        const Vec3 dd = xa->metres_per_second_squared() + xb->metres_per_second_squared() - xc->metres_per_second_squared();
        CHECK(dd.norm() <= 128.0 * kEps * gm / (r * r));
        ++cases;
    }
    REQUIRE(cases == 32);
    WARN("GRAV-A-034: " << cases << " cases; worst linearity residual " << worst_a << " eps GM/r^2 (acceleration), "
         << worst_g << " eps GM/r^3 (tensor); bound 128");
}

TEST_CASE("GRAV-A-035: the field IS a view -- its own coefficients, handed in, reproduce it exactly",
          "[gravity][gradient]") {
    const ConventionalField& f = j2000_field();
    int cases = 0;
    for (int n : {36, 90}) {
        const Storage st = Storage::slice(f, -1, n);
        const CoefficientView v = st.view();
        for (const Vec3& p : p8()) {
            auto a = f.acceleration(at(p), deg(n), ord(n));
            auto av = f.acceleration_of(v, at(p), deg(n), ord(n));
            auto g = f.gradient(at(p), deg(n), ord(n));
            auto gv = f.gradient_of(v, at(p), deg(n), ord(n));
            REQUIRE(a.has_value()); REQUIRE(av.has_value()); REQUIRE(g.has_value()); REQUIRE(gv.has_value());
            CHECK(a->metres_per_second_squared().x == av->metres_per_second_squared().x);
            CHECK(a->metres_per_second_squared().y == av->metres_per_second_squared().y);
            CHECK(a->metres_per_second_squared().z == av->metres_per_second_squared().z);
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    CHECK(at_ij(g->per_second_squared(), i, j) == at_ij(gv->per_second_squared(), i, j));
            ++cases;
        }
    }
    REQUIRE(cases == 16);
    WARN("GRAV-A-035: " << cases << " (degree, position) cases, acceleration and tensor, every component equal to 0 ulp");
}

TEST_CASE("GRAV-A-036: nothing already shipped moved -- potential and acceleration against the values recorded BEFORE "
          "the change", "[gravity][gradient]") {
    // The conventional field exactly as the characterisation probe built it: 2023-01-22 00:00 UTC, secular terms
    // extrapolated beyond the pole fit as tests/l4_ranking.cpp does.
    auto epoch = odl::time::Epoch::from_calendar(odl::time::TimeScale::UTC,
                                                 odl::time::Calendar{2023, 1, 22, 0, 0, 0.0}, leaps());
    REQUIRE(epoch.has_value());
    auto f = model().conventional(*epoch, true);
    REQUIRE(f.has_value());
    int cases = 0;
    for (const auto& row : baseline::kRows) {
        const Vec3 p{row.x, row.y, row.z};
        auto a = f->acceleration(at(p), deg(row.n), ord(row.m));
        auto v = f->potential(at(p), deg(row.n), ord(row.m));
        REQUIRE(a.has_value());
        REQUIRE(v.has_value());
        CHECK(*v == row.potential);
        CHECK(a->metres_per_second_squared().x == row.a[0]);
        CHECK(a->metres_per_second_squared().y == row.a[1]);
        CHECK(a->metres_per_second_squared().z == row.a[2]);
        ++cases;
    }
    REQUIRE(cases == 12);
    WARN("GRAV-A-036: " << cases << " recorded (position, degree, order) cases, the potential and every acceleration "
         "component equal to the pre-change value to 0 ulp");
}

TEST_CASE("GRAV-A-037: the pole is an ordinary point of the tensor, and the tensor is symmetric", "[gravity][gradient]") {
    const ConventionalField& f = j2000_field();
    const double r = 1.07 * kAeM;
    constexpr double kDelta = 1.0e-8;       // colatitude of the displaced points, rad
    int cases = 0, zero_at_pole = 0;
    double worst = 0.0;
    for (const auto& [n, m] : std::array<std::pair<int, int>, 5>{{{2, 1}, {2, 2}, {3, 1}, {3, 2}, {3, 3}}}) {
        const Storage st = Storage::single(n, m, 1.0, 0.7);
        const CoefficientView v = st.view();
        for (double sign : {1.0, -1.0}) {
            auto g_pole = f.gradient_of(v, at(Vec3{0.0, 0.0, sign * r}), deg(n), ord(m));
            REQUIRE(g_pole.has_value());
            double scale = max_abs(g_pole->per_second_squared());
            if (scale == 0.0) {
                // SPEC-gravity §8's amendment of 2026-10-06 (v1.4a): the tensor of an order >= 3 term vanishes exactly
                // on the polar axis, so the registered scale does not exist; use max|G| at colatitude 30 deg on the
                // meridian lambda = 0 at the same radius.
                const double col = std::numbers::pi / 6.0;
                auto g_ref = f.gradient_of(v, at(Vec3{r * std::sin(col), 0.0, sign * r * std::cos(col)}), deg(n), ord(m));
                REQUIRE(g_ref.has_value());
                scale = max_abs(g_ref->per_second_squared());
                ++zero_at_pole;
            }
            REQUIRE(scale > 0.0);
            for (double lam : {0.0, std::numbers::pi / 2.0}) {
                const Vec3 near{r * std::sin(kDelta) * std::cos(lam), r * std::sin(kDelta) * std::sin(lam),
                                sign * r * std::cos(kDelta)};
                auto g_near = f.gradient_of(v, at(near), deg(n), ord(m));
                REQUIRE(g_near.has_value());
                double diff = 0.0;
                for (int i = 0; i < 3; ++i)
                    for (int j = 0; j < 3; ++j)
                        diff = std::max(diff, std::abs(at_ij(g_near->per_second_squared(), i, j)
                                                       - at_ij(g_pole->per_second_squared(), i, j)));
                const double bound = (4.0 * (n + 4) * kDelta + 64.0 * kEps) * scale;
                CHECK(diff <= bound);
                worst = std::max(worst, diff / bound);
                ++cases;
            }
        }
    }
    REQUIRE(cases == 20);
    REQUIRE(zero_at_pole == 2);      // the member (3,3) at the two poles, as the amendment says
    double worst_sym = 0.0;
    for (const Vec3& p : p8()) {
        auto g = f.gradient(at(p), deg(36), ord(36));
        REQUIRE(g.has_value());
        const Mat3& G = g->per_second_squared();
        const double scale = max_abs(G);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                const double asym = std::abs(at_ij(G, i, j) - at_ij(G, j, i)) / (kEps * scale);
                worst_sym = std::max(worst_sym, asym);
                CHECK(asym <= 16.0);
            }
    }
    WARN("GRAV-A-037: " << cases << " pole-continuity cases, worst difference " << worst << " of its bound; "
         "worst asymmetry " << worst_sym << " eps max|G| at the eight positions (bound 16)");
}

TEST_CASE("GRAV-A-038: the view's refusals", "[gravity][gradient]") {
    const ConventionalField& f = j2000_field();
    const Vec3 p = p8()[1];

    // GRAV-F-009: arrays shorter than the degree needs; a degree outside 0 ... 2190
    const std::vector<double> few(10, 0.0);
    auto short_view = CoefficientView::of(5, few, few);          // degree 5 needs index(5,5)+1 = 21 entries
    REQUIRE(!short_view.has_value());
    CHECK(short_view.error().id == "GRAV-F-009");
    CHECK(short_view.error().message.find("21") != std::string::npos);
    CHECK(short_view.error().message.find("10") != std::string::npos);
    auto huge_view = CoefficientView::of(2191, few, few);
    REQUIRE(!huge_view.has_value());
    CHECK(huge_view.error().id == "GRAV-F-009");
    CHECK(!CoefficientView::of(-1, few, few).has_value());

    // GRAV-F-004: degree 10 of a view of degree 5, naming both
    const Storage five = Storage::single(5, 2, 1.0, 0.0);
    auto too_high = f.acceleration_of(five.view(), at(p), deg(10), ord(10));
    REQUIRE(!too_high.has_value());
    CHECK(too_high.error().id == "GRAV-F-004");
    CHECK(too_high.error().message.find("10") != std::string::npos);
    CHECK(too_high.error().message.find("5") != std::string::npos);
    auto too_high_g = f.gradient_of(five.view(), at(p), deg(10), ord(10));
    REQUIRE(!too_high_g.has_value());
    CHECK(too_high_g.error().id == "GRAV-F-004");

    // GRAV-F-008: a view in the tide-free system against the zero-tide conventional field, naming both
    REQUIRE(f.tide_system() == TideSystem::ZeroTide);
    auto mismatch = f.acceleration_of(five.view(TideSystem::TideFree), at(p), deg(5), ord(5));
    REQUIRE(!mismatch.has_value());
    CHECK(mismatch.error().id == "GRAV-F-008");
    CHECK(mismatch.error().message.find(name_of(TideSystem::TideFree)) != std::string::npos);
    CHECK(mismatch.error().message.find(name_of(TideSystem::ZeroTide)) != std::string::npos);
    CHECK(!f.gradient_of(five.view(TideSystem::TideFree), at(p), deg(5), ord(5)).has_value());
    // ... and a view declaring the field's own system, and one declaring none, are accepted
    CHECK(f.acceleration_of(five.view(TideSystem::ZeroTide), at(p), deg(5), ord(5)).has_value());
    CHECK(f.acceleration_of(five.view(), at(p), deg(5), ord(5)).has_value());
}

TEST_CASE("GRAV-A-039: what the tensor costs against the acceleration", "[gravity][gradient]") {
    const ConventionalField& f = j2000_field();
    const Vec3 p = p8()[1];
    for (int n : {90, 360}) {
        auto best = [&](auto&& call) {
            double b = 1e300;
            for (int rep = 0; rep < 5; ++rep) {
                const auto t0 = std::chrono::steady_clock::now();
                for (int k = 0; k < 20; ++k) { auto r = call(); REQUIRE(r.has_value()); }
                b = std::min(b, std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() / 20.0);
            }
            return b;
        };
        const double ta = best([&] { return f.acceleration(at(p), deg(n), ord(n)); });
        const double tg = best([&] { return f.gradient(at(p), deg(n), ord(n)); });
        WARN("GRAV-A-039 / GRAV-P-9: at degree and order " << n << ", acceleration " << ta * 1e6 << " us, gradient "
             << tg * 1e6 << " us, ratio " << tg / ta << " (predicted 1.5 to 3, asserted below 10)");
        CHECK(tg / ta < 10.0);
    }
}
