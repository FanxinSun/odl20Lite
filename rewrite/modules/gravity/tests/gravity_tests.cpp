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

#include "legendre_reference.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numbers>
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
