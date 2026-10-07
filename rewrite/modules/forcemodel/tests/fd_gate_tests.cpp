// fd_gate_tests.cpp — SPEC-forcemodel.md §6.2 and FMOD-A-010 … -014: the finite-difference gate of the force plugins' Jacobians, in L6's form
// (SPEC-measmod §6.2), its sizing FROZEN BEFORE ANY RUN by tools/forcemodel_fd_sizing.cpp (it was tools/forcemodel_fd_sizing.py when it froze it, commit 4f6f5b6).
//
// For each plugin, at each of L4's four points, for each component pair: the central difference of the plugin's OWN acceleration over five sizes
// against the analytic row, within  eps(h) = h^2 F / 6 + nu / h  with F a RIGOROUS bound (three lemmas, the tool's docstring) and nu a registered
// bound of one evaluation that is ASSERTED AT EVERY STENCIL POINT against the same function evaluated in x87 long double from independent
// formulas (the polynomial form of each coefficient's potential, comparator_polynomials.hpp) on the same double inputs.  The wrong rows must fail.
#include "comparator_polynomials.hpp"
#include "fixture.hpp"

#include <odl/relativity/correction.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>

using namespace odl;
namespace comparator = odl::forcemodel::comparator;

namespace {

using LD = long double;
constexpr double kEps = 2.220446049250313e-16;
constexpr std::array<double, 5> kHs{10.0, 30.0, 100.0, 300.0, 1000.0};
constexpr std::array<double, 4> kHvs{0.01, 0.1, 1.0, 10.0};
constexpr double kHMax = 1000.0;

struct V3L { LD x = 0, y = 0, z = 0; };
V3L operator+(V3L a, V3L b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3L operator-(V3L a, V3L b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3L operator*(LD s, V3L a) { return {s * a.x, s * a.y, s * a.z}; }
LD dot(V3L a, V3L b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3L cross(V3L a, V3L b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
V3L up(const Vec3& v) { return {v.x, v.y, v.z}; }
V3L rotate(const Mat3& m, V3L v) {                                    // exact promotion of the double matrix
    return {m.r[0][0] * v.x + m.r[0][1] * v.y + m.r[0][2] * v.z, m.r[1][0] * v.x + m.r[1][1] * v.y + m.r[1][2] * v.z,
            m.r[2][0] * v.x + m.r[2][1] * v.y + m.r[2][2] * v.z};
}
V3L rotate_t(const Mat3& m, V3L v) {
    return {m.r[0][0] * v.x + m.r[1][0] * v.y + m.r[2][0] * v.z, m.r[0][1] * v.x + m.r[1][1] * v.y + m.r[2][1] * v.z,
            m.r[0][2] * v.x + m.r[1][2] * v.y + m.r[2][2] * v.z};
}

/// One coefficient of an expansion: degree, order, which of C and S, and its value.
struct Coef { int n, m; char kind; double value; };

/// a = grad V in x87 long double, V the sum of the point mass and each coefficient's potential GM a_e^n N_nm q / r^(2n+1) — the comparator.
V3L ld_field(bool point_mass, const std::vector<Coef>& coefs, double gm, double ae, V3L x) {
    const LD r2 = dot(x, x), r = sqrtl(r2), ir = 1.0L / r;
    V3L a;
    if (point_mass) a = (-static_cast<LD>(gm) * ir * ir * ir) * x;
    for (const Coef& c : coefs) {
        if (c.value == 0.0) continue;
        const comparator::Harmonic* h = nullptr;
        for (const auto& k : comparator::kHarmonics)
            if (k.n == c.n && k.m == c.m && k.kind == c.kind) h = &k;
        REQUIRE(h != nullptr);
        LD q = 0;
        V3L dq;
        for (int t = 0; t < h->terms; ++t) {
            const auto& mono = h->q[t];
            const LD xi = mono.i ? powl(x.x, mono.i) : 1.0L, yj = mono.j ? powl(x.y, mono.j) : 1.0L, zk = mono.k ? powl(x.z, mono.k) : 1.0L;
            q += mono.c * xi * yj * zk;
            if (mono.i) dq.x += mono.c * mono.i * (mono.i > 1 ? powl(x.x, mono.i - 1) : 1.0L) * yj * zk;
            if (mono.j) dq.y += mono.c * mono.j * xi * (mono.j > 1 ? powl(x.y, mono.j - 1) : 1.0L) * zk;
            if (mono.k) dq.z += mono.c * mono.k * xi * yj * (mono.k > 1 ? powl(x.z, mono.k - 1) : 1.0L);
        }
        const int s = 2 * c.n + 1;
        const LD irs = powl(ir, s), irs2 = powl(ir, s + 2);
        const LD pref = static_cast<LD>(gm) * powl(static_cast<LD>(ae), c.n) * h->norm * c.value;
        a = a + pref * (irs * dq + (-static_cast<LD>(s) * q * irs2) * x);
    }
    return a;
}

std::vector<Coef> coefs_of_field(const gravity::ConventionalField& f, int nmax) {
    std::vector<Coef> out;
    for (int n = 2; n <= nmax; ++n)
        for (int m = 0; m <= n; ++m) {
            out.push_back({n, m, 'C', f.c(n, m)});
            if (m > 0) out.push_back({n, m, 'S', f.s(n, m)});
        }
    return out;
}

std::vector<Coef> coefs_of_increments(const tides::TideIncrements& inc, int nmax) {
    std::vector<Coef> out;
    for (int n = 2; n <= nmax; ++n)
        for (int m = 0; m <= n; ++m) {
            out.push_back({n, m, 'C', inc.dc(n, m)});
            if (m > 0) out.push_back({n, m, 'S', inc.ds(n, m)});
        }
    return out;
}

/// The registered F of §6.2 (i) for a field-like sum: 96 GM / r_min^5 for the point mass, plus GM a_e^n (|C| Phi^C + |S| Phi^S) / r_min^(n+5).
double f_field(bool point_mass, const std::vector<Coef>& coefs, double gm, double ae, double r_min, double factor) {
    double total = point_mass ? 96.0 * gm / std::pow(r_min, 5) : 0.0;
    for (const Coef& c : coefs) {
        for (const auto& k : comparator::kHarmonics)
            if (k.n == c.n && k.m == c.m && k.kind == c.kind)
                total += gm * std::pow(ae, c.n) * std::abs(c.value) * k.phi / std::pow(r_min, c.n + 5);
    }
    return factor * total;
}

struct Case {
    explicit Case(odl::time::Epoch epoch) : t(epoch) {}
    std::string plugin;
    std::string point;
    const dyn::Force* force = nullptr;
    odl::time::Epoch t;
    Vec3 r, v;
    std::function<V3L(const Vec3&, const Vec3&)> reference;     // the long double evaluation of the plugin's own function
    double f_pos = 0.0, f_vel = 0.0, nu = 0.0;
    std::vector<std::pair<std::string, Mat3>> position_controls, velocity_controls;
};

Mat3 mat_sub(const Mat3& a, const Mat3& b) {
    Mat3 m;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = a.r[i][j] - b.r[i][j];
    return m;
}
double max_abs(const Mat3& a) {
    double m = 0.0;
    for (const auto& row : a.r) for (double x : row) m = std::max(m, std::abs(x));
    return m;
}

dyn::ForceEvaluation eval(const Case& c, const Vec3& r, const Vec3& v) {
    dyn::ParameterRegistry reg;
    dyn::ParameterSet params(reg);
    auto ev = c.force->accel(c.t, frames::Position<frames::Frame::GCRS>{r}, v, params, reg);
    if (!ev) FAIL(c.plugin << " at " << c.point << " refused: " << ev.error().id << ": " << ev.error().message);
    return *ev;
}

struct Report {
    double worst_b = 0.0;            // max |f_hat - A| / eps
    double worst_nu = 0.0;           // max |a_double - a_ld| / nu over every stencil point
    double worst_nu_vel = 0.0;
    double best_power_rel = 0.0;     // min eps / ||A||
    std::map<std::string, double> power;
    int stencil_points = 0, components = 0;
};

Report run_gate(const Case& c) {
    Report rep;
    const dyn::ForceEvaluation center = eval(c, c.r, c.v);
    const Mat3 A = center.d_state.d_position();
    const std::optional<Mat3> Av = center.d_state.d_velocity();
    auto assert_nu = [&](const Vec3& a_double, const Vec3& r, const Vec3& v, double& worst) {
        const V3L ref = c.reference(r, v);
        const double dev = std::max({std::abs(static_cast<double>(up(a_double).x - ref.x)), std::abs(static_cast<double>(up(a_double).y - ref.y)),
                                     std::abs(static_cast<double>(up(a_double).z - ref.z))});
        CHECK(dev <= c.nu);                       // asserted at EVERY stencil point (FMOD §6.2 (e))
        worst = std::max(worst, dev / c.nu);
        ++rep.stencil_points;
    };
    auto eps_pos = [&](double h) { return h * h * c.f_pos / 6.0 + c.nu / h; };

    // ---- position columns
    for (int j = 0; j < 3; ++j) {
        for (double h : kHs) {
            Vec3 rp = c.r, rm = c.r;
            (j == 0 ? rp.x : j == 1 ? rp.y : rp.z) += h;
            (j == 0 ? rm.x : j == 1 ? rm.y : rm.z) -= h;
            const Vec3 ap = eval(c, rp, c.v).acceleration.metres_per_second_squared();
            const Vec3 am = eval(c, rm, c.v).acceleration.metres_per_second_squared();
            assert_nu(ap, rp, c.v, rep.worst_nu);
            assert_nu(am, rm, c.v, rep.worst_nu);
            const double spacing = (j == 0 ? rp.x - rm.x : j == 1 ? rp.y - rm.y : rp.z - rm.z);       // the ACTUAL difference of the operands, exact
            const double fh[3] = {(ap.x - am.x) / spacing, (ap.y - am.y) / spacing, (ap.z - am.z) / spacing};
            for (int i = 0; i < 3; ++i) {
                const double err = std::abs(fh[i] - A.r[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]);
                CHECK(err <= eps_pos(h));                                                      // criterion (b)
                rep.worst_b = std::max(rep.worst_b, err / eps_pos(h));
                ++rep.components;
            }
        }
    }
    // ---- velocity columns
    for (int j = 0; j < 3; ++j) {
        for (double h : kHvs) {
            Vec3 vp = c.v, vm = c.v;
            (j == 0 ? vp.x : j == 1 ? vp.y : vp.z) += h;
            (j == 0 ? vm.x : j == 1 ? vm.y : vm.z) -= h;
            const Vec3 ap = eval(c, c.r, vp).acceleration.metres_per_second_squared();
            const Vec3 am = eval(c, c.r, vm).acceleration.metres_per_second_squared();
            assert_nu(ap, c.r, vp, rep.worst_nu_vel);
            assert_nu(am, c.r, vm, rep.worst_nu_vel);
            const double spacing = (j == 0 ? vp.x - vm.x : j == 1 ? vp.y - vm.y : vp.z - vm.z);
            const double fh[3] = {(ap.x - am.x) / spacing, (ap.y - am.y) / spacing, (ap.z - am.z) / spacing};
            for (int i = 0; i < 3; ++i) {
                if (!Av) {
                    CHECK(fh[i] == 0.0);                                                       // criterion (c), a block declared absent
                } else {
                    const double err = std::abs(fh[i] - Av->r[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)]);
                    const double eps = h * h * c.f_vel / 6.0 + c.nu / h;
                    CHECK(err <= eps);                                                         // criterion (c), a block declared present
                    rep.worst_b = std::max(rep.worst_b, err / eps);
                }
                ++rep.components;
            }
        }
    }
    if (!Av) {
        CHECK(center.d_state.neglected_velocity_bound_per_s() == 0.0);
    }
    // ---- the controls (criterion (d)): each wrong row must fail (b) by >= 1e3 eps at the best step
    double best = 1e300;
    for (double h : kHs) best = std::min(best, eps_pos(h));
    rep.best_power_rel = best / std::max(max_abs(A), std::numeric_limits<double>::min());
    for (const auto& [name, wrong] : c.position_controls) {
        const double power = max_abs(mat_sub(wrong, A)) / best;
        rep.power[name] = power;
        CHECK(power >= 1.0e3);
    }
    if (Av) {
        double best_v = 1e300;
        for (double h : kHvs) best_v = std::min(best_v, c.nu / h);
        for (const auto& [name, wrong] : c.velocity_controls) {
            const double power = max_abs(mat_sub(wrong, *Av)) / best_v;
            rep.power[name] = power;
            CHECK(power >= 1.0e3);
        }
    }
    return rep;
}

void print(const Case& c, const Report& r) {
    std::ostringstream os;
    os << "  " << c.plugin << " @ " << c.point << ": F = " << c.f_pos << ", nu = " << c.nu << "; worst |f^ - A| / eps = " << r.worst_b
       << "; worst |a - a_ld| / nu = " << r.worst_nu << " (position stencil), " << r.worst_nu_vel << " (velocity); " << r.stencil_points
       << " stencil points, " << r.components << " components; best eps / ||A|| = " << r.best_power_rel << "; controls:";
    for (const auto& [k, v] : r.power) os << " [" << k << ": " << v << " eps]";
    WARN(os.str());
}

Mat3 map_rows(const Mat3& a, const std::function<double(double)>& f) {
    Mat3 m;
    for (std::size_t i = 0; i < 3; ++i)
        for (std::size_t j = 0; j < 3; ++j) m.r[i][j] = f(a.r[i][j]);
    return m;
}

}  // namespace

TEST_CASE("FMOD-A-010: the finite-difference gate, Gravity (N = M = 4), at L4's four points, with the registered F recomputed", "[forcemodel][fd]") {
    // the frozen figures of SPEC-forcemodel §6.2 (iii): F, eps(10 m), eps(100 m), eps(1000 m) per point
    struct Frozen { double f, e10, e100, e1000; };
    const Frozen frozen[4] = {{2.948e-21, 8.03e-16, 8.52e-17, 4.99e-16}, {3.417e-18, 1.28e-14, 6.97e-15, 5.70e-13},
                              {2.078e-18, 1.06e-14, 4.52e-15, 3.46e-13}, {2.466e-18, 1.13e-14, 5.24e-15, 4.11e-13}};
    int idx = 0;
    for (const auto& p : fx::points()) {
        const auto t = fx::epoch_of(p);
        const auto o = fx::circular_orbit(p.r_m, p.phase_rad);
        const auto orientation = fx::orientation_series();
        const forcemodel::Gravity plugin(fx::model(), orientation, fx::deg(4), fx::ord(4), forcemodel::GravityOptions{true, {}});
        auto field = fx::model().conventional(t, true);
        REQUIRE(field.has_value());
        const double gm = field->scaling().gm_m3_s2(), ae = field->scaling().ae_m();
        const auto coefs = coefs_of_field(*field, 4);
        auto eo = orientation.at(t);
        REQUIRE(eo.has_value());
        const Mat3 m = eo->rotation.m;

        Case c(t);
        c.plugin = "Gravity(4,4)"; c.point = p.name; c.force = &plugin; c.t = t; c.r = o.r; c.v = o.v;
        c.reference = [&](const Vec3& r, const Vec3&) { return rotate_t(m, ld_field(true, coefs, gm, ae, rotate(m, up(r)))); };
        const double a_norm = eval(c, o.r, o.v).acceleration.metres_per_second_squared().norm();
        c.f_pos = f_field(true, coefs, gm, ae, o.r.norm() - kHMax, 1.01);
        c.nu = 64.0 * kEps * a_norm;
        c.f_vel = 0.0;

        // the frozen figures are reproduced, not tuned: F and eps(h) to 2 %
        CHECK(std::abs(c.f_pos - frozen[idx].f) <= 0.02 * frozen[idx].f);
        auto eps_of = [&](double h) { return h * h * c.f_pos / 6.0 + c.nu / h; };
        CHECK(std::abs(eps_of(10.0) - frozen[idx].e10) <= 0.02 * frozen[idx].e10);
        CHECK(std::abs(eps_of(100.0) - frozen[idx].e100) <= 0.02 * frozen[idx].e100);
        CHECK(std::abs(eps_of(1000.0) - frozen[idx].e1000) <= 0.02 * frozen[idx].e1000);

        // the wrong rows, built from the analytic row and the plugin's own rotation: G_itrs = m A m^T
        const Mat3 A = eval(c, o.r, o.v).d_state.d_position();
        const Mat3 g_itrs = m.times(A).times(m.transpose());
        const Mat3 m2 = m.times(m);
        c.position_controls.push_back({"transposed rotation", m2.times(A).times(m2.transpose())});
        c.position_controls.push_back({"unrotated", g_itrs});
        c.position_controls.push_back({"sign flipped", map_rows(A, [](double x) { return -x; })});

        const Report rep = run_gate(c);
        print(c, rep);
        ++idx;
    }
    REQUIRE(idx == 4);
}

TEST_CASE("FMOD-A-011: the finite-difference gate, ThirdBody -- the Sun, the Moon and Jupiter, at the four points", "[forcemodel][fd]") {
    int cases = 0;
    for (const auto& p : fx::points()) {
        const auto t = fx::epoch_of(p);
        const auto o = fx::circular_orbit(p.r_m, p.phase_rad);
        for (eph::Body body : {eph::Body::Sun, eph::Body::Moon, eph::Body::JupiterBarycentre}) {
            const forcemodel::ThirdBody plugin(body, fx::ephemeris(), fx::gm(), fx::leaps());
            auto st = fx::ephemeris().geocentric_state(body, t, fx::leaps());
            auto mu = fx::gm().gm_m3_s2(body);
            REQUIRE(st.has_value());
            REQUIRE(mu.has_value());
            const Vec3 s = 1000.0 * st->position();                           // the test's own crossing
            const double mu_d = *mu;

            Case c(t);
            c.plugin = plugin.id().name; c.point = p.name; c.force = &plugin; c.t = t; c.r = o.r; c.v = o.v;
            c.reference = [s, mu_d](const Vec3& r, const Vec3&) {
                const V3L sl = up(s), rl = up(r), d = sl - rl;
                const LD dn = sqrtl(dot(d, d)), sn = sqrtl(dot(sl, sl));
                return static_cast<LD>(mu_d) * ((1.0L / (dn * dn * dn)) * d - (1.0L / (sn * sn * sn)) * sl);
            };
            const double d_min = s.norm() - o.r.norm() - kHMax;
            REQUIRE(d_min > 0.0);
            c.f_pos = 96.0 * mu_d / std::pow(d_min, 5);
            const Vec3 d0 = s - o.r;
            c.nu = 16.0 * kEps * mu_d * (1.0 / (s.norm() * s.norm()) + 1.0 / (d0.norm() * d0.norm()));
            c.f_vel = 0.0;

            // the wrong row: the factor 3 replaced by 1 in 3 d d^T
            Mat3 wrong;
            const double dd[3] = {d0.x, d0.y, d0.z};
            const double dn = d0.norm();
            for (std::size_t i = 0; i < 3; ++i)
                for (std::size_t j = 0; j < 3; ++j)
                    wrong.r[i][j] = mu_d * (1.0 * dd[i] * dd[j] / std::pow(dn, 5) - (i == j ? 1.0 / (dn * dn * dn) : 0.0));
            c.position_controls.push_back({"3 replaced by 1", wrong});
            c.position_controls.push_back({"sign flipped", map_rows(eval(c, o.r, o.v).d_state.d_position(), [](double x) { return -x; })});

            const Report rep = run_gate(c);
            print(c, rep);
            ++cases;
        }
    }
    REQUIRE(cases == 12);
}

TEST_CASE("FMOD-A-012: the finite-difference gate, Relativity -- the three terms, both blocks, at the four points", "[forcemodel][fd]") {
    using relativity::Correction;
    int cases = 0;
    for (const auto& p : fx::points()) {
        const auto t = fx::epoch_of(p);
        const auto o = fx::circular_orbit(p.r_m, p.phase_rad);
        auto sun = fx::ephemeris().geocentric_state(eph::Body::Sun, t, fx::leaps());
        REQUIRE(sun.has_value());
        const Vec3 earth_r = -1.0 * (1000.0 * sun->position()), earth_v = -1.0 * (1000.0 * sun->velocity());
        const double c2 = Correction::kC * Correction::kC;
        const double gme = Correction::kGmEarth, K = gme / c2;
        const double v = o.v.norm();
        const double r_min = o.r.norm() - kHMax;
        for (auto term : {relativity::Term::Schwarzschild, relativity::Term::LenseThirring, relativity::Term::DeSitter}) {
            const forcemodel::Relativity plugin(term, fx::ephemeris(), fx::leaps());
            Case c(t);
            c.plugin = plugin.id().name; c.point = p.name; c.force = &plugin; c.t = t; c.r = o.r; c.v = o.v;
            c.reference = [term, earth_r, earth_v](const Vec3& r, const Vec3& vel) {
                const V3L rl = up(r), vl = up(vel);
                const LD cc2 = static_cast<LD>(Correction::kC) * static_cast<LD>(Correction::kC);
                const LD gme_l = Correction::kGmEarth, rn = sqrtl(dot(rl, rl));
                switch (term) {
                    case relativity::Term::Schwarzschild:
                        return (gme_l / (cc2 * rn * rn * rn)) * ((4.0L * gme_l / rn - dot(vl, vl)) * rl + (4.0L * dot(rl, vl)) * vl);
                    case relativity::Term::LenseThirring: {
                        const V3L J{0.0L, 0.0L, static_cast<LD>(Correction::kEarthAngularMomentumPerMass)};
                        return (2.0L * gme_l / (cc2 * rn * rn * rn)) * (((3.0L / (rn * rn)) * dot(rl, J)) * cross(rl, vl) + cross(vl, J));
                    }
                    case relativity::Term::DeSitter: {
                        const V3L R = up(earth_r), Rd = up(earth_v);
                        const LD Rn = sqrtl(dot(R, R));
                        const V3L inner = (-static_cast<LD>(Correction::kGmSun) / (cc2 * Rn * Rn * Rn)) * R;
                        return 3.0L * cross(cross(Rd, inner), vl);
                    }
                }
                return V3L{};
            };
            const double a_norm = eval(c, o.r, o.v).acceleration.metres_per_second_squared().norm();
            c.nu = 32.0 * kEps * a_norm;
            c.f_vel = 0.0;
            switch (term) {
                case relativity::Term::Schwarzschild:
                    c.f_pos = K * (4.0 * gme * 180.0 / std::pow(r_min, 6) + (v * v + 4.0 * std::sqrt(3.0) * v * v) * 96.0 / std::pow(r_min, 5));
                    break;
                case relativity::Term::LenseThirring:
                    c.f_pos = 2.0 * K * (3.0 * Correction::kEarthAngularMomentumPerMass * std::sqrt(3.0) * v * 420.0 +
                                         Correction::kEarthAngularMomentumPerMass * v * 60.0) / std::pow(r_min, 6);
                    break;
                case relativity::Term::DeSitter:
                    c.f_pos = 0.0;
                    break;
            }
            const dyn::ForceEvaluation ev = eval(c, o.r, o.v);
            REQUIRE(ev.d_state.d_velocity().has_value());
            const Mat3 A = ev.d_state.d_position(), Av = *ev.d_state.d_velocity();
            if (term != relativity::Term::DeSitter) c.position_controls.push_back({"position block sign flipped", map_rows(A, [](double x) { return -x; })});
            else for (const auto& row : A.r) for (double x : row) CHECK(x == 0.0);        // d a / d r is EXACTLY zero for de Sitter
            c.velocity_controls.push_back({"velocity block sign flipped", map_rows(Av, [](double x) { return -x; })});
            c.velocity_controls.push_back({"velocity block replaced by the position block's shape", A});

            const Report rep = run_gate(c);
            print(c, rep);
            ++cases;
        }
    }
    REQUIRE(cases == 12);
}

TEST_CASE("FMOD-A-013: the finite-difference gate, Tides configured to degrees <= 4, at the four points", "[forcemodel][fd]") {
    int cases = 0;
    for (const auto& p : fx::points()) {
        const auto t = fx::epoch_of(p);
        const auto o = fx::circular_orbit(p.r_m, p.phase_rad);
        const auto orientation = fx::orientation_series();
        forcemodel::TidesOptions opts;                                   // §6.2 (i): solid, solid pole, ocean pole to 2, ocean to 4
        opts.solid = true;
        opts.solid_pole = true;
        opts.ocean = &fx::fes();
        opts.ocean_degree = 4;
        opts.ocean_pole = &fx::desai();
        opts.ocean_pole_degree = 2;
        opts.extrapolate_secular = true;
        const forcemodel::Tides plugin(fx::model(), orientation, fx::ephemeris(), fx::leaps(), opts);
        auto field = fx::model().conventional(t, true);
        REQUIRE(field.has_value());
        const double gm = field->scaling().gm_m3_s2(), ae = field->scaling().ae_m();
        auto eo = orientation.at(t);
        REQUIRE(eo.has_value());
        const Mat3 m = eo->rotation.m;
        const fx::IndependentTides ind = fx::independent_tides(t, eo->record, m, opts, *field);
        REQUIRE(ind.total.max_degree() == 4);
        const auto coefs = coefs_of_increments(ind.total, 4);

        Case c(t);
        c.plugin = "Tides(<=4)"; c.point = p.name; c.force = &plugin; c.t = t; c.r = o.r; c.v = o.v;
        c.reference = [&](const Vec3& r, const Vec3&) { return rotate_t(m, ld_field(false, coefs, gm, ae, rotate(m, up(r)))); };
        const dyn::ForceEvaluation ev = eval(c, o.r, o.v);
        const double a_norm = ev.acceleration.metres_per_second_squared().norm();
        c.f_pos = f_field(false, coefs, gm, ae, o.r.norm() - kHMax, 1.01);
        c.nu = 128.0 * kEps * a_norm;
        c.f_vel = 0.0;
        const Mat3 A = ev.d_state.d_position();
        const Mat3 g_itrs = m.times(A).times(m.transpose());
        const Mat3 m2 = m.times(m);
        c.position_controls.push_back({"transposed rotation", m2.times(A).times(m2.transpose())});
        c.position_controls.push_back({"unrotated", g_itrs});
        c.position_controls.push_back({"sign flipped", map_rows(A, [](double x) { return -x; })});

        const Report rep = run_gate(c);
        print(c, rep);
        ++cases;
    }
    REQUIRE(cases == 4);
}
