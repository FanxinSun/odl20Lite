// first_step_tests.cpp — the first-step cap of variational.cpp (L7 step 3, the Committee's ruling of 2026-11-01, option A; SPEC-integrators INTG-Q-001's note).
//
// THE CRASH, WITHOUT SRP: a force that REFUSES inside the Earth (the shape of SRP's shadow model, SHDW-F-002) ends a propagation call if a stage of the integrator's first trial step lands there; `integrate` starts at
// span/100, a large part of an orbit across a long span.  The tests register (before they ran): T1 a two-day call at 720 km must propagate; T2 the same at 200 km and 1000 km over three days; T3 an UNBOUND start is not
// capped and propagates as before.  That the cap changes nothing it does not act on is pinned by the whole of the existing suites passing unchanged (every call with span/100 <= 0.21 T is made exactly as before).

#include <catch2/catch_test_macros.hpp>

#include <odl/dynamics/variational.hpp>

#include <cmath>
#include <memory>
#include <vector>

using namespace odl;
using namespace odl::dyn;
using frames::Frame;

namespace {

constexpr double kMu = 3.986004415e14;          // m^3 s^-2
constexpr double kSurface = 6378137.0;          // m

/// Two-body that REFUSES (as SRP's shadow model does, SHDW-F-002) inside the Earth's radius.
class TwoBodyKeepOut final : public Force {
public:
    ForceId id() const override { return ForceId{"two-body-keep-out"}; }
    const std::vector<ParameterId>& consumes() const override { return none_; }

    odl::Result<ForceEvaluation, DynError>
    accel(const odl::time::Epoch&, const frames::Position<Frame::GCRS>& r_m, const Vec3&, const ParameterSet&, const ParameterRegistry& reg) const override {
        const Vec3 r = r_m.metres();
        const double r2 = r.x * r.x + r.y * r.y + r.z * r.z;
        const double rn = std::sqrt(r2);
        if (rn < kSurface) return odl::err(DynError{"TST-F-001", "the satellite is inside the keep-out body"});
        const double r3 = r2 * rn, r5 = r3 * r2;
        const Vec3 a{-kMu * r.x / r3, -kMu * r.y / r3, -kMu * r.z / r3};
        const double rv[3] = {r.x, r.y, r.z};
        Mat3 dadr{};
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j) dadr.r[i][j] = -kMu * ((i == j ? 1.0 / r3 : 0.0) - 3.0 * rv[i] * rv[j] / r5);
        return ForceEvaluation{frames::Acceleration<Frame::GCRS>{a}, StateJacobian::no_velocity_dependence(dadr, 0.0), ParameterJacobian{reg}};
    }

private:
    std::vector<ParameterId> none_{};
};

struct Rig {
    ParameterRegistry reg;
    ForceSet forces;
    ParameterSet params;
    Rig() : reg(), forces(reg), params(reg) { REQUIRE(forces.add(std::make_shared<TwoBodyKeepOut>()).has_value()); }
};

frames::State<Frame::GCRS> circular(double altitude_km, double speed_factor = 1.0) {
    const double r_km = 6378.137 + altitude_km;
    return frames::State<Frame::GCRS>(*odl::time::Epoch::from_gps_week(2000, 0.0), Vec3{r_km, 0.0, 0.0}, Vec3{0.0, speed_factor * std::sqrt(kMu / (r_km * 1e3)) / 1e3, 0.0});
}

}  // namespace

TEST_CASE("DYN-A-040  T1: a two-day call across a keep-out body propagates (the first trial step is capped)", "[first_step]") {
    Rig rig;
    std::size_t ran = 0;
    for (double span : {172800.0, 129600.0, 200000.0}) {
        auto r = propagate(rig.forces, rig.params, circular(720.0), span, 1e-9);
        INFO("span " << span);
        REQUIRE(r.has_value());
        const double rn = std::sqrt(r->position().x * r->position().x + r->position().y * r->position().y + r->position().z * r->position().z) * 1e3;
        CHECK(std::abs(rn - (kSurface + 720e3 - 0.863e3)) < 5e3);       // still a circle of the same radius (6378.137 km Earth in this rig)
        ++ran;
    }
    CHECK(ran == 3);
}

TEST_CASE("DYN-A-041  T2: three days at 200 km and at 1000 km, also with the STM", "[first_step]") {
    Rig rig;
    std::size_t ran = 0;
    for (double alt : {200.0, 400.0, 1000.0}) {
        auto r = propagate(rig.forces, rig.params, circular(alt), 259200.0, 1e-9);
        INFO("altitude " << alt);
        REQUIRE(r.has_value());
        auto s = propagate_with_stm(rig.forces, rig.params, circular(alt), 259200.0, 1e-9);
        REQUIRE(s.has_value());
        ++ran;
    }
    CHECK(ran == 3);
}

TEST_CASE("DYN-A-042  T3: an unbound start is not capped and propagates as before", "[first_step]") {
    Rig rig;
    // 1.5 times the circular speed at 1000 km is hyperbolic (above the escape speed sqrt(2) v_c): no period, no cap
    auto r = propagate(rig.forces, rig.params, circular(1000.0, 1.5), 3000.0, 1e-9);
    REQUIRE(r.has_value());
    const double rn_km = std::sqrt(r->position().x * r->position().x + r->position().y * r->position().y + r->position().z * r->position().z);
    CHECK(rn_km > 6378.137 + 1000.0);                                      // it moves outward (the RADIUS: the start is the pericentre and the path bends round the Earth)
}
