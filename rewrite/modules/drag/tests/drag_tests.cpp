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
          "the drag result, in both directions", "[drag][gate]") {
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
    }
}

TEST_CASE("DRAG-A-007  require_verified reaches through the Force plugin too, not only the "
          "free function", "[drag][gate]") {
    ParameterRegistry reg;
    auto c_d_id = reg.declare(ParameterDeclaration{ParameterKind::drag_coefficient, "1", "C_D", "t"});
    ParameterSet params(reg);
    REQUIRE(params.set(c_d_id, 2.2).has_value());
    const auto when = epoch_at(2000, 1, 1, 0.0);
    const auto x0 = leo_state(when);
    const auto sw = quiet_sw(atmosphere::Verification::unverified_redistribution);
    Drag drag(c_d_id, 10.0, 500.0, sw, zero_eop(), leaps());
    auto r = drag.accel(when, frames::Position<Frame::GCRS>{odl::metres_from_km(x0.position())},
                        odl::metres_from_km(x0.velocity()), params, reg);
    // Drag::accel calls the free function with its default require_verified=false,
    // so this one SUCCEEDS -- documenting the boundary precisely, since a
    // caller wanting the refusal through the Force interface would need a
    // variant that requests it, which this version does not yet offer (open
    // question, PROVENANCE.md §28).
    CHECK(r.has_value());
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
    // from a source that has not applied them), not a contrived one.
    odl::eop::EopRecord unprepared_eop;
    unprepared_eop.subdaily_applied = false;
    auto bad_eop = acceleration(when, r, v, 2.2, 10.0, 500.0, sw, unprepared_eop, leaps());
    REQUIRE_FALSE(bad_eop.has_value());
    CHECK(bad_eop.error().id == "DRAG-F-003");

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
// The position Jacobian's radial part, against a real finite difference.
//
// Unlike DRAG-A-004's C_D column, this is NOT a claim of near-machine-precision
// agreement: DRAG-R-004 is a NAMED APPROXIMATION (radial scale-height only,
// lateral gradient neglected), and a true finite difference of the full
// acceleration() call also picks up a channel the analytic form does not
// model at all -- v_rel itself has a weak dependence on position (the
// GCRS<->ITRS velocity transform's transport term depends on r), which the
// analytic dadr, built by bumping ONLY altitude at fixed v_rel, cannot see.
// So this checks that the code correctly implements what it claims to (the
// right order of magnitude and the right sign along the direction it says it
// models), not that the approximation has no error -- and DRAG-A-005 was not
// a substitute: tr(A) never touches da/dr, since that block sits off the
// diagonal of the 6x6 Jacobian, so nothing before this test actually
// exercised R-004 (found by speccheck.py's coverage check, not anticipated).
//
// THIS TEST FOUND A REAL DEFECT, not a tolerance question. drag.cpp's first
// draft of `a_direction` (the d(a)/d(rho) factor dadr is built from) read
// `(coeff/rho) * v_rel`; the correct expression, since a = coeff*speed*v_rel,
// is `(coeff/rho) * speed * v_rel` -- missing exactly a factor of |v_rel|.
// Before the fix this test measured a 7300x discrepancy (v_rel at this LEO
// case is ~7.7 km/s, matching the missing factor almost exactly) and a near-
// zero dot product; after it, 1.18e-2, well inside the tolerance below.
// Recorded in full in PROVENANCE.md.

TEST_CASE("DRAG-A-010  the position Jacobian's radial part against a finite difference "
          "of the real acceleration, to the precision the approximation itself claims",
          "[drag][gate]") {
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
    // dominated -- the wrong thing to be measuring here.
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
    // altitude-only perturbation DRAG-R-004's own derivation assumes (the
    // induced geodetic-latitude shift from Earth's oblateness, at this step
    // size over this radius, is smaller than the effect being measured by
    // several more orders of magnitude than the tolerance below needs).
    const double r_norm = r0.norm();
    const Vec3 r_hat = (1.0 / r_norm) * r0;
    constexpr double kStepM = 10.0;   // metres; small against a ~60 km scale height

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
    // Order-of-magnitude and sign agreement, not near-machine precision: see
    // the test-case comment above for why exact agreement is not expected.
    // Measured at 1.18e-2 for this test's own geometry (the v_rel(r) channel
    // the analytic form omits, plus the FD step's own truncation) -- 5e-2
    // keeps a 4x margin above the measured value without loosening so far
    // that a real regression (this test's own first draft was off by
    // ~7300x, a missing factor of |v_rel| -- see the test-case comment) could
    // hide inside the tolerance.
    CHECK(rel < 0.05);
    // and the two vectors must at least point the same general way -- a
    // sign flip in the radial direction would pass a magnitude-only check
    CHECK(fd_da.dot(predicted_da) > 0.0);
}
