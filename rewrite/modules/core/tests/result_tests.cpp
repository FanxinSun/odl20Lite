// The step-1 gate: a refusal round-trips through the alias, and the contract
// the whole tree rests on behaves as SPEC-template.md §5 requires.

#include <odl/core/result.hpp>
#include <odl/core/units.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <type_traits>

#include <string>

namespace {

odl::Result<int> halve(int n) {
    if (n % 2 != 0) {
        return odl::err("CORE-F-001", "cannot halve " + std::to_string(n) + ": not even");
    }
    return n / 2;
}

}  // namespace

TEST_CASE("a value round-trips", "[core][result]") {
    const auto r = halve(8);
    REQUIRE(r.has_value());
    REQUIRE(*r == 4);
    REQUIRE(r.value() == 4);
}

TEST_CASE("a refusal round-trips, carrying its clause and its values", "[core][result]") {
    const auto r = halve(7);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().id == "CORE-F-001");
    // The message names the offending value, per SPEC-template.md §5: a
    // diagnostic that cannot be acted on without re-running is not a diagnostic.
    REQUIRE(r.error().message.find('7') != std::string::npos);
}

TEST_CASE("a refusal is not silently convertible to a value", "[core][result]") {
    STATIC_REQUIRE_FALSE(std::is_convertible_v<odl::Result<int>, int>);
    // bool conversion is explicit, so `if (r)` works but `int x = r;` does not.
    STATIC_REQUIRE(std::is_constructible_v<bool, odl::Result<int>>);
    STATIC_REQUIRE_FALSE(std::is_convertible_v<odl::Result<int>, bool>);
}

TEST_CASE("Result<void> works, for operations that only succeed or refuse",
          "[core][result]") {
    const auto check = [](bool ok) -> odl::Result<void> {
        if (!ok) return odl::err("CORE-F-002", "refused");
        return {};
    };
    REQUIRE(check(true).has_value());
    REQUIRE_FALSE(check(false).has_value());
    REQUIRE(check(false).error().id == "CORE-F-002");
}

TEST_CASE("errors do not leak between operations", "[core][result]") {
    // SPEC-template.md §5 R-ERR-1 and plan §5 constraint 5: diagnostic state is
    // scoped to one operation.  There is no accumulator to check, which is the
    // point — this test exists so that anyone who later adds one has to delete it.
    const auto a = halve(7);
    const auto b = halve(8);
    REQUIRE_FALSE(a.has_value());
    REQUIRE(b.has_value());
    REQUIRE(*b == 4);
}

// The km/metre crossing.  SPEC-frames FRAME-R-062: there is ONE named site, and
// this is the test that keeps it one.
TEST_CASE("units: the kilometre/metre crossing is named, exact and one-way-at-a-time", "[core]") {
    using namespace odl;
    static_assert(kMetresPerKilometre == 1000.0);
    static_assert(metres_from_km(1.0) == 1000.0);
    static_assert(km_from_metres(1000.0) == 1.0);

    // The round trip is correct to within one ulp, and NOT exact in general.
    // This test asserted exactness on the reasoning that 1000 is a power of ten
    // and therefore harmless; that is wrong, because a power of ten is not a
    // power of two, and 1e-9 fails it. The values where it is exact are the ones
    // whose scaled form happens to be representable, which is most of them and
    // not all.
    for (double v : {0.0, 1.0, -7331.0, 6378.1363, 1.0 / 3.0, 1e-9, 1e9}) {
        const double back = km_from_metres(metres_from_km(v));
        CHECK(std::abs(back - v) <= std::abs(v) * std::numeric_limits<double>::epsilon());
    }
    // Exact for the cases that matter to this tree: a geocentric radius in km
    // and an acceleration in m s^-2 both scale without rounding.
    CHECK(metres_from_km(7331.0) == 7331000.0);
    CHECK(km_from_metres(7331000.0) == 7331.0);

    const Vec3 a_m{1e-3, -2e-3, 3.5e-3};                 // an acceleration in m s^-2
    const Vec3 a_km = state_accel_km_s2_from_m_s2(a_m);
    CHECK(a_km.x == 1e-6);
    CHECK(a_km.y == -2e-6);
    CHECK(a_km.z == 3.5e-6);

    const Vec3 r_km{7331.0, 0.0, 0.0};
    CHECK(field_position_m_from_state_km(r_km).x == 7331000.0);

    // The crossing is a function call, never an implicit conversion: there is no
    // type that turns one into the other on its own.
    static_assert(std::is_same_v<decltype(state_accel_km_s2_from_m_s2(a_m)), Vec3>);
}
