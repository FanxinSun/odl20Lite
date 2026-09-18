// The step-1 gate: a refusal round-trips through the alias, and the contract
// the whole tree rests on behaves as SPEC-template.md §5 requires.

#include <odl/core/result.hpp>

#include <catch2/catch_test_macros.hpp>

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
