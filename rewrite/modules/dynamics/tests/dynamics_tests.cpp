// dynamics_tests.cpp — L3 step 1's gate.
//
// SPEC-dynamics §8.  The surface is gated with a TRIVIAL force on purpose: a
// trivial force exercises the surface without negotiating with it, which is why
// the plan freezes the interface before any real force exists.
//
// What these gates CANNOT do is stated in the specification and restated here so
// a reader of the tests meets it: there is no external specification of this
// interface (plan §4 rule 6), so nothing below can check that the surface is the
// RIGHT SHAPE — only that it is self-consistent and carries a force end to end.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/dynamics/force_set.hpp>
#include <odl/gravity/field.hpp>

#include <memory>
#include <type_traits>
#include <vector>

using namespace odl;
using namespace odl::dyn;
using frames::Frame;

namespace {

odl::time::Epoch an_epoch() {
    auto e = odl::time::Epoch::from_gps_week(2000, 0.0);
    REQUIRE(e.has_value());
    return *e;
}

frames::State<Frame::GCRS> a_state(const odl::time::Epoch& t) {
    return frames::State<Frame::GCRS>{t, Vec3{7331.0, 0.0, 0.0}, Vec3{0.0, 7.374, 0.0}};
}

/// A constant acceleration scaled by one parameter, declaring NO velocity
/// dependence — with the bound DYN-R-027 requires.
class TrivialForce final : public Force {
public:
    TrivialForce(ParameterId p, double bound) : p_(p), consumes_{p}, bound_(bound) {}
    ForceId id() const override { return ForceId{"trivial"}; }
    const std::vector<ParameterId>& consumes() const override { return consumes_; }

    odl::Result<ForceEvaluation, DynError>
    accel(const odl::time::Epoch&, const frames::Position<Frame::GCRS>& r_m, const Vec3&,
          const ParameterSet& params, const ParameterRegistry& reg) const override {
        auto s = params.value(p_);
        if (!s) return odl::err(s.error());
        const Vec3 a{*s * 1.0e-7, 0.0, 0.0};
        Mat3 dadr{};                      // constant in position
        ParameterJacobian dp{reg};
        auto put = dp.set_column(p_, Vec3{1.0e-7, 0.0, 0.0});
        if (!put) return odl::err(put.error());
        (void)r_m;
        return ForceEvaluation{frames::Acceleration<Frame::GCRS>{a},
                               StateJacobian::no_velocity_dependence(dadr, bound_),
                               std::move(dp)};
    }

private:
    ParameterId p_;
    std::vector<ParameterId> consumes_;
    double bound_;
};

/// A drag-shaped force: proportional to -v, so its velocity block is LIVE.
class VelocityForce final : public Force {
public:
    explicit VelocityForce(ParameterId p) : p_(p), consumes_{p} {}
    ForceId id() const override { return ForceId{"velocity"}; }
    const std::vector<ParameterId>& consumes() const override { return consumes_; }

    odl::Result<ForceEvaluation, DynError>
    accel(const odl::time::Epoch&, const frames::Position<Frame::GCRS>&, const Vec3& v,
          const ParameterSet& params, const ParameterRegistry& reg) const override {
        auto k = params.value(p_);
        if (!k) return odl::err(k.error());
        const Vec3 a{-*k * v.x, -*k * v.y, -*k * v.z};
        Mat3 dadv{};
        dadv.r[0][0] = dadv.r[1][1] = dadv.r[2][2] = -*k;
        ParameterJacobian dp{reg};
        auto put = dp.set_column(p_, Vec3{-v.x, -v.y, -v.z});
        if (!put) return odl::err(put.error());
        return ForceEvaluation{frames::Acceleration<Frame::GCRS>{a},
                               StateJacobian::with_velocity(Mat3{}, dadv), std::move(dp)};
    }

private:
    ParameterId p_;
    std::vector<ParameterId> consumes_;
};

}  // namespace

// ---------------------------------------------------------------------------
// DYN-A-002 and DYN-A-013: COMPILE-TIME, not runtime. A bare index must not be
// expressible; a runtime refusal would mean the expression existed.

static_assert(!std::is_constructible_v<ParameterId, std::size_t>,
              "DYN-R-002: a ParameterId must not be makeable from an index");
static_assert(!std::is_constructible_v<ParameterId, int>,
              "DYN-R-002: nor from an int");
static_assert(!std::is_constructible_v<ParameterId, const char*>,
              "DYN-R-003: nor from a name — a name alone collides across subjects");
static_assert(!std::is_default_constructible_v<ParameterId>,
              "DYN-R-002: nor out of nothing");
// The absence of a member is asserted through a CONCEPT rather than a bare
// requires-expression: outside a template, a requirement body that does not
// compile is a hard error rather than `false`, so the assertion would fail to
// build instead of failing to hold — which reads the same in a log and is not.
template <class T, class I>
concept HasSubscript = requires(T t, I i) { t[i]; };
template <class T>
concept HasData = requires(T t) { t.data(); };
template <class T, class I>
concept HasPositionalColumn = requires(T t, I i) { t.column(i); };

static_assert(!HasSubscript<ParameterSet, std::size_t>,
              "DYN-R-002: ParameterSet must have no positional access");
static_assert(!HasData<ParameterSet>,
              "DYN-R-002: nor a raw block a caller could index");
static_assert(!HasPositionalColumn<ParameterJacobian, std::size_t>,
              "DYN-R-005: a Jacobian column is addressed by identity");
// and the same concepts hold POSITIVELY where positional access is legitimate,
// so the assertions above are known to be testing something.
static_assert(HasSubscript<std::vector<double>, std::size_t>,
              "the concept detects subscripting where it exists");
static_assert(HasData<std::vector<double>>,
              "the concept detects data() where it exists");
static_assert(!std::is_copy_constructible_v<ParameterRegistry>,
              "DYN-R-002: two copies of a registry would be two identities claiming to be one");

TEST_CASE("DYN-A-001  the surface carries a trivial force end to end", "[dynamics][gate]") {
    ParameterRegistry reg;
    const auto cd = reg.declare({ParameterKind::drag_coefficient, "1", "C_D", "sat-A"});
    ParameterSet vals{reg};
    REQUIRE(vals.set(cd, 2.2).has_value());

    ForceSet set{reg};
    REQUIRE(set.add(std::make_shared<TrivialForce>(cd, 5.0e-14)).has_value());

    const auto t = an_epoch();
    const auto s = a_state(t);
    const auto d = set.derivative(t, s, vals);
    REQUIRE(d.has_value());
    // the crossing: 2.2e-7 m/s^2 becomes 2.2e-10 km/s^2
    CHECK_THAT(d->acceleration_km_s2.x, Catch::Matchers::WithinRel(2.2e-10, 1e-15));
    CHECK(d->velocity_km_s.y == s.velocity().y);

    const auto j = set.jacobians(t, s, vals);
    REQUIRE(j.has_value());
    CHECK(j->da_dp.width() == 1);
    const auto col = j->da_dp.column(cd);
    REQUIRE(col.has_value());
    CHECK_THAT(col->x, Catch::Matchers::WithinRel(1.0e-7, 1e-15));
}

TEST_CASE("DYN-A-003  identity, not name", "[dynamics]") {
    ParameterRegistry reg;
    const auto a = reg.declare({ParameterKind::drag_coefficient, "1", "C_D", "sat-A"});
    const auto b = reg.declare({ParameterKind::drag_coefficient, "1", "C_D", "sat-B"});
    CHECK_FALSE(a == b);
    ParameterSet vals{reg};
    REQUIRE(vals.set(a, 2.2).has_value());
    REQUIRE(vals.set(b, 2.9).has_value());
    CHECK(*vals.value(a) == 2.2);
    CHECK(*vals.value(b) == 2.9);
}

TEST_CASE("DYN-A-004  nothing has a fixed width", "[dynamics]") {
    ParameterRegistry reg;
    const auto a = reg.declare({ParameterKind::drag_coefficient, "1", "C_D", "sat-A"});
    CHECK(reg.size() == 1);
    ParameterJacobian j1{reg};
    CHECK(j1.width() == 1);
    const auto b = reg.declare({ParameterKind::srp_scale, "1", "C_R", "sat-A"});
    CHECK(reg.size() == 2);
    ParameterJacobian j2{reg};
    CHECK(j2.width() == 2);          // follows the registry, with nothing declared twice
    (void)a; (void)b;
}

TEST_CASE("DYN-A-005  a foreign identity is refused, naming both", "[dynamics]") {
    ParameterRegistry one, two;
    const auto a = one.declare({ParameterKind::drag_coefficient, "1", "C_D", "sat-A"});
    ParameterSet vals{two};
    const auto r = vals.set(a, 1.0);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().id == "DYN-F-001");
    ParameterJacobian j{two};
    const auto c = j.column(a);
    REQUIRE_FALSE(c.has_value());
    CHECK(c.error().id == "DYN-F-003");
    // proven both ways
    ParameterSet ok{one};
    CHECK(ok.set(a, 1.0).has_value());
}

TEST_CASE("DYN-A-006  the velocity block is live", "[dynamics]") {
    ParameterRegistry reg;
    const auto k = reg.declare({ParameterKind::ballistic_coefficient, "1/s", "k", "sat-A"});
    ParameterSet vals{reg};
    REQUIRE(vals.set(k, 1.0e-5).has_value());
    ForceSet set{reg};
    REQUIRE(set.add(std::make_shared<VelocityForce>(k)).has_value());
    const auto t = an_epoch();
    const auto j = set.jacobians(t, a_state(t), vals);
    REQUIRE(j.has_value());
    CHECK_THAT(j->da_dv.r[0][0], Catch::Matchers::WithinRel(-1.0e-5, 1e-15));
    CHECK(j->neglected_velocity_bound_per_s == 0.0);   // nothing was declared absent
}

TEST_CASE("DYN-A-017  a neglected velocity dependence carries a bound", "[dynamics]") {
    ParameterRegistry reg;
    const auto cd = reg.declare({ParameterKind::drag_coefficient, "1", "C_D", "sat-A"});
    ParameterSet vals{reg};
    REQUIRE(vals.set(cd, 2.2).has_value());
    ForceSet set{reg};
    REQUIRE(set.add(std::make_shared<TrivialForce>(cd, 3.1942e-14)).has_value());
    const auto t = an_epoch();
    const auto j = set.jacobians(t, a_state(t), vals);
    REQUIRE(j.has_value());
    // ABSENT is not ZERO: the bound survives into the caller's budget.
    CHECK_THAT(j->neglected_velocity_bound_per_s,
               Catch::Matchers::WithinRel(3.1942e-14, 1e-12));
    // and the two states are distinguishable in the type
    const auto absent = StateJacobian::no_velocity_dependence(Mat3{}, 1.0e-14);
    const auto present = StateJacobian::with_velocity(Mat3{}, Mat3{});
    CHECK_FALSE(absent.d_velocity().has_value());
    CHECK(present.d_velocity().has_value());
}

TEST_CASE("DYN-A-007  the set reports what it summed", "[dynamics]") {
    ParameterRegistry reg;
    const auto cd = reg.declare({ParameterKind::drag_coefficient, "1", "C_D", "sat-A"});
    const auto k = reg.declare({ParameterKind::ballistic_coefficient, "1/s", "k", "sat-A"});
    ParameterSet vals{reg};
    REQUIRE(vals.set(cd, 2.2).has_value());
    REQUIRE(vals.set(k, 1.0e-6).has_value());
    ForceSet set{reg};
    REQUIRE(set.add(std::make_shared<TrivialForce>(cd, 1.0e-14)).has_value());
    REQUIRE(set.add(std::make_shared<VelocityForce>(k)).has_value());

    const auto t = an_epoch();
    const auto s = a_state(t);
    const auto terms = set.contributions_at(t, s, vals);
    REQUIRE(terms.has_value());
    REQUIRE(terms->size() == 2);
    CHECK((*terms)[0].force.name == "trivial");
    CHECK((*terms)[1].force.name == "velocity");

    Vec3 sum{};
    for (const auto& c : *terms) {
        sum.x += c.acceleration_m_s2.x; sum.y += c.acceleration_m_s2.y;
        sum.z += c.acceleration_m_s2.z;
    }
    const auto d = set.derivative(t, s, vals);
    REQUIRE(d.has_value());
    CHECK_THAT(d->acceleration_km_s2.x, Catch::Matchers::WithinRel(sum.x / 1000.0, 1e-15));
    CHECK_THAT(d->acceleration_km_s2.y, Catch::Matchers::WithinRel(sum.y / 1000.0, 1e-15));
}

TEST_CASE("DYN-A-012  refusals fire, and do not fire on the adjacent input", "[dynamics]") {
    ParameterRegistry reg;
    const auto cd = reg.declare({ParameterKind::drag_coefficient, "1", "C_D", "sat-A"});
    ForceSet set{reg};
    REQUIRE(set.add(std::make_shared<TrivialForce>(cd, 1.0e-14)).has_value());

    // DYN-F-005: the same identity twice
    const auto dup = set.add(std::make_shared<TrivialForce>(cd, 1.0e-14));
    REQUIRE_FALSE(dup.has_value());
    CHECK(dup.error().id == "DYN-F-005");

    // DYN-F-002: a value the caller never supplied is NOT defaulted to zero
    ParameterSet empty{reg};
    const auto t = an_epoch();
    const auto r = set.derivative(t, a_state(t), empty);
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error().id == "DYN-F-002");

    // proven both ways
    ParameterSet vals{reg};
    REQUIRE(vals.set(cd, 2.2).has_value());
    CHECK(set.derivative(t, a_state(t), vals).has_value());
}

TEST_CASE("DYN-A-009  no global state", "[dynamics]") {
    ParameterRegistry r1, r2;
    const auto a = r1.declare({ParameterKind::drag_coefficient, "1", "C_D", "A"});
    const auto b = r2.declare({ParameterKind::drag_coefficient, "1", "C_D", "B"});
    ParameterSet v1{r1}, v2{r2};
    REQUIRE(v1.set(a, 1.0).has_value());
    REQUIRE(v2.set(b, 9.0).has_value());
    ForceSet s1{r1}, s2{r2};
    REQUIRE(s1.add(std::make_shared<TrivialForce>(a, 0.0)).has_value());
    REQUIRE(s2.add(std::make_shared<TrivialForce>(b, 0.0)).has_value());
    const auto t = an_epoch();
    const auto d1 = s1.derivative(t, a_state(t), v1);
    const auto d2 = s2.derivative(t, a_state(t), v2);
    REQUIRE(d1.has_value()); REQUIRE(d2.has_value());
    CHECK(d1->acceleration_km_s2.x != d2->acceleration_km_s2.x);
    // and running them again in the other order changes nothing
    const auto d2b = s2.derivative(t, a_state(t), v2);
    const auto d1b = s1.derivative(t, a_state(t), v1);
    REQUIRE(d1b.has_value()); REQUIRE(d2b.has_value());
    CHECK(d1b->acceleration_km_s2.x == d1->acceleration_km_s2.x);
    CHECK(d2b->acceleration_km_s2.x == d2->acceleration_km_s2.x);
}

TEST_CASE("DYN-A-018  a gradient() entry point would be additive", "[dynamics]") {
    // GRAV-Q-006's structural half, asserted against the HEADER a caller compiles
    // against rather than against the specification's prose. Every
    // ConventionalField entry point RETURNS its result; none fills a
    // caller-supplied buffer, so adding one breaks no caller.
    using CF = odl::gravity::ConventionalField;
    static_assert(!std::is_void_v<decltype(std::declval<const CF&>().acceleration(
        std::declval<const frames::ItrsPosition&>(), std::declval<odl::gravity::Degree>(),
        std::declval<odl::gravity::Order>()))>,
        "acceleration returns its value");
    static_assert(!std::is_void_v<decltype(std::declval<const CF&>().potential(
        std::declval<const frames::ItrsPosition&>(), std::declval<odl::gravity::Degree>(),
        std::declval<odl::gravity::Order>()))>,
        "potential returns its value");
    SUCCEED("SPEC-gravity §5's entry points return values; gradient() is additive (DYN-R-028)");
}
