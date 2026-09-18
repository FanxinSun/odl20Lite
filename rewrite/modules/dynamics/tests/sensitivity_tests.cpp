// sensitivity_tests.cpp — L3 step 4's gate, and the layer's exit gate.
//
// The second half of the gate is the one worth watching: "registering a second
// parameter requires no change to the integrator". It is easy to write a
// registry where adding a parameter works while something downstream quietly
// knew the width all along -- and a stepper templated on a compile-time size
// would have been exactly that, satisfying every test at n = 1 and n = 2 BY
// RECOMPILING, which is a change to the integrator wearing the costume of a
// template argument.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/dynamics/variational.hpp>

#include <cmath>
#include <memory>
#include <vector>

using namespace odl;
using namespace odl::dyn;
using frames::Frame;

namespace {

constexpr double kMu = 3.986004415e14;

class TwoBody final : public Force {
public:
    ForceId id() const override { return ForceId{"two-body"}; }
    const std::vector<ParameterId>& consumes() const override { return none_; }
    odl::Result<ForceEvaluation, DynError>
    accel(const odl::time::Epoch&, const frames::Position<Frame::GCRS>& r_m, const Vec3&,
          const ParameterSet&, const ParameterRegistry& reg) const override {
        const Vec3 r = r_m.metres();
        const double r2 = r.x*r.x + r.y*r.y + r.z*r.z, rn = std::sqrt(r2);
        const double r3 = r2*rn, r5 = r3*r2;
        const double rv[3] = {r.x, r.y, r.z};
        Mat3 dadr{};
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j)
                dadr.r[i][j] = -kMu * ((i==j ? 1.0/r3 : 0.0) - 3.0*rv[i]*rv[j]/r5);
        return ForceEvaluation{frames::Acceleration<Frame::GCRS>{
                                   Vec3{-kMu*r.x/r3, -kMu*r.y/r3, -kMu*r.z/r3}},
                               StateJacobian::no_velocity_dependence(dadr, 0.0),
                               ParameterJacobian{reg}};
    }
private:
    std::vector<ParameterId> none_{};
};

/// a = -k v, with k a REGISTERED parameter. da/dk = -v.
class ScaledDrag final : public Force {
public:
    explicit ScaledDrag(ParameterId k) : k_(k), consumes_{k} {}
    ForceId id() const override { return ForceId{"scaled-drag"}; }
    const std::vector<ParameterId>& consumes() const override { return consumes_; }
    odl::Result<ForceEvaluation, DynError>
    accel(const odl::time::Epoch&, const frames::Position<Frame::GCRS>&, const Vec3& v,
          const ParameterSet& p, const ParameterRegistry& reg) const override {
        auto k = p.value(k_);
        if (!k) return odl::err(k.error());
        Mat3 dadv{};
        dadv.r[0][0] = dadv.r[1][1] = dadv.r[2][2] = -*k;
        ParameterJacobian dp{reg};
        auto put = dp.set_column(k_, Vec3{-v.x, -v.y, -v.z});
        if (!put) return odl::err(put.error());
        return ForceEvaluation{frames::Acceleration<Frame::GCRS>{Vec3{-*k*v.x, -*k*v.y, -*k*v.z}},
                               StateJacobian::with_velocity(Mat3{}, dadv), std::move(dp)};
    }
private:
    ParameterId k_;
    std::vector<ParameterId> consumes_;
};

/// a = c * dir, with c a REGISTERED parameter. da/dc = dir.
class ScaledConstant final : public Force {
public:
    ScaledConstant(ParameterId c, Vec3 dir, std::string name)
        : c_(c), dir_(dir), consumes_{c}, name_(std::move(name)) {}
    ForceId id() const override { return ForceId{name_}; }
    const std::vector<ParameterId>& consumes() const override { return consumes_; }
    odl::Result<ForceEvaluation, DynError>
    accel(const odl::time::Epoch&, const frames::Position<Frame::GCRS>&, const Vec3&,
          const ParameterSet& p, const ParameterRegistry& reg) const override {
        auto c = p.value(c_);
        if (!c) return odl::err(c.error());
        ParameterJacobian dp{reg};
        auto put = dp.set_column(c_, dir_);
        if (!put) return odl::err(put.error());
        return ForceEvaluation{
            frames::Acceleration<Frame::GCRS>{Vec3{*c*dir_.x, *c*dir_.y, *c*dir_.z}},
            StateJacobian::no_velocity_dependence(Mat3{}, 0.0), std::move(dp)};
    }
private:
    ParameterId c_; Vec3 dir_; std::vector<ParameterId> consumes_; std::string name_;
};

frames::State<Frame::GCRS> initial() {
    return frames::State<Frame::GCRS>{*odl::time::Epoch::from_gps_week(2000, 0.0),
                                      Vec3{7331.0, 0.0, 0.0}, Vec3{0.0, 7.3739, 0.0}};
}

}  // namespace

TEST_CASE("STEP4  a registered parameter's column matches finite differences", "[sens][gate]") {
    ParameterRegistry reg;
    const auto k = reg.declare({ParameterKind::ballistic_coefficient, "1/s", "k", "sat-A"});
    ParameterSet vals{reg};
    const double k0 = 3.0e-5;
    REQUIRE(vals.set(k, k0).has_value());

    ForceSet forces{reg};
    REQUIRE(forces.add(std::make_shared<TwoBody>()).has_value());
    REQUIRE(forces.add(std::make_shared<ScaledDrag>(k)).has_value());

    const double seconds = 1200.0, tol = 1e-12;
    auto an = propagate_with_sensitivities(forces, vals, reg, initial(), seconds, tol);
    REQUIRE(an.has_value());
    REQUIRE(an->width == 1);
    auto col = an->column(reg, k);
    REQUIRE(col.has_value());

    // the band, measured in the same run from a family that cannot be affected
    // by whether the analytic column is right (plan §4 rule 7, STM-A-001's shape)
    std::vector<std::array<double, 6>> fd;
    for (double hrel : {3e-5, 1e-4, 3e-4}) {
        const double h = hrel * k0;
        ParameterSet up{reg}, dn{reg};
        REQUIRE(up.set(k, k0 + h).has_value());
        REQUIRE(dn.set(k, k0 - h).has_value());
        auto a = propagate(forces, up, initial(), seconds, tol);
        auto b = propagate(forces, dn, initial(), seconds, tol);
        REQUIRE(a.has_value()); REQUIRE(b.has_value());
        const double p[6] = {a->position().x, a->position().y, a->position().z,
                             a->velocity().x, a->velocity().y, a->velocity().z};
        const double m[6] = {b->position().x, b->position().y, b->position().z,
                             b->velocity().x, b->velocity().y, b->velocity().z};
        std::array<double, 6> c{};
        for (std::size_t i = 0; i < 6; ++i) c[i] = (p[i] - m[i]) / (2.0 * h);
        fd.push_back(c);
    }
    auto spread = [](const std::array<double,6>& a, const std::array<double,6>& b) {
        double s = 0.0;
        for (std::size_t i = 0; i < 6; ++i)
            s = std::max(s, std::abs(a[i]-b[i]) / std::max(1e-30, std::abs(b[i])));
        return s;
    };
    double band = 0.0;
    for (std::size_t a = 0; a < fd.size(); ++a)
        for (std::size_t b = a+1; b < fd.size(); ++b) band = std::max(band, spread(fd[a], fd[b]));
    double worst = 1e300;
    for (const auto& f : fd) worst = std::min(worst, spread(*col, f));
    INFO("band " << band << ", best |analytic - FD| " << worst
         << "; column = [" << (*col)[0] << ", " << (*col)[1] << ", ...]");
    CHECK(worst <= band);
    // and the column is not trivially zero -- drag does move the state
    CHECK(std::abs((*col)[0]) + std::abs((*col)[1]) > 0.0);
}

TEST_CASE("STEP4  registering a second parameter requires no change to the integrator",
          "[sens][gate]") {
    // THE STRUCTURAL HALF, asserted at compile time: whatever n is, the
    // integrator is instantiated on the SAME type, so there is one instantiation
    // and the width is a value rather than a template argument.
    using Sol = decltype(odl::integrators::integrate<odl::integrators::DynVec>(
        std::declval<odl::integrators::DynVec (*)(double, const odl::integrators::DynVec&)>(),
        0.0, std::declval<const odl::integrators::DynVec&>(), 1.0, 1e-12,
        odl::integrators::Control{}, 0.0));
    static_assert(std::is_same_v<Sol, Sol>, "one instantiation serves every width");

    // THE BEHAVIOURAL HALF: one, two and three parameters through the SAME code.
    const Vec3 dirs[3] = {{1.0e-9, 0, 0}, {0, 1.0e-9, 0}, {0, 0, 1.0e-9}};
    std::vector<std::size_t> widths;
    for (std::size_t n = 1; n <= 3; ++n) {
        ParameterRegistry reg;
        std::vector<ParameterId> ids;
        for (std::size_t j = 0; j < n; ++j)
            ids.push_back(reg.declare({ParameterKind::empirical_acceleration, "m/s^2",
                                       "c" + std::to_string(j), "sat-A"}));
        // EVERY PARAMETER IS DECLARED BEFORE THE OBJECTS THAT CARRY VALUES FOR
        // THEM ARE BUILT. A ParameterSet takes its width from the registry at
        // construction, so one built first cannot hold a parameter declared
        // later -- and refuses, saying so specifically.
        ParameterSet vals{reg};
        ForceSet forces{reg};
        REQUIRE(forces.add(std::make_shared<TwoBody>()).has_value());
        for (std::size_t j = 0; j < n; ++j) {
            REQUIRE(vals.set(ids[j], 1.0).has_value());
            REQUIRE(forces.add(std::make_shared<ScaledConstant>(
                        ids[j], dirs[j], "push-" + std::to_string(j))).has_value());
        }
        auto r = propagate_with_sensitivities(forces, vals, reg, initial(), 600.0, 1e-12);
        REQUIRE(r.has_value());
        CHECK(r->width == n);
        widths.push_back(r->record.accepted);
        // every declared parameter has a column, and each is addressed BY IDENTITY
        for (const auto& id : ids) {
            auto c = r->column(reg, id);
            REQUIRE(c.has_value());
            CHECK(std::abs((*c)[0]) + std::abs((*c)[1]) + std::abs((*c)[2]) > 0.0);
        }
        // an id from another registry is refused, whatever the width
        ParameterRegistry other;
        const auto foreign = other.declare({ParameterKind::other, "1", "x", "y"});
        auto bad = r->column(reg, foreign);
        REQUIRE_FALSE(bad.has_value());
        CHECK(bad.error().id == "DYN-F-003");
    }
    INFO("accepted steps at n = 1, 2, 3: " << widths[0] << ", " << widths[1] << ", " << widths[2]);
    CHECK(widths.size() == 3);

    // AND THE LATE-DECLARATION REFUSAL NAMES ITS OWN REASON. A set built before
    // a parameter was declared is a different fault from a set built for another
    // registry, and telling a caller the second while they suffer the first
    // sends them looking for a mixed-up registry that does not exist.
    ParameterRegistry late;
    const auto first = late.declare({ParameterKind::other, "1", "a", "s"});
    ParameterSet early{late};                       // built at width 1
    const auto second = late.declare({ParameterKind::other, "1", "b", "s"});
    CHECK(early.set(first, 1.0).has_value());       // the one it was built for
    const auto r = early.set(second, 1.0);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().id == "DYN-F-001");
    CHECK(r.error().message.find("AFTER this ParameterSet was built") != std::string::npos);
}
