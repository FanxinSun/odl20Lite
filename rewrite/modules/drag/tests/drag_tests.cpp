// drag_tests.cpp — SPEC-drag §8, L4 step 4's gate.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/drag/drag.hpp>
#include <odl/drag/geodetic.hpp>
#include <odl/core/units.hpp>
#include <odl/dynamics/force_set.hpp>
#include <odl/dynamics/variational.hpp>
#include <odl/eop/record.hpp>
#include <odl/frames/transform.hpp>
#include <odl/time/leap_table.hpp>

#include <array>
#include <cmath>
#include <fstream>
#include <memory>
#include <numbers>
#include <sstream>
#include <vector>

using namespace odl;
using namespace odl::drag;
using namespace odl::dyn;
using odl::frames::Frame;

namespace {

constexpr double kMu = 3.986004415e14;   // m^3 s^-2, l2_floors.cpp's own value

/// A trivial two-body force, the same shape stm_tests.cpp's own TwoBody --
/// copied rather than shared because that one is private to its own test
/// file, and this module's test for Liouville needs a gravity term to
/// integrate alongside drag exactly as any real propagation would.
class TwoBody final : public Force {
public:
    ForceId id() const override { return ForceId{"two-body"}; }
    const std::vector<ParameterId>& consumes() const override { return none_; }
    odl::Result<ForceEvaluation, DynError>
    accel(const odl::time::Epoch&, const frames::Position<Frame::GCRS>& r_m, const Vec3&,
         const ParameterSet&, const ParameterRegistry& reg) const override {
        const Vec3 r = r_m.metres();
        const double r2 = r.x * r.x + r.y * r.y + r.z * r.z;
        const double rn = std::sqrt(r2);
        const double r3 = r2 * rn, r5 = r3 * r2;
        const Vec3 a{-kMu * r.x / r3, -kMu * r.y / r3, -kMu * r.z / r3};
        const double rv[3] = {r.x, r.y, r.z};
        Mat3 dadr{};
        for (std::size_t i = 0; i < 3; ++i)
            for (std::size_t j = 0; j < 3; ++j)
                dadr.r[i][j] = -kMu * ((i == j ? 1.0 / r3 : 0.0) - 3.0 * rv[i] * rv[j] / r5);
        return ForceEvaluation{frames::Acceleration<Frame::GCRS>{a},
                               StateJacobian::no_velocity_dependence(dadr, 0.0),
                               ParameterJacobian{reg}};
    }
private:
    std::vector<ParameterId> none_{};
};

std::string slurp(const char* path) {
    std::ifstream f(path);
    REQUIRE(f.is_open());
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

odl::eop::EopRecord zero_eop() {
    odl::eop::EopRecord eop;
    eop.subdaily_applied = true;   // FRAME-F-003's own precondition; the values
                                    // being zero is a stated simplification, not
                                    // an oversight -- drag's own physics (density,
                                    // co-rotation direction) is insensitive to the
                                    // sub-arcsecond pole position this omits.
    return eop;
}

odl::time::Epoch epoch_at(int year, int month, int day, double hour = 0.0) {
    odl::time::Calendar c{year, month, day, static_cast<int>(hour), 0, 0.0};
    auto e = odl::time::Epoch::from_calendar(odl::time::TimeScale::UTC, c, leaps());
    REQUIRE(e.has_value());
    return *e;
}

atmosphere::SpaceWeather quiet_sw(atmosphere::Verification v) {
    atmosphere::SpaceWeather sw;
    sw.f107_previous_day = 150.0;
    sw.f107a_centred81 = 150.0;
    sw.daily.ap = 4.0;
    sw.snapshot_id = "test-stated";
    sw.snapshot_sha256 = "test-stated, not a real snapshot";
    sw.verification = v;
    return sw;
}

/// A circular equatorial state at a stated radius.
frames::State<Frame::GCRS> circular_state(odl::time::Epoch when, double r_m) {
    const double v = std::sqrt(kMu / r_m);
    return frames::State<Frame::GCRS>{when, odl::km_from_metres(Vec3{r_m, 0.0, 0.0}),
                                      odl::km_from_metres(Vec3{0.0, v, 0.0})};
}

/// LEO-ish circular state: r = 7331 km equatorial, this project's own
/// standard test radius throughout (SHDW, L2's floors, STM's own Setup).
/// Fine for wiring/refusal/sensitivity checks, where the drag MAGNITUDE is
/// not the point -- but at this radius (~953 km altitude) the atmosphere is
/// thin enough that DRAG-A-005's determinant test needs a lower, denser
/// orbit of its own (below) to see a decay worth asserting on.
frames::State<Frame::GCRS> leo_state(odl::time::Epoch when) {
    return circular_state(when, 7331.0e3);
}

}  // namespace

// ---------------------------------------------------------------------------
// Geodetic conversion (a utility this module needed, not drag physics itself)

TEST_CASE("DRAG-A-001  geodetic round trip, and the closed-form equator/pole cases",
          "[drag][gate]") {
    int checked = 0;
    double worst_rad = 0.0, worst_alt = 0.0;
    for (auto [lat_deg, lon_deg, alt_m] :
        {std::array{0.0, 0.0, 500000.0}, std::array{45.0, 90.0, 800000.0},
         std::array{-60.0, -120.0, 300000.0}, std::array{89.9, 30.0, 400000.0},
         std::array{-89.9, -170.0, 700000.0}}) {
        const Geodetic g{lat_deg * std::numbers::pi / 180.0, lon_deg * std::numbers::pi / 180.0, alt_m};
        const Vec3 ecef = geodetic_to_itrs(g);
        const Geodetic back = itrs_to_geodetic(ecef);
        worst_rad = std::max({worst_rad, std::abs(back.latitude_rad - g.latitude_rad),
                              std::abs(back.longitude_rad - g.longitude_rad)});
        worst_alt = std::max(worst_alt, std::abs(back.altitude_m - g.altitude_m));
        ++checked;
    }
    INFO("checked " << checked << " round trips; worst angle error " << worst_rad
         << " rad, worst altitude error " << worst_alt << " m");
    CHECK(checked == 5);
    CHECK(worst_rad < 1.0e-12);
    CHECK(worst_alt < 1.0e-6);

    // the closed-form cases: equator at zero altitude is exactly (a,0,0);
    // the pole at zero altitude is exactly (0,0,b) -- identities the
    // ellipsoid's own definition gives, not a second implementation's output
    const Vec3 eq = geodetic_to_itrs(Geodetic{0.0, 0.0, 0.0});
    CHECK_THAT(eq.x, Catch::Matchers::WithinAbs(kWgs84SemiMajorM, 1.0e-6));
    CHECK_THAT(eq.y, Catch::Matchers::WithinAbs(0.0, 1.0e-6));
    CHECK_THAT(eq.z, Catch::Matchers::WithinAbs(0.0, 1.0e-6));
    const Vec3 pole = geodetic_to_itrs(Geodetic{std::numbers::pi / 2.0, 0.0, 0.0});
    CHECK_THAT(pole.z, Catch::Matchers::WithinAbs(kWgs84SemiMinorM, 1.0e-6));
    CHECK_THAT(std::hypot(pole.x, pole.y), Catch::Matchers::WithinAbs(0.0, 1.0e-6));
}

// ---------------------------------------------------------------------------
// The force law itself

TEST_CASE("DRAG-A-002  the force law: magnitude and direction against the closed form, "
          "parameters stated in the test", "[drag][gate]") {
    // A REALISTIC (Sengers et al. 2014 Table 4-INSPIRED, not read from any
    // library) C_D and a stated area/mass -- LEO, quiet space weather. This
    // is arithmetic-correctness verification: given a stated rho, C_D, A, m
    // and v_rel, does the code compute exactly what the closed form says.
    const auto when = epoch_at(2015, 6, 21, 12.0);
    const auto x0 = leo_state(when);
    const double c_d = 2.2;          // realistic sphere/satellite value, stated
    const double area = 10.0;        // m^2, stated
    const double mass = 500.0;       // kg, stated
    const auto sw = quiet_sw(atmosphere::Verification::verified_against_issuer);

    auto result = acceleration(when, odl::metres_from_km(x0.position()),
                               odl::metres_from_km(x0.velocity()), c_d, area, mass, sw,
                               zero_eop(), leaps());
    REQUIRE(result.has_value());

    // independently recompute rho at the same place, via the SAME atmosphere
    // call, and check the closed form directly -- an arithmetic check, not a
    // second physics derivation (the physics is re-derived in the header/spec)
    auto itrs = frames::to_itrs(x0, zero_eop(), leaps());
    REQUIRE(itrs.has_value());
    const Vec3 v_rel = odl::metres_from_km(itrs->velocity());
    const double speed = v_rel.norm();
    const auto geo = itrs_to_geodetic(odl::metres_from_km(itrs->position()));
    atmosphere::Place place;
    auto cal = when.calendar(odl::time::TimeScale::UTC, leaps());
    REQUIRE(cal.has_value());
    place.day_of_year = 172;   // 2015-06-21 -- stated independently as a sanity cross-check
    place.seconds_of_day = cal->hour * 3600.0 + cal->minute * 60.0 + cal->second;
    place.geodetic_latitude_deg = geo.latitude_rad * 180.0 / std::numbers::pi;
    place.longitude_deg = geo.longitude_rad * 180.0 / std::numbers::pi;
    place.altitude_km = geo.altitude_m / 1000.0;
    auto rho = atmosphere::for_drag(place, sw);
    REQUIRE(rho.has_value());

    const double expected_mag = 0.5 * rho->total_mass_kg_m3 * c_d * (area / mass) * speed * speed;
    // v_rel, and so expected_dir, live in ITRS (the co-rotating frame the
    // force law is built from); result->acceleration_m_s2 is GCRS
    // (DragResult's own stated convention). Rotated by the SAME M the
    // implementation uses (drag.cpp: M^T maps ITRS -> GCRS) before
    // comparing, or the two are different frames wearing the same units.
    auto rot = frames::gcrs_to_itrs(when, zero_eop(), leaps());
    REQUIRE(rot.has_value());
    const Vec3 expected_dir_itrs = (-1.0 / speed) * v_rel;   // opposes relative velocity
    const Vec3 expected_dir = rot->m.transpose().apply(expected_dir_itrs);

    INFO("rho = " << rho->total_mass_kg_m3 << " kg/m^3, speed = " << speed
         << " m/s, expected |a| = " << expected_mag << " m/s^2, got |a| = "
         << result->acceleration_m_s2.norm());
    CHECK_THAT(result->acceleration_m_s2.norm(), Catch::Matchers::WithinRel(expected_mag, 1.0e-9));
    const double dir_dev = (odl::Vec3{result->acceleration_m_s2.x / result->acceleration_m_s2.norm() -
                                      expected_dir.x,
                                      result->acceleration_m_s2.y / result->acceleration_m_s2.norm() -
                                      expected_dir.y,
                                      result->acceleration_m_s2.z / result->acceleration_m_s2.norm() -
                                      expected_dir.z})
                                .norm();
    CHECK(dir_dev < 1.0e-9);

    // sanity against Table 4 (Sengers et al. 2014): at LEO the speed ratio S
    // for atomic oxygen is O(6-8), where the sphere's free-molecular C_0 is
    // 2.0-2.4 -- the stated C_D=2.2 sits inside that published range, so this
    // test's own input is not physically absurd, though it is not itself the
    // published value being checked (SPEC-drag §4's rule-4 finding).
    CHECK(c_d > 2.0);
    CHECK(c_d < 2.5);
}

TEST_CASE("DRAG-A-003  drag opposes relative velocity, never assists it", "[drag][gate]") {
    const auto when = epoch_at(2015, 6, 21, 12.0);
    const auto x0 = leo_state(when);
    const auto sw = quiet_sw(atmosphere::Verification::verified_against_issuer);
    auto result = acceleration(when, odl::metres_from_km(x0.position()),
                               odl::metres_from_km(x0.velocity()), 2.2, 10.0, 500.0, sw,
                               zero_eop(), leaps());
    REQUIRE(result.has_value());
    auto itrs = frames::to_itrs(x0, zero_eop(), leaps());
    REQUIRE(itrs.has_value());
    const Vec3 v_rel = odl::metres_from_km(itrs->velocity());
    // dot product with v_rel (in GCRS this is NOT quite v_gcrs, but the sign
    // test is via the ITRS-frame quantity the force was actually built from)
    auto rot = frames::gcrs_to_itrs(when, zero_eop(), leaps());
    REQUIRE(rot.has_value());
    const Vec3 a_itrs = rot->m.apply(result->acceleration_m_s2);
    CHECK(a_itrs.dot(v_rel) < 0.0);
}

// ---------------------------------------------------------------------------
// C_D as a registered parameter, and its sensitivity against finite differences

TEST_CASE("DRAG-A-004  C_D is a registered parameter (DYN-F-002 fires when it is not set), "
          "and its Jacobian column matches a finite difference", "[drag][gate]") {
    ParameterRegistry reg;
    auto c_d_id = reg.declare(ParameterDeclaration{ParameterKind::drag_coefficient, "1",
                                                   "C_D", "test satellite"});
    const auto when = epoch_at(2015, 6, 21, 12.0);
    const auto x0 = leo_state(when);
    const auto sw = quiet_sw(atmosphere::Verification::verified_against_issuer);
    Drag drag(c_d_id, 10.0, 500.0, sw, zero_eop(), leaps());

    ParameterSet empty_params(reg);   // C_D never set
    auto refused = drag.accel(when, frames::Position<Frame::GCRS>{odl::metres_from_km(x0.position())},
                              odl::metres_from_km(x0.velocity()), empty_params, reg);
    REQUIRE_FALSE(refused.has_value());
    CHECK(refused.error().id == "DRAG-F-006");

    ParameterSet params(reg);
    const double c_d0 = 2.2;
    REQUIRE(params.set(c_d_id, c_d0).has_value());
    auto r0 = drag.accel(when, frames::Position<Frame::GCRS>{odl::metres_from_km(x0.position())},
                         odl::metres_from_km(x0.velocity()), params, reg);
    REQUIRE(r0.has_value());
    auto analytic = r0->d_parameters.column(c_d_id);
    REQUIRE(analytic.has_value());

    // finite difference: a(C_D) is LINEAR in C_D (density and v_rel do not
    // depend on it), so central and one-sided differences agree to rounding
    // regardless of step -- chosen here at 1e-4 relative, an unremarkable
    // step for a linear function, not a tuned one (rule 7 is about
    // comparator FAMILIES for NONLINEAR checks; a linear function has none
    // of the h-dependent bias that rule addresses).
    const double h = c_d0 * 1.0e-4;
    ParameterSet p_plus(reg), p_minus(reg);
    REQUIRE(p_plus.set(c_d_id, c_d0 + h).has_value());
    REQUIRE(p_minus.set(c_d_id, c_d0 - h).has_value());
    auto r_plus = drag.accel(when, frames::Position<Frame::GCRS>{odl::metres_from_km(x0.position())},
                             odl::metres_from_km(x0.velocity()), p_plus, reg);
    auto r_minus = drag.accel(when, frames::Position<Frame::GCRS>{odl::metres_from_km(x0.position())},
                              odl::metres_from_km(x0.velocity()), p_minus, reg);
    REQUIRE(r_plus.has_value());
    REQUIRE(r_minus.has_value());
    const Vec3 fd{(r_plus->acceleration.metres_per_second_squared().x -
                  r_minus->acceleration.metres_per_second_squared().x) / (2 * h),
                 (r_plus->acceleration.metres_per_second_squared().y -
                  r_minus->acceleration.metres_per_second_squared().y) / (2 * h),
                 (r_plus->acceleration.metres_per_second_squared().z -
                  r_minus->acceleration.metres_per_second_squared().z) / (2 * h)};
    const double rel = (fd - *analytic).norm() / analytic->norm();
    INFO("analytic d(a)/d(C_D) = " << analytic->x << "," << analytic->y << "," << analytic->z
         << "; finite-difference = " << fd.x << "," << fd.y << "," << fd.z
         << "; relative deviation " << rel);
    CHECK(rel < 1.0e-8);
}

// ---------------------------------------------------------------------------
// Liouville, with REAL drag -- not the synthetic linear case STM-A-005b used

TEST_CASE("DRAG-A-005  Liouville with the real drag force: det(Phi) = exp(integral tr A dt), "
          "and the determinant genuinely decays", "[drag][gate]") {
    ParameterRegistry reg;
    auto c_d_id = reg.declare(ParameterDeclaration{ParameterKind::drag_coefficient, "1",
                                                   "C_D", "test satellite"});
    ParameterSet params(reg);
    REQUIRE(params.set(c_d_id, 2.2).has_value());

    const auto when = epoch_at(2015, 6, 21, 12.0);
    const auto sw = quiet_sw(atmosphere::Verification::verified_against_issuer);
    ForceSet forces(reg);
    REQUIRE(forces.add(std::make_shared<TwoBody>()).has_value());
    // A LARGE area/mass ratio, deliberately, AND a low, dense orbit: 300 km
    // altitude (r = 6678 km), not this file's usual 7331 km -- at ~953 km
    // DRAG-A-002 measured rho ~ 2e-15 kg/m^3, too thin for the determinant to
    // move measurably in a test-sized propagation (tried first; det stayed at
    // 1 - 1e-7). At 300 km the density is roughly four orders of magnitude
    // higher, and 5 m^2/kg is a high-drag object (a deployed panel, not a
    // compact bus), so that STM-A-005b's own point -- the determinant
    // ACTUALLY MOVES -- is sharp rather than marginal.
    REQUIRE(forces.add(std::make_shared<Drag>(c_d_id, 25.0, 5.0, sw, zero_eop(), leaps()))
                .has_value());

    const auto x0 = circular_state(when, 6678.0e3);
    std::vector<double> dets;
    for (double seconds : {300.0, 900.0}) {
        auto r = propagate_with_stm(forces, params, x0, seconds, 1e-11);
        REQUIRE(r.has_value());
        // determinant by Gaussian elimination, same routine STM's own tests use
        Mat6 m = r->phi;
        double det = 1.0;
        for (std::size_t c = 0; c < 6; ++c) {
            std::size_t p = c;
            for (std::size_t i = c + 1; i < 6; ++i)
                if (std::abs(m[i][c]) > std::abs(m[p][c])) p = i;
            if (p != c) { std::swap(m[p], m[c]); det = -det; }
            det *= m[c][c];
            for (std::size_t i = c + 1; i < 6; ++i) {
                const double f = m[i][c] / m[c][c];
                for (std::size_t j = c; j < 6; ++j) m[i][j] -= f * m[c][j];
            }
        }
        INFO("after " << seconds << " s with REAL drag: det Phi = " << det
             << ", exp(integral tr A dt) = " << std::exp(r->integrated_trace)
             << ", integral tr A dt = " << r->integrated_trace);
        // the two independent routes to det(Phi) agree...
        CHECK_THAT(det, Catch::Matchers::WithinRel(std::exp(r->integrated_trace), 1e-7));
        // ...the trace integral is genuinely negative (dissipative, unlike
        // conservative gravity alone, where STM-A-005 measured it at exactly
        // zero)...
        CHECK(r->integrated_trace < 0.0);
        // ...and the determinant has genuinely moved off 1 by far more than
        // the integrator's own tolerance (1e-11) could produce as noise --
        // four orders of magnitude of margin, not a threshold tuned to what
        // this one run happened to produce.
        CHECK(det < 1.0 - 1.0e-5);
        dets.push_back(det);
    }
    // and the longer propagation shows MORE decay than the shorter one --
    // the physical claim a single-instant threshold cannot make.
    REQUIRE(dets.size() == 2);
    CHECK(dets[1] < dets[0]);
}

// ---------------------------------------------------------------------------
// The atmosphere's provenance reaches the force's own result

TEST_CASE("DRAG-A-006  the atmosphere's verification flag and snapshot identity survive into "
          "the drag result AND into the Force plugin's ForceEvaluation, in both directions",
          "[drag][gate]") {
    ParameterRegistry reg;
    auto c_d_id = reg.declare(ParameterDeclaration{ParameterKind::drag_coefficient, "1", "C_D", "t"});
    ParameterSet params(reg);
    REQUIRE(params.set(c_d_id, 2.2).has_value());

    // pre-2004: unverified
    {
        const auto when = epoch_at(2000, 1, 1, 0.0);
        const auto st = leo_state(when);
        const auto sw = quiet_sw(atmosphere::Verification::unverified_redistribution);
        auto result = acceleration(when, odl::metres_from_km(st.position()),
                                   odl::metres_from_km(st.velocity()), 2.2, 10.0, 500.0, sw,
                                   zero_eop(), leaps());
        REQUIRE(result.has_value());
        CHECK(result->atmosphere_record.verification == atmosphere::Verification::unverified_redistribution);
        CHECK(result->atmosphere_record.snapshot_id == sw.snapshot_id);

        auto refused = acceleration(when, odl::metres_from_km(st.position()),
                                    odl::metres_from_km(st.velocity()), 2.2, 10.0, 500.0, sw,
                                    zero_eop(), leaps(), /*require_verified=*/true);
        REQUIRE_FALSE(refused.has_value());
        CHECK(refused.error().id == "DRAG-F-001");

        // the SAME sample, through the Force plugin: ForceEvaluation::provenance
        // carries the same identity and the same (un)verified flag.
        Drag drag(c_d_id, 10.0, 500.0, sw, zero_eop(), leaps());
        auto plugin_result = drag.accel(when, frames::Position<Frame::GCRS>{odl::metres_from_km(st.position())},
                                        odl::metres_from_km(st.velocity()), params, reg);
        REQUIRE(plugin_result.has_value());
        REQUIRE(plugin_result->provenance.has_value());
        CHECK_FALSE(plugin_result->provenance->verified_against_issuer);
        CHECK(plugin_result->provenance->source_id == sw.snapshot_id);
    }
    // post-2004: verified, and require_verified does NOT refuse
    {
        const auto when = epoch_at(2015, 6, 21, 12.0);
        const auto st = leo_state(when);
        const auto sw = quiet_sw(atmosphere::Verification::verified_against_issuer);
        auto result = acceleration(when, odl::metres_from_km(st.position()),
                                   odl::metres_from_km(st.velocity()), 2.2, 10.0, 500.0, sw,
                                   zero_eop(), leaps());
        REQUIRE(result.has_value());
        CHECK(result->atmosphere_record.verification == atmosphere::Verification::verified_against_issuer);

        auto not_refused = acceleration(when, odl::metres_from_km(st.position()),
                                        odl::metres_from_km(st.velocity()), 2.2, 10.0, 500.0, sw,
                                        zero_eop(), leaps(), /*require_verified=*/true);
        CHECK(not_refused.has_value());

        Drag drag(c_d_id, 10.0, 500.0, sw, zero_eop(), leaps());
        auto plugin_result = drag.accel(when, frames::Position<Frame::GCRS>{odl::metres_from_km(st.position())},
                                        odl::metres_from_km(st.velocity()), params, reg);
        REQUIRE(plugin_result.has_value());
        REQUIRE(plugin_result->provenance.has_value());
        CHECK(plugin_result->provenance->verified_against_issuer);
    }
}

TEST_CASE("DRAG-A-007  require_verified is a construction-time option of Drag, and reaches "
          "through the Force plugin, not only the free function", "[drag][gate]") {
    ParameterRegistry reg;
    auto c_d_id = reg.declare(ParameterDeclaration{ParameterKind::drag_coefficient, "1", "C_D", "t"});
    ParameterSet params(reg);
    REQUIRE(params.set(c_d_id, 2.2).has_value());
    const auto unverified_epoch = epoch_at(2000, 1, 1, 0.0);
    const auto unverified_state = leo_state(unverified_epoch);
    const auto unverified_sw = quiet_sw(atmosphere::Verification::unverified_redistribution);
    const auto verified_epoch = epoch_at(2015, 6, 21, 12.0);
    const auto verified_state = leo_state(verified_epoch);
    const auto verified_sw = quiet_sw(atmosphere::Verification::verified_against_issuer);

    // require_verified=false (the default): does NOT refuse, even on an
    // unverified sample -- the pre-existing, unchanged behaviour, now
    // correctly framed as the DEFAULT rather than the only reachable one.
    {
        Drag drag(c_d_id, 10.0, 500.0, unverified_sw, zero_eop(), leaps());
        auto r = drag.accel(unverified_epoch,
                            frames::Position<Frame::GCRS>{odl::metres_from_km(unverified_state.position())},
                            odl::metres_from_km(unverified_state.velocity()), params, reg);
        CHECK(r.has_value());
    }
    // require_verified=true, constructed once: refuses DRAG-F-001 through
    // accel() on the unverified sample -- the boundary DRAG-A-006's older
    // draft found unreachable, now closed (dyn::Provenance's own review,
    // PROVENANCE.md §28).
    {
        Drag drag(c_d_id, 10.0, 500.0, unverified_sw, zero_eop(), leaps(), /*require_verified=*/true);
        auto r = drag.accel(unverified_epoch,
                            frames::Position<Frame::GCRS>{odl::metres_from_km(unverified_state.position())},
                            odl::metres_from_km(unverified_state.velocity()), params, reg);
        REQUIRE_FALSE(r.has_value());
        CHECK(r.error().id == "DRAG-F-001");
    }
    // require_verified=true, on a verified sample: does NOT refuse -- shown
    // adjacent to the refusing case per this project's own refusal-catalogue
    // discipline (a refusal fired is only informative next to the same
    // configuration NOT firing on valid input).
    {
        Drag drag(c_d_id, 10.0, 500.0, verified_sw, zero_eop(), leaps(), /*require_verified=*/true);
        auto r = drag.accel(verified_epoch,
                            frames::Position<Frame::GCRS>{odl::metres_from_km(verified_state.position())},
                            odl::metres_from_km(verified_state.velocity()), params, reg);
        CHECK(r.has_value());
    }
}

// ---------------------------------------------------------------------------

TEST_CASE("DRAG-A-008  refusals fire on non-physical inputs, and not on the adjacent valid one",
          "[drag][gate]") {
    const auto when = epoch_at(2015, 6, 21, 12.0);
    const auto x0 = leo_state(when);
    const auto sw = quiet_sw(atmosphere::Verification::verified_against_issuer);
    const Vec3 r = odl::metres_from_km(x0.position()), v = odl::metres_from_km(x0.velocity());

    // DRAG-F-002: non-positive mass or area.
    auto bad_mass = acceleration(when, r, v, 2.2, 10.0, -1.0, sw, zero_eop(), leaps());
    REQUIRE_FALSE(bad_mass.has_value());
    CHECK(bad_mass.error().id == "DRAG-F-002");

    auto bad_area = acceleration(when, r, v, 2.2, 0.0, 500.0, sw, zero_eop(), leaps());
    REQUIRE_FALSE(bad_area.has_value());
    CHECK(bad_area.error().id == "DRAG-F-002");

    // DRAG-F-003: the GCRS<->ITRS transform itself refuses. FRAME-F-003 (read
    // in transform.hpp) refuses an EopRecord whose sub-daily corrections are
    // not applied -- a legitimate, easily-reached case (an EOP record read
    // from a source that has not applied them), not a contrived one. The
    // underlying cause is now carried STRUCTURED (DRAG-Q-002, plan §5
    // constraint 10), not only folded into the message's free text.
    odl::eop::EopRecord unprepared_eop;
    unprepared_eop.subdaily_applied = false;
    auto bad_eop = acceleration(when, r, v, 2.2, 10.0, 500.0, sw, unprepared_eop, leaps());
    REQUIRE_FALSE(bad_eop.has_value());
    CHECK(bad_eop.error().id == "DRAG-F-003");
    REQUIRE(bad_eop.error().cause.has_value());
    CHECK(bad_eop.error().cause->id == "FRAME-F-003");

    // DRAG-F-003's other two nominal causes (the epoch-to-UTC-calendar
    // conversion; the GCRS<->ITRS rotation) are BOTH acknowledged absences,
    // not merely untried -- each has a structural reason, checked directly
    // rather than assumed:
    //
    // The ROTATION cause can never fire: transform.cpp's own to_itrs(state,
    // eop, leaps) computes gcrs_to_itrs(state.epoch(), eop, leaps)
    // INTERNALLY, with these exact arguments, before this function's own
    // later, separate gcrs_to_itrs(t, eop, leaps) call (identical t, eop,
    // leaps) is ever reached -- so by the time that second call runs, the
    // first (line ~78) has already proven it succeeds. Read directly in
    // drag.cpp and transform.cpp, not inferred.
    //
    // The CALENDAR cause is shadowed the same way, checked here rather than
    // assumed: a pre-1972 epoch (built by Duration arithmetic on an
    // otherwise-valid Epoch, since Epoch::from_calendar itself refuses to
    // construct one directly) fails at the TRANSFORM site first, not the
    // calendar site -- to_itrs needs UT1 (= UTC + delta-UT1), which needs
    // the SAME TAI<->UTC rendering .calendar(UTC, leaps) performs, and
    // to_itrs runs first in this function. Demonstrated directly below,
    // asserting WHICH id fires, not merely that refusal happens.
    {
        const auto base = epoch_at(1980, 1, 1, 0.0);
        const auto pre1972 = base.add(odl::time::Duration::from_seconds(-20.0 * 365.25 * 86400.0));
        const auto st = leo_state(pre1972);
        auto pre1972_result = acceleration(pre1972, odl::metres_from_km(st.position()),
                                           odl::metres_from_km(st.velocity()), 2.2, 10.0, 500.0, sw,
                                           zero_eop(), leaps());
        REQUIRE_FALSE(pre1972_result.has_value());
        CHECK(pre1972_result.error().id == "DRAG-F-003");
        REQUIRE(pre1972_result.error().cause.has_value());
        // TIME-F-002 (the transform's own UT1/UTC need), not a calendar-
        // conversion id: the demonstration IS that the transform site fires
        // first, pre-empting the calendar site entirely.
        CHECK(pre1972_result.error().cause->id == "TIME-F-002");
    }

    // DRAG-F-005: atmosphere::for_drag itself refuses. ATMO-F-001 (read in
    // atmosphere.cpp) refuses a negative geodetic altitude -- reached here
    // with a genuine physical case, a satellite below the WGS84 ellipsoid
    // (re-entry/decay), not a fabricated Place field: geodetic_to_itrs at
    // -50 km altitude gives a real ITRS position, carried through to_gcrs
    // (the frames module's own tested inverse of to_itrs) to get a
    // consistent GCRS state, exactly as any other caller would arrive at
    // this position.
    {
        const Geodetic below{0.1, 0.2, -50000.0};
        const Vec3 r_itrs_m = geodetic_to_itrs(below);
        const odl::frames::ItrsState itrs_state{when, odl::km_from_metres(r_itrs_m),
                                                 odl::km_from_metres(Vec3{0.0, 0.0, 7.5})};
        auto gcrs = odl::frames::to_gcrs(itrs_state, zero_eop(), leaps());
        REQUIRE(gcrs.has_value());
        auto bad_alt = acceleration(when, odl::metres_from_km(gcrs->position()),
                                    odl::metres_from_km(gcrs->velocity()), 2.2, 10.0, 500.0, sw,
                                    zero_eop(), leaps());
        REQUIRE_FALSE(bad_alt.has_value());
        CHECK(bad_alt.error().id == "DRAG-F-005");
    }

    // DRAG-F-004 (zero relative velocity) is NOT fired here. Its guard is
    // `speed > 0.0` on the ITRS-frame relative velocity, and the only way to
    // reach exactly zero is a state whose ITRS velocity a rigorous
    // precession-nutation-polar-motion transform maps to bit-exact
    // {0,0,0} -- not a physical orbit (no real satellite is EXACTLY
    // co-rotating, and even a constructed near-geostationary state lands at
    // a tiny but nonzero residual, which is the CORRECT non-refusing answer,
    // not a gap in the check). Recorded as an acknowledged absence per plan
    // §4 rule 4, not silently skipped: the guard exists and is read at
    // drag.cpp's `if (!(speed > 0.0))`, but no honest input in this test
    // suite reaches it.

    auto ok = acceleration(when, r, v, 2.2, 10.0, 500.0, sw, zero_eop(), leaps());
    CHECK(ok.has_value());
}

TEST_CASE("DRAG-A-009  day-of-year against the Gregorian leap-year rule's own exceptions",
          "[drag][gate]") {
    using odl::drag::detail::day_of_year;

    // 1900 is NOT a leap year (divisible by 100, not 400); 2000 IS (divisible
    // by 400); 2100 is NOT (divisible by 100, not 400, the same shape as
    // 1900 but on the other side of the table this tree can represent as a
    // UTC epoch) -- exactly the branches the rule's three clauses cover.
    // Called directly (not through an Epoch): 1900 is before the leap
    // table's 1972 floor and 2100 is past any realistic table expiry, so
    // neither is representable as a UTC `Epoch` in this tree (SPEC-time
    // TIME-F-002, TIME-F-004) -- exercising the century exception at all
    // requires reaching the calculation directly, which is why it is
    // exposed in `detail` rather than left anonymous.
    struct Case { int y, m, d, expect; };
    const Case cases[] = {
        {1900, 3, 1, 60},    // no Feb 29 in 1900: Jan(31)+Feb(28)+1 = 60
        {2000, 3, 1, 61},    // Feb 29 EXISTS in 2000: 31+29+1 = 61
        {2100, 3, 1, 60},    // no Feb 29 in 2100, same shape as 1900
        {2004, 3, 1, 61},    // an ordinary (div-4-not-100) leap year: 31+29+1 = 61
        {2015, 6, 21, 172},  // the epoch every other test in this file uses
        {2015, 1, 1, 1},
        {2015, 12, 31, 365}, // 2015 ordinary: no 366th day
        {2016, 12, 31, 366}, // 2016 leap: the year HAS a 366th day
    };
    for (const auto& c : cases) {
        INFO("y=" << c.y << " m=" << c.m << " d=" << c.d);
        CHECK(day_of_year(c.y, c.m, c.d) == c.expect);
    }

    // And the end-to-end wiring, through the public entry point, for a
    // representable date: day_of_year's return value actually reaches
    // atmosphere::Place and a representable day does not spuriously refuse
    // via ATMO-F-002 (whose accepted range is exactly 1 ... 366).
    const auto when = epoch_at(2016, 12, 31, 12.0);
    const auto st = leo_state(when);
    const auto sw = quiet_sw(atmosphere::Verification::verified_against_issuer);
    auto result = acceleration(when, odl::metres_from_km(st.position()),
                               odl::metres_from_km(st.velocity()), 2.2, 10.0, 500.0, sw,
                               zero_eop(), leaps());
    REQUIRE(result.has_value());
}

// ---------------------------------------------------------------------------
// The position Jacobian, against a real finite difference.
//
// DRAG-R-004's Jacobian has two channels: channel 1 (density's altitude
// dependence, a central difference of two atmosphere calls, lateral gradient
// still neglected) and channel 2 (v_rel's own weak dependence on position,
// exact and analytic). At the equator specifically (this test's own
// geometry), a purely geocentric-radial perturbation coincides exactly with
// the geodetic normal (the deviation of the vertical, (e^2/2) sin(2*lat), is
// identically zero at latitude 0), so channel 1's neglected lateral gradient
// cannot enter this comparison at all -- what CAN enter is channel 1's own
// discretisation error and any error in channel 2's own approximation, both
// checked below.
//
// THIS TEST FOUND TWO REAL DEFECTS, not a tolerance question, across two
// rounds of review.
//
// (1) drag.cpp's first draft of `a_direction` (the d(a)/d(rho) factor
// channel 1 is built from) read `(coeff/rho) * v_rel`; the correct
// expression, since a = coeff*speed*v_rel, is `(coeff/rho) * speed * v_rel`
// -- missing exactly a factor of |v_rel| (the CO-ROTATING-frame relative
// speed, ~7.24 km/s at this case, not the ~7.73 km/s inertial orbital speed
// -- the co-rotating figure is what the measured ~7330x discrepancy actually
// matched). PROVENANCE.md §28.4.
//
// (2) The FIRST FIX's tolerance was itself found wanting by review: a
// one-sided (forward) altitude difference measured 1.18e-2, and "5e-2 keeps
// a 4x margin" was set from that passing number -- rule 7's own defect, a
// threshold chosen from the result it judges. Caught by a falsifiable
// prediction (halving the step should roughly halve a first-order
// truncation term): it dropped to 3.8e-3, confirming step-dependent
// truncation, not the neglected lateral gradient this test's own comment had
// blamed it on. Switching to a CENTRAL difference (second-order truncation)
// and adding the exact channel-2 term should have landed near the textbook
// (Delta/H)^2/6 (~1e-4 at 1 km) -- it measured ~9.5e-4 instead, an order of
// magnitude off that formula. A STEP SWEEP (1, 0.5, 0.25, 0.1, 0.05 km),
// with channel 1 and channel 2 isolated from each other (channel 2 verified
// separately by holding rho fixed and finite-differencing a(v_itrs(r))
// through the real to_itrs velocity, matching the analytic formula to 6
// figures), found why: channel 1's own error is NON-MONOTONIC across
// 1-0.25 km, then drops sharply and stably at 0.1 km and below -- the
// signature of NRLMSISE-00's own fitted cubic-spline structure in altitude
// (SPEC-atmosphere §3.1), not smooth Taylor truncation, at the coarser
// scale. drag.cpp's step is now 0.1 km (DRAG-P-1, revised); this test
// verifies the STABILITY that step sweep found, not a formula that turned
// out not to describe the real profile. PROVENANCE.md §28.5.

TEST_CASE("DRAG-A-010  the position Jacobian against a finite difference of the real "
          "acceleration, with a stability check standing in for a formula that "
          "turned out not to describe the real profile", "[drag][gate]") {
    ParameterRegistry reg;
    auto c_d_id = reg.declare(ParameterDeclaration{ParameterKind::drag_coefficient, "1",
                                                   "C_D", "test satellite"});
    ParameterSet params(reg);
    REQUIRE(params.set(c_d_id, 2.2).has_value());

    const auto when = epoch_at(2015, 6, 21, 12.0);
    const auto sw = quiet_sw(atmosphere::Verification::verified_against_issuer);
    // 300 km, the same low/dense orbit DRAG-A-005 uses: at ~953 km the
    // density gradient itself is measurable but the SIGNAL (the actual
    // acceleration change over a metres-scale radial step) is close enough
    // to the atmosphere model's and the integrator's own rounding floor that
    // the comparison would be noise-dominated rather than approximation-
    // dominated -- the wrong thing to be measuring here. Equatorial, per the
    // comment above: the point this test's own lateral-gradient argument
    // relies on.
    const auto x0 = circular_state(when, 6678.0e3);
    Drag drag(c_d_id, 10.0, 500.0, sw, zero_eop(), leaps());

    const Vec3 r0 = odl::metres_from_km(x0.position());
    const Vec3 v0 = odl::metres_from_km(x0.velocity());
    auto r0_eval = drag.accel(when, frames::Position<Frame::GCRS>{r0}, v0, params, reg);
    REQUIRE(r0_eval.has_value());
    const Mat3& analytic = r0_eval->d_state.d_position();

    // A radial GCRS step. GCRS and ITRS share an origin and differ by a pure
    // rotation, so "radially outward" is the SAME physical direction in
    // both frames -- perturbing r_gcrs along r_gcrs/|r_gcrs| is exactly the
    // altitude-only perturbation channel 1's own derivation assumes.
    const double r_norm = r0.norm();
    const Vec3 r_hat = (1.0 / r_norm) * r0;
    constexpr double kStepM = 10.0;   // metres; small against a ~40 km scale height

    auto eval_at = [&](double sign) {
        const Vec3 r = r0 + (sign * kStepM) * r_hat;
        return drag.accel(when, frames::Position<Frame::GCRS>{r}, v0, params, reg);
    };
    auto r_plus = eval_at(+1.0);
    auto r_minus = eval_at(-1.0);
    REQUIRE(r_plus.has_value());
    REQUIRE(r_minus.has_value());
    const Vec3 a_plus = r_plus->acceleration.metres_per_second_squared();
    const Vec3 a_minus = r_minus->acceleration.metres_per_second_squared();
    const Vec3 fd_da = (1.0 / (2.0 * kStepM)) * (a_plus - a_minus);
    const Vec3 predicted_da = analytic.apply(r_hat);

    const double rel = (fd_da - predicted_da).norm() / fd_da.norm();
    INFO("finite-difference d(a)/d(r_hat) = " << fd_da.x << "," << fd_da.y << "," << fd_da.z
         << "; analytic-Jacobian prediction = " << predicted_da.x << "," << predicted_da.y
         << "," << predicted_da.z << "; relative deviation " << rel);
    // Measured 1.19e-6 at drag.cpp's registered 0.1 km step. 1e-4 keeps
    // three orders of margin -- not chosen to fit this one number, but
    // because it is comfortably above what the STABILITY check just below
    // would let through: if channel 1 were back in the 1-0.25 km regime's
    // non-monotonic ~1e-3 error (a wrong step, a reverted fix, a changed
    // atmosphere pin shifting the spline structure), that check fails first
    // and names the reason; this bound is the coarser, whole-Jacobian
    // backstop.
    CHECK(rel < 1.0e-4);
    // and the two vectors must at least point the same general way -- a
    // sign flip in the radial direction would pass a magnitude-only check
    CHECK(fd_da.dot(predicted_da) > 0.0);

    // THE STABILITY CHECK. Independently reconstruct the Place drag.cpp
    // itself builds (DRAG-A-002's own pattern) and recompute channel 1 ALONE
    // (the density-altitude term only, not channel 2) at drag.cpp's own
    // registered step AND at half of it, from the same kind of atmosphere
    // calls drag.cpp makes internally. The two must closely agree: that is
    // what "past the spline structure, in the smooth/converged regime"
    // MEANS, operationally, and it is exactly the property whose ABSENCE
    // this test's own review caught at the coarser 1-0.25 km steps.
    {
        auto itrs = frames::to_itrs(x0, zero_eop(), leaps());
        REQUIRE(itrs.has_value());
        const auto geo = itrs_to_geodetic(odl::metres_from_km(itrs->position()));
        auto cal = when.calendar(odl::time::TimeScale::UTC, leaps());
        REQUIRE(cal.has_value());
        atmosphere::Place place;
        place.day_of_year = 172;   // 2015-06-21, DRAG-A-002's own stated cross-check
        place.seconds_of_day = cal->hour * 3600.0 + cal->minute * 60.0 + cal->second;
        place.geodetic_latitude_deg = geo.latitude_rad * 180.0 / std::numbers::pi;
        place.longitude_deg = geo.longitude_rad * 180.0 / std::numbers::pi;
        place.altitude_km = geo.altitude_m / 1000.0;

        auto channel1_slope = [&](double stepKm) -> odl::Result<double, atmosphere::AtmoError> {
            atmosphere::Place up = place, down = place;
            up.altitude_km += stepKm;
            down.altitude_km -= stepKm;
            auto rho_up = atmosphere::for_drag(up, sw);
            if (!rho_up.has_value()) return odl::err(rho_up.error());
            auto rho_down = atmosphere::for_drag(down, sw);
            if (!rho_down.has_value()) return odl::err(rho_down.error());
            return (rho_up->total_mass_kg_m3 - rho_down->total_mass_kg_m3) /
                   (2.0 * odl::metres_from_km(stepKm));
        };
        constexpr double kProductionStepKm = 0.1;   // matching DRAG-P-1 exactly
        auto slope_full = channel1_slope(kProductionStepKm);
        auto slope_half = channel1_slope(kProductionStepKm / 2.0);
        REQUIRE(slope_full.has_value());
        REQUIRE(slope_half.has_value());
        const double stability_rel = std::abs(*slope_full - *slope_half) / std::abs(*slope_full);
        INFO("channel 1 d(rho)/d(alt): at 0.1 km step = " << *slope_full << ", at 0.05 km step = "
             << *slope_half << "; relative difference " << stability_rel);
        // 1e-3: comfortably above rounding, comfortably below the ~1e-1 to
        // 1 scale the review's own step sweep measured for the NON-converged
        // 1-0.25 km regime -- tight enough to fail if that regime returned,
        // loose enough not to chase the atmosphere model's own last-digit
        // noise.
        CHECK(stability_rel < 1.0e-3);
    }
}
