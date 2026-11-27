// joint_tests.cpp — SPEC-estimation.md §8.5, EST-A-506: the joint covariance assembled over the six state parameters and L3's registered force parameters, in the registry's issue order.  This target links
// `dynamics` AND `estimation`, so that `modules/estimation` stays `core`-only (EST-Q-008; EST-Q-009 ruled 2026-11-27: the orbit fit's nonlinearity bound and finite-difference correlations belong with
// the exit gate, not here).  REGISTERED before step 5's code existed (v0.6, 6794e34): nothing here may be tuned to a result.
//
// The construction (§8.5): three parameters declared in the order c0, c1, c2 (kind empirical_acceleration, unit "1", subject "sat-A"); a ForceSet of a two-body force (mu = 3.986004415e14 m^3/s^2, the unit
// of the L3 tests) and three constant-acceleration forces along x, y, z of magnitude 1e-6 m/s^2 (= 1e-9 km/s^2) each scaled by its parameter, all values 1 — re-implemented here against the L3 `Force`
// interface as modules/dynamics/tests/sensitivity_tests.cpp does; the initial state r = (7000, 0, 0) km, v = (0, sqrt(mu/7000), 0) km/s (a circular orbit of period 5828.5 s); 58 epochs every 100 s
// from 100 s to 5800 s, each from propagate_with_sensitivities giving Phi and the sensitivity columns BY IDENTITY (column(reg, id)); the observations are the three position components, sigma = 1e-3,
// noise 0.8 sigma times seeded normals (seed 20261127, linearised at the truth: the residual is the noise); rows over the 9 parameters (x, y, z, vx, vy, vz, c0, c1, c2).

#include <odl/dynamics/variational.hpp>
#include <odl/estimation/joint_covariance.hpp>
#include <odl/estimation/normal_equations.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace odl;
using namespace odl::dyn;
using namespace odl::estimation;
using frames::Frame;
using ld = long double;

namespace {

constexpr double kMu = 3.986004415e14;   // NOT-A-UNIT-CROSSING: m^3/s^2, the gravitational parameter in the unit the L3 tests use

class TwoBody final : public Force {
public:
    ForceId id() const override { return ForceId{"two-body"}; }
    const std::vector<ParameterId>& consumes() const override { return none_; }
    odl::Result<ForceEvaluation, DynError> accel(const odl::time::Epoch&, const frames::Position<Frame::GCRS>& r_m, const Vec3&, const ParameterSet&, const ParameterRegistry& reg) const override {
        const Vec3 r = r_m.metres();
        const double r2 = r.x * r.x + r.y * r.y + r.z * r.z, rn = std::sqrt(r2);
        const double r3 = r2 * rn, r5 = r3 * r2;
        const double rv[3] = {r.x, r.y, r.z};
        Mat3 dadr{};
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j) dadr.r[i][j] = -kMu * ((i == j ? 1.0 / r3 : 0.0) - 3.0 * rv[i] * rv[j] / r5);
        return ForceEvaluation{frames::Acceleration<Frame::GCRS>{Vec3{-kMu * r.x / r3, -kMu * r.y / r3, -kMu * r.z / r3}}, StateJacobian::no_velocity_dependence(dadr, 0.0), ParameterJacobian{reg}};
    }

private:
    std::vector<ParameterId> none_{};
};

class ScaledConstant final : public Force {
public:
    ScaledConstant(ParameterId c, Vec3 dir, std::string name) : c_(c), dir_(dir), consumes_{c}, name_(std::move(name)) {}
    ForceId id() const override { return ForceId{name_}; }
    const std::vector<ParameterId>& consumes() const override { return consumes_; }
    odl::Result<ForceEvaluation, DynError> accel(const odl::time::Epoch&, const frames::Position<Frame::GCRS>&, const Vec3&, const ParameterSet& p, const ParameterRegistry& reg) const override {
        auto c = p.value(c_);
        if (!c) return odl::err(c.error());
        ParameterJacobian dp{reg};
        auto put = dp.set_column(c_, dir_);
        if (!put) return odl::err(put.error());
        return ForceEvaluation{frames::Acceleration<Frame::GCRS>{Vec3{*c * dir_.x, *c * dir_.y, *c * dir_.z}}, StateJacobian::no_velocity_dependence(Mat3{}, 0.0), std::move(dp)};
    }

private:
    ParameterId c_;
    Vec3 dir_;
    std::vector<ParameterId> consumes_;
    std::string name_;
};

struct Rng {
    std::uint64_t s = 20261127ULL;
    std::uint64_t next() {
        s += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = s;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    double uni() { return static_cast<double>(next() >> 11) * 0x1p-52 - 1.0; }
    double normal() {
        for (;;) {
            const double u = uni(), v = uni(), q = u * u + v * v;
            if (q >= 1.0 || q == 0.0) continue;
            return u * std::sqrt(-2.0 * std::log(q) / q);
        }
    }
};

using Mat = std::vector<ld>;
Mat inverse(Mat a, std::size_t n) {
    Mat inv(n * n, 0.0L);
    for (std::size_t i = 0; i < n; ++i) inv[i * n + i] = 1.0L;
    for (std::size_t c = 0; c < n; ++c) {
        std::size_t p = c;
        for (std::size_t i = c + 1; i < n; ++i)
            if (std::fabs(a[i * n + c]) > std::fabs(a[p * n + c])) p = i;
        for (std::size_t j = 0; j < n; ++j) {
            std::swap(a[c * n + j], a[p * n + j]);
            std::swap(inv[c * n + j], inv[p * n + j]);
        }
        const ld d = a[c * n + c];
        for (std::size_t j = 0; j < n; ++j) {
            a[c * n + j] /= d;
            inv[c * n + j] /= d;
        }
        for (std::size_t i = 0; i < n; ++i) {
            if (i == c) continue;
            const ld f = a[i * n + c];
            if (f == 0.0L) continue;
            for (std::size_t j = 0; j < n; ++j) {
                a[i * n + j] -= f * a[c * n + j];
                inv[i * n + j] -= f * inv[c * n + j];
            }
        }
    }
    return inv;
}

constexpr std::size_t kN = 9;
constexpr std::size_t kEpochs = 58;
constexpr double kSigma = 1.0e-3;   // NOT-A-UNIT-CROSSING: the registered observation sigma, 10^-3 (position units of the integrator's state)

struct Rows {
    std::vector<double> a;       // (3 * kEpochs) x 9
    std::vector<double> noise;   // 3 * kEpochs
};

// the rows over (x, y, z, vx, vy, vz, c0, c1, c2); `order` gives which registered parameter's column goes in force slot 0, 1, 2 (identity {0,1,2} is the correct assembly)
Rows build_rows(const std::array<int, 3>& order) {
    ParameterRegistry reg;
    std::vector<ParameterId> ids;
    for (int j = 0; j < 3; ++j) ids.push_back(reg.declare({ParameterKind::empirical_acceleration, "1", "c" + std::to_string(j), "sat-A"}));
    ParameterSet vals{reg};
    ForceSet forces{reg};
    REQUIRE(forces.add(std::make_shared<TwoBody>()).has_value());
    const Vec3 dirs[3] = {{1.0e-6, 0, 0}, {0, 1.0e-6, 0}, {0, 0, 1.0e-6}};   // NOT-A-UNIT-CROSSING: 1e-6 m/s^2 = 1e-9 km/s^2
    for (std::size_t j = 0; j < 3; ++j) {
        REQUIRE(vals.set(ids[j], 1.0).has_value());
        REQUIRE(forces.add(std::make_shared<ScaledConstant>(ids[j], dirs[j], "push-" + std::to_string(j))).has_value());
    }
    const double v0 = std::sqrt(3.986004415e5 / 7000.0);   // km/s
    const frames::State<Frame::GCRS> x0{*odl::time::Epoch::from_gps_week(2000, 0.0), Vec3{7000.0, 0.0, 0.0}, Vec3{0.0, v0, 0.0}};
    Rng g;
    Rows rows;
    for (std::size_t e = 1; e <= kEpochs; ++e) {
        const double seconds = 100.0 * static_cast<double>(e);
        auto sol = propagate_with_sensitivities(forces, vals, reg, x0, seconds, 1.0e-12);   // NOT-A-UNIT-CROSSING: the integration tolerance, as in the L3 tests
        REQUIRE(sol.has_value());
        std::array<std::array<double, 6>, 3> cols{};
        for (std::size_t j = 0; j < 3; ++j) {
            auto c = sol->column(reg, ids[static_cast<std::size_t>(order[j])]);          // BY IDENTITY, never by column number
            REQUIRE(c.has_value());
            cols[j] = *c;
        }
        for (std::size_t r = 0; r < 3; ++r) {
            for (std::size_t c = 0; c < 6; ++c) rows.a.push_back(sol->phi[r][c]);
            for (std::size_t j = 0; j < 3; ++j) rows.a.push_back(cols[j][r]);
            rows.noise.push_back(0.8 * kSigma * g.normal());
        }
    }
    return rows;
}

NormalEquations fit_of(const Rows& rows, const std::vector<std::string>& names) {
    auto made = NormalEquations::create(names);
    REQUIRE(made.has_value());
    NormalEquations ne = std::move(*made);
    for (std::size_t k = 0; k < rows.noise.size(); ++k) REQUIRE(ne.add(std::span<const double>(&rows.a[k * kN], kN), rows.noise[k], kSigma).has_value());
    return ne;
}

}  // namespace

TEST_CASE("EST-A-506: the joint covariance assembled over the state and L3's registered parameters, in registry order", "[estimation][joint][a506]") {
    // the layout from the registry's declarations in issue order
    ParameterRegistry reg;
    std::vector<ParameterId> ids;
    for (int j = 0; j < 3; ++j) ids.push_back(reg.declare({ParameterKind::empirical_acceleration, "1", "c" + std::to_string(j), "sat-A"}));
    std::vector<std::string> force_names;
    for (const auto& id : ids) {
        auto d = reg.declaration(id);
        REQUIRE(d.has_value());
        force_names.push_back((*d)->name);
    }
    const auto layout = JointParameterLayout::create({"x", "y", "z", "vx", "vy", "vz"}, force_names);
    REQUIRE(layout.has_value());
    const std::vector<std::string> names = layout->all();
    REQUIRE(names.size() == kN);

    const Rows rows = build_rows({0, 1, 2});
    REQUIRE(rows.noise.size() == 3 * kEpochs);
    auto ne = fit_of(rows, names);
    const auto sol = ne.solve();
    REQUIRE(sol.has_value());
    const double B = sol->certified_relative_error;
    std::printf("JOINT 506 solve: n %zu rows %zu nu %td B %.3e kappa_est %.3e\n", kN, rows.noise.size(), sol->degrees_of_freedom, B, sol->condition.condition_estimate);
    // (a) the layout equals Solution::names
    CHECK(sol->names == names);
    const auto jc = joint_covariance(*sol, *layout);
    REQUIRE(jc.has_value());
    CHECK(jc->names == names);
    CHECK(sol->degrees_of_freedom == 165);   // 3 * 58 - 9

    // (b) Q against the independent long double inverse of A^T W A formed from the same rows, in the D-normalised Frobenius norm, to 64 B
    Mat N(kN * kN, 0.0L);
    for (std::size_t k = 0; k < rows.noise.size(); ++k)
        for (std::size_t i = 0; i < kN; ++i)
            for (std::size_t j = 0; j < kN; ++j) N[i * kN + j] += (static_cast<ld>(rows.a[k * kN + i]) / kSigma) * (static_cast<ld>(rows.a[k * kN + j]) / kSigma);
    const Mat Qref = inverse(N, kN);
    ld num = 0.0L, den = 0.0L;
    for (std::size_t i = 0; i < kN; ++i)
        for (std::size_t j = 0; j < kN; ++j) {
            const ld dd = std::sqrt(N[i * kN + i] * N[j * kN + j]);
            const ld a = static_cast<ld>(jc->formal[i * kN + j]) * dd, b = Qref[i * kN + j] * dd;
            num += (a - b) * (a - b);
            den += b * b;
        }
    const double qerr = static_cast<double>(std::sqrt(num / den));
    std::printf("JOINT 506b Q against the independent inverse: D-normalised Frobenius relative error %.3e (bound 64 B = %.3e)\n", qerr, 64.0 * B);
    CHECK(qerr <= 64.0 * B);

    // (c) the correlation matrix: the independent normalisation, exact symmetry, unit diagonal, |rho| <= 1
    double rmax = 0.0;
    bool sym = true, unit = true, bounded = true;
    for (std::size_t i = 0; i < kN; ++i) {
        unit = unit && jc->correlation[i * kN + i] == 1.0;
        for (std::size_t j = 0; j < kN; ++j) {
            const double rho_ref = static_cast<double>(Qref[i * kN + j] / std::sqrt(Qref[i * kN + i] * Qref[j * kN + j]));
            rmax = std::max(rmax, std::fabs(jc->correlation[i * kN + j] - rho_ref));
            sym = sym && jc->correlation[i * kN + j] == jc->correlation[j * kN + i];
            bounded = bounded && std::fabs(jc->correlation[i * kN + j]) <= 1.0;
        }
    }
    std::printf("JOINT 506c correlation: largest |rho - rho_ref| %.3e (bound 64 B = %.3e); symmetric %d, unit diagonal %d, bounded %d\n", rmax, 64.0 * B, sym, unit, bounded);
    CHECK(rmax <= 64.0 * B);
    CHECK(sym);
    CHECK(unit);
    CHECK(bounded);

    // (d) formal and scaled: distinct and exactly related; sigma-hat_0^2 = Omega-hat / nu
    REQUIRE(jc->scaled.has_value());
    const double s2 = *sol->sigma0_squared();
    std::printf("JOINT 506d sigma-hat_0^2 = %.9g (predicted about 0.64, the squared noise factor); Omega-hat %.9g, nu %td\n", s2, *sol->weighted_rss, sol->degrees_of_freedom);
    bool exact = true;
    for (std::size_t k = 0; k < jc->formal.size(); ++k) exact = exact && ((*jc->scaled)[k] == s2 * jc->formal[k]);
    CHECK(exact);
    CHECK(s2 == *sol->weighted_rss / static_cast<double>(sol->degrees_of_freedom));
    CHECK(std::fabs(s2 - 1.0) >= 0.05);

    // (e) the sensitivity columns are those of the right ids: the c_j block of Q equals the reference's
    for (std::size_t j = 6; j < 9; ++j) {
        const double want = static_cast<double>(Qref[j * kN + j]);
        std::printf("JOINT 506e Q(%s, %s) = %.9e (independent %.9e, relative %.2e)\n", names[j].c_str(), names[j].c_str(), jc->formal[j * kN + j], want, std::fabs(jc->formal[j * kN + j] - want) / want);
        CHECK(std::fabs(jc->formal[j * kN + j] - want) / want <= 64.0 * B);
    }

    // controls (must fail): the layout with the force names permuted is refused; rows built with the columns of c0 and c2 exchanged give a different Q(c0, c0)
    {
        const auto perm = JointParameterLayout::create({"x", "y", "z", "vx", "vy", "vz"}, {force_names[1], force_names[0], force_names[2]});
        REQUIRE(perm.has_value());
        const auto bad = joint_covariance(*sol, *perm);
        REQUIRE(!bad.has_value());
        CHECK(std::string(bad.error().id) == "EST-F-505");
        CHECK(bad.error().message.find("6") != std::string::npos);
        CHECK(bad.error().message.find("c0") != std::string::npos);
        CHECK(bad.error().message.find("c1") != std::string::npos);
        std::printf("JOINT 506control permuted layout refused: %s\n", bad.error().message.c_str());
        const Rows swapped = build_rows({2, 1, 0});
        auto ne2 = fit_of(swapped, names);
        const auto sol2 = ne2.solve();
        REQUIRE(sol2.has_value());
        const double a = jc->formal[6 * kN + 6], b = sol2->cofactor[6 * kN + 6];
        std::printf("JOINT 506control columns of c0 and c2 exchanged: Q(c0, c0) %.9e against the correct %.9e (relative difference %.3e, must be >= 1e-3)\n", b, a, std::fabs(b - a) / a);
        CHECK(std::fabs(b - a) / a >= 1.0e-3);   // NOT-A-UNIT-CROSSING: registered "1e-3" relative
    }
}
