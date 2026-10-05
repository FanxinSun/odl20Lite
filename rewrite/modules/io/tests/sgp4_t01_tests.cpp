// sgp4_t01_tests.cpp — SPEC-io-sgp4.md §9.3, IOSG-R-012: T-01's required-disagreement gate.
//
// THE ORDER OF WORK IS THE POINT (plan/subplan_L6/L6-3.md: "T-01's band is fixed before the
// comparison runs"). The element set and the two Horizons tables were vendored first
// (PROVENANCE.md §38.11, data only); this file's FIRST test case, IOSG-A-010, is the PREDICTION,
// committed before either table is compared with anything. It computes, from the element set, the EOP
// and leap-second pins and a time grid ALONE, what a legacy IAU-76/80 chain would differ from this
// tree's TEME -> GCRS by, in three readings of what that chain does with the IERS celestial-pole
// offsets, and it freezes the numbers. It does not -- cannot, the build gives it no path -- open either
// table. The comparison (IOSG-A-011) is a later commit and reads the numbers this one froze.
//
// WHAT IS COMPARED. For each epoch of the grid, the difference between a legacy chain's J2000 position
// and this tree's GCRS position, d_k = r_legacy,k - r_L1,k, is fitted by a small rotation plus a time
// shift:  d_k = Omega x r_k + tau * v_k   (Omega: three components in GCRS axes, tau: seconds).
// A rotation vector says the size AND the direction of a frame difference at once, independent of
// where the orbit happens to be; tau absorbs the one thing that is not a frame difference at all, a
// small disagreement about the epoch.

#include <odl/core/vec3.hpp>
#include <odl/eop/series.hpp>
#include <odl/frames/transform.hpp>
#include <odl/io/sgp4.hpp>
#include <odl/io/tle.hpp>
#include <odl/time/epoch.hpp>
#include <odl/time/leap_table.hpp>

extern "C" {
#include <erfa.h>
}

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using odl::Vec3;
using odl::time::Epoch;
using odl::time::TimeScale;

constexpr double kPi = 3.14159265358979323846;
constexpr double kArcsec = kPi / (180.0 * 3600.0);
constexpr double kMas = kArcsec / 1000.0;      // radians per milliarcsecond
constexpr int kEpochs = 61;                    // 07:30 to 12:30 TDB in 5-minute steps
constexpr double kDocumentedOriginMas = 53.0;    // Horizons' manual: the origin of RA of its intermediate frame differs by about -53 mas

std::string slurp(const char* path) {
    std::ifstream f(path, std::ios::binary);
    REQUIRE(f.good());
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

const odl::time::LeapTable& leaps() {
    static const odl::time::LeapTable t = [] {
        auto r = odl::time::LeapTable::parse(slurp(ODL_LEAP_SECOND_FILE), {"iers-leap-seconds", "", ""});
        REQUIRE(r.has_value());
        return *r;
    }();
    return t;
}

const odl::eop::EopSeries& finals() {
    static const odl::eop::EopSeries s = [] {
        auto r = odl::eop::EopSeries::load_finals2000a(slurp(ODL_FINALS2000A_FILE),
                                                       odl::eop::EopProvenance{"eop-finals2000a", "", "", ""}, leaps());
        if (!r.has_value()) FAIL("finals2000A: " << r.error().message);
        return *r;
    }();
    return s;
}

odl::io::Tle t01_tle() {
    std::istringstream in(slurp(ODL_T01_TLE_FILE));
    std::string name, l1, l2;
    std::getline(in, name);
    std::getline(in, l1);
    std::getline(in, l2);
    for (std::string* l : {&l1, &l2})
        while (!l->empty() && (l->back() == '\r' || l->back() == ' ')) l->pop_back();
    auto t = odl::io::read_tle(l1, l2);
    if (!t.has_value()) FAIL("TLE: " << t.error().message);
    return *t;
}

/// The k-th epoch of the capture's grid: 2026-10-05 07:30:00 TDB + 5k minutes.
odl::time::Calendar grid_calendar(int k) {
    return odl::time::Calendar{2026, 10, 5, 7 + (30 + 5 * k) / 60, (30 + 5 * k) % 60, 0.0};
}

Epoch grid_epoch(int k) {
    auto e = Epoch::from_calendar(TimeScale::TDB, grid_calendar(k), leaps());
    REQUIRE(e.has_value());
    return *e;
}

/// d_k = Omega x r_k + tau v_k, by linear least squares over every epoch.
struct Fit {
    double omega[3] = {0.0, 0.0, 0.0};   // rad, GCRS axes
    double tau = 0.0;                    // s
    double rms_m = 0.0;                  // residual after the fit, metres
    double mean_m = 0.0, max_m = 0.0;    // |d_k| itself, metres
};

Fit fit_rotation_and_shift(const std::vector<Vec3>& r, const std::vector<Vec3>& v, const std::vector<Vec3>& d) {
    double n[4][5] = {};
    for (std::size_t k = 0; k < r.size(); ++k) {
        const double rr[3] = {r[k].x, r[k].y, r[k].z};
        const double col[4][3] = {{0.0, -rr[2], rr[1]}, {rr[2], 0.0, -rr[0]}, {-rr[1], rr[0], 0.0},
                                  {v[k].x, v[k].y, v[k].z}};
        const double dd[3] = {d[k].x, d[k].y, d[k].z};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j)
                for (int c = 0; c < 3; ++c) n[i][j] += col[i][c] * col[j][c];
            for (int c = 0; c < 3; ++c) n[i][4] += col[i][c] * dd[c];
        }
    }
    for (int c = 0; c < 4; ++c) {                                 // Gauss-Jordan with partial pivoting
        int p = c;
        for (int rr = c + 1; rr < 4; ++rr)
            if (std::fabs(n[rr][c]) > std::fabs(n[p][c])) p = rr;
        for (int k = 0; k < 5; ++k) std::swap(n[c][k], n[p][k]);
        for (int rr = 0; rr < 4; ++rr) {
            if (rr == c) continue;
            const double f = n[rr][c] / n[c][c];
            for (int k = c; k < 5; ++k) n[rr][k] -= f * n[c][k];
        }
    }
    Fit f;
    for (int i = 0; i < 3; ++i) f.omega[i] = n[i][4] / n[i][i];
    f.tau = n[3][4] / n[3][3];
    double ss = 0.0, sm = 0.0;
    for (std::size_t k = 0; k < r.size(); ++k) {
        const double rr[3] = {r[k].x, r[k].y, r[k].z};
        const double model[3] = {f.omega[1] * rr[2] - f.omega[2] * rr[1] + f.tau * v[k].x,
                                 f.omega[2] * rr[0] - f.omega[0] * rr[2] + f.tau * v[k].y,
                                 f.omega[0] * rr[1] - f.omega[1] * rr[0] + f.tau * v[k].z};
        const double e0 = d[k].x - model[0], e1 = d[k].y - model[1], e2 = d[k].z - model[2];
        ss += e0 * e0 + e1 * e1 + e2 * e2;
        const double dn = d[k].norm();
        sm += dn;
        f.max_m = std::max(f.max_m, dn * 1000.0);
    }
    const double count = static_cast<double>(r.size());
    f.rms_m = std::sqrt(ss / count) * 1000.0;
    f.mean_m = sm / count * 1000.0;
    return f;
}

/// The prediction: this tree's chain and three readings of the legacy chain, at every epoch of the grid.
/// Nothing here reads a Horizons table.
struct Prediction {
    std::vector<Vec3> r_l1, v_l1;                // this tree, GCRS, km and km/s
    std::vector<Vec3> d_a, d_a_kin, d_b, d_c;    // legacy minus this tree, km
    std::vector<Vec3> d_d;                       // D: the manual's quoted RA-origin offset applied as a rotation about the pole, km
    double dpsi_mas = 0.0, deps_mas = 0.0;       // the offsets solved at the first epoch
    double kin_mas = 0.0;                        // the kinematic equation-of-equinoxes terms at the first epoch
    double mean_radius_km = 0.0;
};

Prediction predict() {
    const odl::io::Tle tle = t01_tle();
    odl::eop::EopPolicy policy;
    policy.max_quality = odl::eop::Quality::Predicted;   // the epochs lie past finals2000A's observed data: predictions

    Prediction p;
    double radius_sum = 0.0;
    for (int k = 0; k < kEpochs; ++k) {
        const Epoch ep = grid_epoch(k);
        const auto teme = odl::io::propagate_teme(tle, ep, leaps());
        REQUIRE(teme.has_value());
        const auto rec = finals().at(ep, policy);
        REQUIRE(rec.has_value());
        const auto gcrs = odl::frames::to_gcrs(*teme, *rec, leaps());
        REQUIRE(gcrs.has_value());
        const Vec3 r_l1 = gcrs->position();
        radius_sum += r_l1.norm();

        // --- the legacy chains, from ERFA: r_J2000 = P^T N^T R3(-Eqe) r_TEME (VAL06 Rev 3 Appendix C, (C-3))
        const auto tt = ep.two_part_jd(TimeScale::TT, leaps());
        REQUIRE(tt.has_value());
        const double t1 = tt->day, t2 = tt->fraction;
        double rp[3][3], rpt[3][3];
        eraPmat76(t1, t2, rp);                                // IAU 1976 precession, J2000 -> mean of date
        eraTr(rp, rpt);
        double dpsi80 = 0.0, deps80 = 0.0;
        eraNut80(t1, t2, &dpsi80, &deps80);                   // IAU 1980 nutation (106 terms)
        const double epsa = eraObl80(t1, t2);                 // mean obliquity
        // THE OBSERVED POLE: this tree's CIP, X and Y from IAU 2006/2000A plus the EOP series' dX, dY
        double cip_x = 0.0, cip_y = 0.0;
        eraXy06(t1, t2, &cip_x, &cip_y);
        cip_x += rec->dx;
        cip_y += rec->dy;
        const auto pole = [&](double d_psi, double d_eps, double out[2]) {
            double rn[3][3], rnt[3][3];
            eraNumat(epsa, dpsi80 + d_psi, deps80 + d_eps, rn);
            eraTr(rn, rnt);
            double z[3] = {0.0, 0.0, 1.0}, a[3], b[3];
            eraRxp(rnt, z, a);
            eraRxp(rpt, a, b);
            out[0] = b[0];
            out[1] = b[1];
        };
        // the offsets (dpsi, deps) that put the legacy true pole on the observed one: "IAU76/80 precession-
        // nutation theory, corrected daily by GPS measurements" (Horizons' manual, PROVENANCE.md §38.12)
        double dp = 0.0, de = 0.0;
        for (int it = 0; it < 6; ++it) {
            double f0[2], f1[2], f2[2];
            pole(dp, de, f0);
            pole(dp + 1.0e-9, de, f1);
            pole(dp, de + 1.0e-9, f2);
            const double a11 = (f1[0] - f0[0]) / 1.0e-9, a21 = (f1[1] - f0[1]) / 1.0e-9;
            const double a12 = (f2[0] - f0[0]) / 1.0e-9, a22 = (f2[1] - f0[1]) / 1.0e-9;
            const double e0 = cip_x - f0[0], e1 = cip_y - f0[1], det = a11 * a22 - a12 * a21;
            dp += (a22 * e0 - a12 * e1) / det;
            de += (-a21 * e0 + a11 * e1) / det;
        }
        const double om_moon = eraFaom03(((t1 - 2451545.0) + t2) / 36525.0);
        const double kin = (0.00264 * std::sin(om_moon) + 0.000063 * std::sin(2.0 * om_moon)) * kArcsec;
        if (k == 0) {
            p.dpsi_mas = dp / kMas;
            p.deps_mas = de / kMas;
            p.kin_mas = kin / kMas;
        }
        double r_teme[3] = {teme->position().x, teme->position().y, teme->position().z};
        const auto legacy = [&](double off_psi, double off_eps, double eqe_extra) {
            const double dpsi = dpsi80 + off_psi, deps = deps80 + off_eps;
            double rn[3][3], rnt[3][3], rz[3][3];
            eraNumat(epsa, dpsi, deps, rn);
            eraTr(rn, rnt);
            eraIr(rz);
            eraRz(-(dpsi * std::cos(epsa) + eqe_extra), rz);
            double tod[3], mod[3], j2000[3];
            eraRxp(rz, r_teme, tod);
            eraRxp(rnt, tod, mod);
            eraRxp(rpt, mod, j2000);
            return Vec3{j2000[0], j2000[1], j2000[2]};
        };
        p.r_l1.push_back(r_l1);
        p.v_l1.push_back(gcrs->velocity());
        // A: pole offsets in the nutation AND in the equation of the equinoxes (Vallado's / Rev 3's convention)
        p.d_a.push_back(legacy(dp, de, 0.0) - r_l1);
        //    A with the kinematic equation-of-equinoxes terms in TEME -> TOD as well (the convention-variant band term)
        p.d_a_kin.push_back(legacy(dp, de, kin) - r_l1);
        // B: no offsets at all
        p.d_b.push_back(legacy(0.0, 0.0, 0.0) - r_l1);
        // C: offsets in the nutation matrix, the equation of the equinoxes from the model nutation only
        p.d_c.push_back(legacy(dp, de, -dp * std::cos(epsa)) - r_l1);
        // D: not a chain. The manual's own number taken at face value: Horizons' intermediate frame's RA origin is
        // 53 mas WEST of IAU 2006/2000A's, so the same components land 53 mas west in the sky: Omega = (0, 0, -53 mas),
        // d = Omega x r.
        const double omega_d = -kDocumentedOriginMas * kMas;     // rad, about +z; negative = westward
        p.d_d.push_back(Vec3{-omega_d * r_l1.y, omega_d * r_l1.x, 0.0});
    }
    p.mean_radius_km = radius_sum / static_cast<double>(kEpochs);
    return p;
}

}  // namespace

// ----------------------------------------------------------------------------------------------------------
// IOSG-A-010: THE PREDICTION. No Horizons table is opened (the build supplies no path to one).
// ----------------------------------------------------------------------------------------------------------
TEST_CASE("IOSG-A-010: T-01's prediction, frozen BEFORE either Horizons table is compared with anything",
          "[sgp4][t01][prediction]") {
    const Prediction p = predict();
    const Fit a = fit_rotation_and_shift(p.r_l1, p.v_l1, p.d_a);
    const Fit a_kin = fit_rotation_and_shift(p.r_l1, p.v_l1, p.d_a_kin);
    const Fit b = fit_rotation_and_shift(p.r_l1, p.v_l1, p.d_b);
    const Fit c = fit_rotation_and_shift(p.r_l1, p.v_l1, p.d_c);
    const Fit d = fit_rotation_and_shift(p.r_l1, p.v_l1, p.d_d);
    const auto show = [](const char* name, const Fit& f) {
        std::ostringstream o;
        o << name << ": Omega = (" << f.omega[0] / kMas << ", " << f.omega[1] / kMas << ", " << f.omega[2] / kMas
          << ") mas, tau " << f.tau * 1e6 << " us, |d| mean " << f.mean_m << " m max " << f.max_m << " m, fit rms "
          << f.rms_m << " m";
        return o.str();
    };
    INFO(show("A", a));
    INFO(show("A'", a_kin));
    INFO(show("B", b));
    INFO(show("C", c));
    INFO(show("D", d));
    INFO("solved offsets at the first epoch: dpsi " << p.dpsi_mas << " mas, deps " << p.deps_mas << " mas; kinematic term "
         << p.kin_mas << " mas; mean |r| " << p.mean_radius_km << " km");

    const auto near = [](double value, double frozen, double tol) { return std::fabs(value - frozen) <= tol; };
    constexpr double kTolMas = 0.05;     // reproducibility of the frozen rotation components, mas

    // the inputs the prediction rests on
    CHECK(near(p.mean_radius_km, 7279.4, 0.5));
    CHECK(near(p.dpsi_mas, -123.20, 0.05));
    CHECK(near(p.deps_mas, -10.89, 0.05));
    CHECK(near(p.kin_mas, -1.476, 0.005));

    // CASE A -- Horizons' chain puts the pole on the observed pole in the nutation AND in the equation of the
    // equinoxes: the legacy chain and this tree agree. Omega = (0, 0, ~0); with the kinematic terms in
    // TEME -> TOD as well, Omega_z = the kinematic term.
    CHECK(near(a.omega[0] / kMas, 0.0, kTolMas));
    CHECK(near(a.omega[1] / kMas, 0.0, kTolMas));
    CHECK(near(a.omega[2] / kMas, -0.03, kTolMas));
    CHECK(a.max_m < 0.005);
    CHECK(near(a_kin.omega[2] / kMas, -1.51, kTolMas));
    CHECK(near(a_kin.mean_m, 0.034, 0.003));
    CHECK(near(a_kin.max_m, 0.053, 0.003));

    // CASE B -- no pole offsets at all: a TILT of the pole, an equatorial axis, 50 mas
    CHECK(near(b.omega[0] / kMas, -10.60, kTolMas));
    CHECK(near(b.omega[1] / kMas, 49.03, kTolMas));
    CHECK(near(b.omega[2] / kMas, 0.00, kTolMas));
    CHECK(near(b.mean_m, 1.716, 0.005));
    CHECK(near(b.max_m, 1.774, 0.005));

    // CASE C -- offsets in the nutation matrix, the equation of the equinoxes from the model alone: a rotation
    // ABOUT THE POLE of 113 mas, positive (the legacy position is turned eastward of this tree's)
    CHECK(near(c.omega[0] / kMas, 0.30, kTolMas));
    CHECK(near(c.omega[1] / kMas, 0.01, kTolMas));
    CHECK(near(c.omega[2] / kMas, 112.98, kTolMas));
    CHECK(near(c.mean_m, 2.580, 0.005));
    CHECK(near(c.max_m, 4.001, 0.005));

    // CASE D -- the manual's own number at face value (no chain): a rotation about the pole, WESTWARD, 53 mas
    CHECK(near(d.omega[0] / kMas, 0.0, kTolMas));
    CHECK(near(d.omega[1] / kMas, 0.0, kTolMas));
    CHECK(near(d.omega[2] / kMas, -53.00, kTolMas));
    CHECK(near(d.mean_m, 1.210, 0.005));
    CHECK(near(d.max_m, 1.877, 0.005));

    // the time shift is no part of any frame difference here, and each case is a pure rotation (the fit leaves nothing)
    for (const Fit* f : {&a, &a_kin, &b, &c, &d}) {
        CHECK(std::fabs(f->tau) < 1.0e-6);
        CHECK(f->rms_m < 0.001);
    }

    // THE BAND, built from the named smaller contributions (PROVENANCE.md §38.12): the kinematic
    // equation-of-equinoxes terms 1.5 mas, EOP prediction differences between finals2000A and Horizons' own EOP
    // file 0.3 mas, ICRF/DE441 axis alignment 0.2 mas: the worst-case sum, 2.0 mas, is 70.6 mm at the mean radius.
    constexpr double kBandMas = 1.5 + 0.3 + 0.2;
    CHECK(near(kBandMas * kMas * p.mean_radius_km * 1.0e6, 70.6, 0.1));   // km -> mm
    // and the cases are separated by far more than the band
    const auto sep = [&](const Fit& x, const Fit& y) {
        const double d0 = x.omega[0] - y.omega[0], d1 = x.omega[1] - y.omega[1], d2 = x.omega[2] - y.omega[2];
        return std::sqrt(d0 * d0 + d1 * d1 + d2 * d2) / kMas;
    };
    CHECK(sep(a, b) > 10.0 * kBandMas);
    CHECK(sep(a, c) > 10.0 * kBandMas);
    CHECK(sep(b, c) > 10.0 * kBandMas);
    CHECK(sep(a, d) > 10.0 * kBandMas);
    CHECK(sep(b, d) > 10.0 * kBandMas);
    CHECK(sep(c, d) > 10.0 * kBandMas);
}
