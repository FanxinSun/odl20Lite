// beidou_tests.cpp — SPEC-spacecraft.md §8, SPCR-A-024.

#include <catch2/catch_test_macros.hpp>

#include <odl/spacecraft/beidou.hpp>

using namespace odl;
using namespace odl::spacecraft;

// --- SPCR-A-024 ----------------------------------------------------------------

TEST_CASE("SPCR-A-024  beidou() refuses unconditionally, with SPCR-F-006, "
          "and the refusal's own message names every one of the five "
          "independent reasons (curved surfaces, no populated per-"
          "satellite value, the unread maneuver-yaw equations, no "
          "redistribution terms, no consumer)",
          "[spacecraft][beidou]") {
    auto result = beidou();
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().id == "SPCR-F-006");

    const std::string& msg = result.error().message;
    CHECK(msg.find("cylinders") != std::string::npos);
    CHECK(msg.find("parabolic") != std::string::npos);
    CHECK(msg.find("FILE-FORMAT specification") != std::string::npos);
    CHECK(msg.find("MANEUVER-YAW") != std::string::npos);
    CHECK(msg.find("OCR-garbled") != std::string::npos);
    CHECK(msg.find("REDISTRIBUTION") != std::string::npos);
    CHECK(msg.find("CONSUMER") != std::string::npos);
}
