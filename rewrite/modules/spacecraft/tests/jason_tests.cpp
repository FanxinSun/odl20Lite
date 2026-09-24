// jason_tests.cpp — SPEC-spacecraft.md, SPCR-A-025 and SPCR-A-030..032
// (Jason-2, Jason-3, Jason-1).

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/macromodel/cited.hpp>
#include <odl/spacecraft/jason.hpp>

#include <variant>

using namespace odl;
using namespace odl::macromodel;
using namespace odl::spacecraft;
using Catch::Matchers::WithinAbs;

// --- SPCR-A-025: Jason-2 and Jason-3 are cell-by-cell identical ------------

TEST_CASE("SPCR-A-025  jason2()/jason3(): the source's own stated equality "
          "(\"the macro-model is the same as for Jason-3\"/\"identical for "
          "the two satellites\") checked cell by cell, not merely quoted; "
          "8 surfaces each (6 body + 2 solar array), every visible AND "
          "infrared triple sums to 1; every surface body-fixed (NOT "
          "sun-pointing, the solar array included, jason.hpp's own header "
          "comment)",
          "[spacecraft][jason]") {
    auto j2 = jason2(JasonMassSource::Baseline);
    auto j3 = jason3(JasonMassSource::Baseline);
    REQUIRE(j2.has_value());
    REQUIRE(j3.has_value());
    REQUIRE(j2->surfaces().size() == 8);
    REQUIRE(j3->surfaces().size() == 8);

    for (std::size_t k = 0; k < 8; ++k) {
        const auto& a = std::get<FlatSurface>(j2->surfaces()[k]);
        const auto& b = std::get<FlatSurface>(j3->surfaces()[k]);
        CHECK(a.normal_mode() == NormalMode::body_fixed);
        CHECK(b.normal_mode() == NormalMode::body_fixed);
        CHECK_THAT(a.area_m2().value(), WithinAbs(b.area_m2().value(), 1.0e-12));
        CHECK_THAT(a.absorptivity().value(), WithinAbs(b.absorptivity().value(), 1.0e-12));
        CHECK_THAT(a.specular().value(), WithinAbs(b.specular().value(), 1.0e-12));
        CHECK_THAT(a.diffuse().value(), WithinAbs(b.diffuse().value(), 1.0e-12));
        // Citations differ (each satellite cited to its OWN section),
        // proving this is independent citation, not a silent call-through
        // (jason.hpp's own header comment, `gps_block_iir_m`'s own
        // "citation names the inheritance" precedent applied in reverse:
        // here the values agree but the citations must NOT be identical).
        CHECK(a.absorptivity().citation() != b.absorptivity().citation());

        const auto vis_a = a.front(Band::visible);
        CHECK_THAT(vis_a.absorptivity.value() + vis_a.specular.value() + vis_a.diffuse.value(),
                   WithinAbs(1.0, 1.0e-9));
        // Infrared does NOT sum to exactly 1 for every row, unlike visible --
        // a genuine, small (<=0.002) source-side rounding property, checked
        // directly (Sec.7.3/12.3's own 4-decimal print, e.g. the -Y row's
        // own 0.104+0.569+0.328=1.001), not a defect in this file's own
        // transcription. Tolerance widened for THIS check only, the
        // deviation itself recorded in SPEC-spacecraft.md rather than
        // silently absorbed by a loose tolerance nobody explains.
        const auto ir_a = a.front(Band::infrared);
        CHECK_THAT(ir_a.absorptivity.value() + ir_a.specular.value() + ir_a.diffuse.value(),
                   WithinAbs(1.0, 2.0e-3));
    }
}

// --- SPCR-A-030: solar array is body-fixed at a FIXED normal ---------------

TEST_CASE("SPCR-A-030  jason2(): the solar array rows carry a FIXED "
          "(+1,0,0)/(-1,0,0) body-frame normal exactly as Sec.7.3 prints "
          "them, not a sun-pointing surface -- a genuine difference from "
          "every GPS/Galileo/QZSS panel this tree has built before this "
          "round",
          "[spacecraft][jason]") {
    auto m = jason2(JasonMassSource::Baseline);
    REQUIRE(m.has_value());
    int sun_pointing_count = 0;
    int plus_x_9_8 = 0, minus_x_9_8 = 0;
    for (const auto& s : m->surfaces()) {
        const auto& fs = std::get<FlatSurface>(s);
        if (fs.normal_mode() == NormalMode::sun_pointing) ++sun_pointing_count;
        if (std::abs(fs.area_m2().value() - 9.8) < 1.0e-9) {
            REQUIRE(fs.normal_mode() == NormalMode::body_fixed);
            const Vec3& n = fs.body_fixed_normal()->vec();
            if (n.x > 0.0) ++plus_x_9_8; else ++minus_x_9_8;
        }
    }
    CHECK(sun_pointing_count == 0);
    CHECK(plus_x_9_8 == 1);
    CHECK(minus_x_9_8 == 1);
}

// --- SPCR-A-031: mass/CoM, per satellite ------------------------------------

TEST_CASE("SPCR-A-031  jason2()/jason3(): mass and centre of mass are each "
          "section's own baseline (505.9/509.6 kg), genuinely different "
          "between the two satellites",
          "[spacecraft][jason]") {
    auto j2 = jason2(JasonMassSource::Baseline);
    auto j3 = jason3(JasonMassSource::Baseline);
    REQUIRE(j2.has_value());
    REQUIRE(j3.has_value());
    CHECK_THAT(j2->mass_kg().value(), WithinAbs(505.9, 1.0e-9));
    CHECK_THAT(j2->centre_of_mass_m().value().x, WithinAbs(0.9768, 1.0e-9));
    CHECK_THAT(j3->mass_kg().value(), WithinAbs(509.6, 1.0e-9));
    CHECK_THAT(j3->centre_of_mass_m().value().x, WithinAbs(1.0023, 1.0e-9));
    CHECK(j2->mass_kg().value() != j3->mass_kg().value());
}

// --- SPCR-A-032: Jason-1 refuses, with all stated reasons -------------------

TEST_CASE("SPCR-A-032  jason1(): refuses unconditionally with SPCR-F-007; "
          "the refusal's own message names both reasons (non-energy-"
          "conserving optics, the 0.97 scale factor) and states no "
          "consumer needs it, checked by substring not merely asserted",
          "[spacecraft][jason]") {
    auto m = jason1();
    REQUIRE(!m.has_value());
    CHECK(m.error().id == "SPCR-F-007");
    const std::string& msg = m.error().message;
    CHECK(msg.find("0.97") != std::string::npos);
    CHECK(msg.find("tuning") != std::string::npos);
    CHECK(msg.find("0.5827") != std::string::npos);
    CHECK(msg.find("No consumer") != std::string::npos);
}
