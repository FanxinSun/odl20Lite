// scaled_force_tests.cpp — SPEC-estimation.md EST-A-903 (v0.7, registered 2026-11-27 BEFORE this file existed): the SRP-scale carrier ScaledForce (EST-R-901, EST-F-902).
//
// The construction is that of modules/dynamics/tests/sensitivity_tests.cpp: a two-body force (with a velocity-dependent drag term whose coefficient `k` is a REGISTERED parameter, so that the wrapped force has a
// velocity Jacobian and a parameter column of its own), wrapped with a registered `c`.  Every tolerance is the registered one: `==` for (a) and (b); for (c) the band is measured in the same run (the difference of the
// central differences at h and h/2; the test's reading of "four times their own difference" is the max-norm over the six components of the column).
#include <catch2/catch_test_macros.hpp>

#include <odl/dynamics/force_set.hpp>
#include <odl/dynamics/variational.hpp>
#include <odl/forcemodel/scaled_force.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <vector>

using namespace odl;
using namespace odl::dyn;
using frames::Frame;

namespace {

constexpr double kMu = 3.986004415e14;

/// a = -mu r/|r|^3 - k v, k registered; velocity Jacobian -k I, position Jacobian the two-body gradient, column of k = -v.  `declared_bound` is the neglected-velocity bound it declares when `with_vel` is false.
class Inner final : public Force {
public:
    Inner(ParameterId k, bool with_vel, double declared_bound) : k_(k), consumes_{k}, with_vel_(with_vel), bound_(declared_bound) {}
    ForceId id() const override { return ForceId{"inner-test"}; }
    const std::vector<ParameterId>& consumes() const override { return consumes_; }
    odl::Result<ForceEvaluation, DynError> accel(const odl::time::Epoch&, const frames::Position<Frame::GCRS>& r_m, const Vec3& v, const ParameterSet& p, const ParameterRegistry& reg) const override {
        auto k = p.value(k_);
        if (!k) return odl::err(k.error());
        const Vec3 r = r_m.metres();
        const double r2 = r.x * r.x + r.y * r.y + r.z * r.z, rn = std::sqrt(r2), r3 = r2 * rn, r5 = r3 * r2;
        const double rv[3] = {r.x, r.y, r.z};
        Mat3 dadr{}, dadv{};
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j) dadr.r[i][j] = -kMu * ((i == j ? 1.0 / r3 : 0.0) - 3.0 * rv[i] * rv[j] / r5);
        dadv.r[0][0] = dadv.r[1][1] = dadv.r[2][2] = -*k;
        ParameterJacobian dp{reg};
        if (auto put = dp.set_column(k_, Vec3{-v.x, -v.y, -v.z}); !put) return odl::err(put.error());
        return ForceEvaluation{frames::Acceleration<Frame::GCRS>{Vec3{-kMu * r.x / r3 - *k * v.x, -kMu * r.y / r3 - *k * v.y, -kMu * r.z / r3 - *k * v.z}},
                               with_vel_ ? StateJacobian::with_velocity(dadr, dadv) : StateJacobian::no_velocity_dependence(dadr, bound_), std::move(dp)};
    }
private:
    ParameterId k_;
    std::vector<ParameterId> consumes_;
    bool with_vel_;
    double bound_;
};

/// THE CONTROL: the same decorator with a defect — d(a)/d(r) is passed through UNSCALED (times 1 instead of times c).
class BadScaled final : public Force {
public:
    BadScaled(std::shared_ptr<const Force> inner, ParameterId c) : inner_(std::move(inner)), c_(c) {
        consumes_.push_back(c);
        for (const auto& p : inner_->consumes()) consumes_.push_back(p);
    }
    ForceId id() const override { return ForceId{"bad-scaled"}; }
    const std::vector<ParameterId>& consumes() const override { return consumes_; }
    odl::Result<ForceEvaluation, DynError> accel(const odl::time::Epoch& t, const frames::Position<Frame::GCRS>& r, const Vec3& v, const ParameterSet& p, const ParameterRegistry& reg) const override {
        auto ev = inner_->accel(t, r, v, p, reg);
        if (!ev) return odl::err(ev.error());
        const double c = *p.value(c_);
        const Vec3 a = ev->acceleration.metres_per_second_squared();
        const auto scale = [c](const Mat3& m) { Mat3 o; for (std::size_t i = 0; i < 3; ++i) for (std::size_t j = 0; j < 3; ++j) o.r[i][j] = c * m.r[i][j]; return o; };
        ParameterJacobian dp{reg};
        for (const auto& q : inner_->consumes()) { const auto col = *ev->d_parameters.column(q); (void)dp.set_column(q, Vec3{c * col.x, c * col.y, c * col.z}); }
        (void)dp.set_column(c_, a);
        return ForceEvaluation{frames::Acceleration<Frame::GCRS>{Vec3{c * a.x, c * a.y, c * a.z}},
                               StateJacobian::with_velocity(ev->d_state.d_position() /* the defect: not scaled */, scale(*ev->d_state.d_velocity())), std::move(dp)};
    }
private:
    std::shared_ptr<const Force> inner_;
    ParameterId c_;
    std::vector<ParameterId> consumes_;
};

frames::State<Frame::GCRS> initial() {
    return frames::State<Frame::GCRS>{*odl::time::Epoch::from_gps_week(2000, 0.0), Vec3{7331.0, 0.0, 0.0}, Vec3{0.0, 7.3739, 0.0}};
}

bool same(const Mat3& a, const Mat3& b) {
    for (std::size_t i = 0; i < 3; ++i) for (std::size_t j = 0; j < 3; ++j) if (!(a.r[i][j] == b.r[i][j])) return false;
    return true;
}
Mat3 times(double c, const Mat3& m) { Mat3 o; for (std::size_t i = 0; i < 3; ++i) for (std::size_t j = 0; j < 3; ++j) o.r[i][j] = c * m.r[i][j]; return o; }

double max_norm_diff(const std::array<double, 6>& a, const std::array<double, 6>& b) {
    double m = 0.0;
    for (std::size_t i = 0; i < 6; ++i) m = std::max(m, std::fabs(a[i] - b[i]));
    return m;
}

}  // namespace

TEST_CASE("EST-A-903 the SRP-scale carrier ScaledForce", "[est][scaled]") {
    ParameterRegistry reg;
    const auto c = reg.declare({ParameterKind::srp_scale, "1", "srp_scale", "sat-A"});
    const auto k = reg.declare({ParameterKind::ballistic_coefficient, "1/s", "k", "sat-A"});
    const double k0 = 3.0e-5;
    const odl::time::Epoch t0 = *odl::time::Epoch::from_gps_week(2000, 0.0);
    const frames::Position<Frame::GCRS> pos{Vec3{7331000.0, 1200.0, -800.0}};
    const Vec3 vel{12.0, 7373.9, 35.0};

    auto set_values = [&](double cv) {
        ParameterSet vals{reg};
        REQUIRE(vals.set(c, cv).has_value());
        REQUIRE(vals.set(k, k0).has_value());
        return vals;
    };

    auto inner = std::make_shared<Inner>(k, true, 0.0);
    auto made = odl::forcemodel::ScaledForce::create(inner, c, reg);
    REQUIRE(made.has_value());
    const auto& dec = **made;

    // (a) c = 1: the decorated force IS the inner one, bit for bit.
    {
        const auto vals = set_values(1.0);
        const auto e0 = inner->accel(t0, pos, vel, vals, reg);
        const auto e1 = dec.accel(t0, pos, vel, vals, reg);
        REQUIRE(e0.has_value()); REQUIRE(e1.has_value());
        const Vec3 a0 = e0->acceleration.metres_per_second_squared(), a1 = e1->acceleration.metres_per_second_squared();
        CHECK((a0.x == a1.x && a0.y == a1.y && a0.z == a1.z));
        CHECK(same(e0->d_state.d_position(), e1->d_state.d_position()));
        REQUIRE(e1->d_state.d_velocity().has_value());
        CHECK(same(*e0->d_state.d_velocity(), *e1->d_state.d_velocity()));
        const Vec3 ck0 = *e0->d_parameters.column(k), ck1 = *e1->d_parameters.column(k);
        CHECK((ck0.x == ck1.x && ck0.y == ck1.y && ck0.z == ck1.z));
    }

    // (b) c = 2.5: every output is 2.5 times the inner's, ==.
    {
        const double cv = 2.5;
        const auto vals = set_values(cv);
        const auto e0 = inner->accel(t0, pos, vel, vals, reg);
        const auto e1 = dec.accel(t0, pos, vel, vals, reg);
        REQUIRE(e0.has_value()); REQUIRE(e1.has_value());
        const Vec3 a0 = e0->acceleration.metres_per_second_squared(), a1 = e1->acceleration.metres_per_second_squared();
        CHECK((a1.x == cv * a0.x && a1.y == cv * a0.y && a1.z == cv * a0.z));
        CHECK(same(e1->d_state.d_position(), times(cv, e0->d_state.d_position())));
        REQUIRE(e1->d_state.d_velocity().has_value());
        CHECK(same(*e1->d_state.d_velocity(), times(cv, *e0->d_state.d_velocity())));
        const Vec3 ck0 = *e0->d_parameters.column(k), ck1 = *e1->d_parameters.column(k);
        CHECK((ck1.x == cv * ck0.x && ck1.y == cv * ck0.y && ck1.z == cv * ck0.z));
        // the column of c is the inner acceleration at c = 1 (the inner force does not read c)
        const Vec3 cc = *e1->d_parameters.column(c);
        CHECK((cc.x == a0.x && cc.y == a0.y && cc.z == a0.z));
        // consumes(): c first, then the inner force's
        REQUIRE(dec.consumes().size() == 2);
        CHECK(dec.consumes()[0] == c);
        CHECK(dec.consumes()[1] == k);
        CHECK(dec.id().name == "inner-test.scaled");

        // the declared neglected-velocity bound is scaled by |c| (an inner force with no velocity dependence and a bound of 0.125 s^-1)
        auto inner_nv = std::make_shared<Inner>(k, false, 0.125);
        auto dec_nv = odl::forcemodel::ScaledForce::create(inner_nv, c, reg);
        REQUIRE(dec_nv.has_value());
        for (double cn : {2.5, -2.5}) {
            const auto vn = set_values(cn);
            const auto en = (*dec_nv)->accel(t0, pos, vel, vn, reg);
            REQUIRE(en.has_value());
            CHECK_FALSE(en->d_state.d_velocity().has_value());
            CHECK(en->d_state.neglected_velocity_bound_per_s() == std::fabs(cn) * 0.125);
        }
    }

    // (c) the column of c of propagate_with_sensitivities against the central difference of propagate in c at h and h/2.
    const double seconds = 1200.0, tol = 1e-12, c0 = 1.7, h = 1e-3;
    auto propagate_at = [&](const Force& f, double cv) {
        ForceSet fs{reg};
        REQUIRE(fs.add(std::shared_ptr<const Force>(std::shared_ptr<const Force>{}, &f)).has_value());
        const auto vals = set_values(cv);
        auto r = propagate(fs, vals, initial(), seconds, tol);
        REQUIRE(r.has_value());
        return std::array<double, 6>{r->position().x, r->position().y, r->position().z, r->velocity().x, r->velocity().y, r->velocity().z};   // km and km/s: the unit of SensitivitySolution::column (the state's own, as in sensitivity_tests.cpp)
    };
    auto central = [&](const Force& f, double hh) {
        const auto up = propagate_at(f, c0 + hh), dn = propagate_at(f, c0 - hh);
        std::array<double, 6> d{};
        for (std::size_t i = 0; i < 6; ++i) d[i] = (up[i] - dn[i]) / (2.0 * hh);
        return d;
    };
    auto analytic = [&](const std::shared_ptr<const Force>& f) {
        ForceSet fs{reg};
        REQUIRE(fs.add(f).has_value());
        const auto vals = set_values(c0);
        auto sol = propagate_with_sensitivities(fs, vals, reg, initial(), seconds, tol);
        REQUIRE(sol.has_value());
        auto col = sol->column(reg, c);
        REQUIRE(col.has_value());
        return *col;
    };
    const auto d1 = central(dec, h), d2 = central(dec, h / 2.0);
    const double band = 4.0 * max_norm_diff(d1, d2);
    const auto an = analytic(*made);
    const double miss_good = max_norm_diff(an, d2);
    std::printf("EST-A-903 (c): band = 4*|D(h)-D(h/2)|_inf = %.17g  |analytic - D(h/2)|_inf = %.17g\n", band, miss_good);
    CHECK(band > 0.0);
    CHECK(miss_good <= band);

    // control (must fail): the decorator that scales d(a)/d(r) by 1 instead of c misses (c) by at least 1e3 times the band
    const auto an_bad = analytic(std::make_shared<BadScaled>(inner, c));
    const double miss_bad = max_norm_diff(an_bad, d2);
    std::printf("EST-A-903 control: |analytic(bad) - D(h/2)|_inf = %.17g  = %.17g bands\n", miss_bad, miss_bad / band);
    CHECK(miss_bad >= 1e3 * band);

    // (d) EST-F-902: a non-finite c, and a wrapped force that already consumes c; and the null force and a foreign parameter.
    {
        const auto vals = set_values(std::numeric_limits<double>::quiet_NaN());
        const auto bad = dec.accel(t0, pos, vel, vals, reg);
        REQUIRE_FALSE(bad.has_value());
        CHECK(bad.error().id == "EST-F-902");
        const auto vinf = set_values(std::numeric_limits<double>::infinity());
        CHECK_FALSE(dec.accel(t0, pos, vel, vinf, reg).has_value());

        auto twice = odl::forcemodel::ScaledForce::create(inner, k, reg);   // the inner force consumes k already
        REQUIRE_FALSE(twice.has_value());
        CHECK(twice.error().id == "EST-F-902");
        auto none = odl::forcemodel::ScaledForce::create(nullptr, c, reg);
        REQUIRE_FALSE(none.has_value());
        CHECK(none.error().id == "EST-F-902");
        ParameterRegistry other;
        const auto foreign = other.declare({ParameterKind::srp_scale, "1", "x", "y"});
        auto fo = odl::forcemodel::ScaledForce::create(inner, foreign, reg);
        REQUIRE_FALSE(fo.has_value());
        CHECK(fo.error().id == "EST-F-902");
    }
}
