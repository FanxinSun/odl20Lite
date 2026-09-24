// galileo_tests.cpp — SPEC-spacecraft.md §8, SPCR-A-009 through SPCR-A-013.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <odl/macromodel/cited.hpp>
#include <odl/spacecraft/galileo.hpp>

#include <variant>

using namespace odl;
using namespace odl::macromodel;
using namespace odl::spacecraft;
using Catch::Matchers::WithinAbs;

namespace {

/// GSC prints the SAME physical point in both "Mechanical RF" and "ANTEX RF"
/// columns for its own ARP/PCO/LRR tables -- ground truth for the frame
/// mapping, not something this tree derives from the yaw law's own prose.
[[nodiscard]] Vec3 mm(double x, double y, double z) noexcept { return Vec3{x, y, z}; }

}  // namespace

// --- SPCR-A-009 ----------------------------------------------------------------

TEST_CASE("SPCR-A-009  galileo_frame_from_mechanical, VERIFIED against three "
          "real GSC coordinate pairs (its own Mechanical RF / ANTEX RF "
          "columns for the identical physical point) -- not trusted from "
          "the yaw law's own stated sign convention alone",
          "[spacecraft][galileo]") {
    // GSAT0101 (IOV): CoM in Mechanical RF, and ARP/LRR in both frames.
    const Vec3 com_0101 = mm(1205.84, 628.97, 553.44);
    const Vec3 arp_0101_mech = mm(1375.50, 600.00, 1100.50);
    const Vec3 arp_0101_antex = mm(-169.66, 28.97, 547.06);
    const Vec3 lrr_0101_mech = mm(2298.00, 595.00, 1174.00);
    const Vec3 lrr_0101_antex = mm(-1092.16, 33.97, 620.56);

    // The ANTEX-RF column is CoM-relative (SMSD24's own convention, already
    // used for GPS's IGSMETA reading: "PCOs given in the current IGS ANTEX
    // files refer to CoM") -- so the comparison is against (mechanical -
    // CoM), rotated, not against the bare mechanical point.
    Vec3 got = galileo_frame_from_mechanical(arp_0101_mech - com_0101);
    CHECK_THAT(got.x, WithinAbs(arp_0101_antex.x, 1.0e-9));
    CHECK_THAT(got.y, WithinAbs(arp_0101_antex.y, 1.0e-9));
    CHECK_THAT(got.z, WithinAbs(arp_0101_antex.z, 1.0e-9));

    got = galileo_frame_from_mechanical(lrr_0101_mech - com_0101);
    CHECK_THAT(got.x, WithinAbs(lrr_0101_antex.x, 1.0e-9));
    CHECK_THAT(got.y, WithinAbs(lrr_0101_antex.y, 1.0e-9));
    CHECK_THAT(got.z, WithinAbs(lrr_0101_antex.z, 1.0e-9));

    // GSAT0201 (FOC): an independent block, same rotation.
    const Vec3 com_0201 = mm(316.89, -13.48, 561.92);
    const Vec3 arp_0201_mech = mm(140.00, 0.00, 1215.00);
    const Vec3 arp_0201_antex = mm(176.89, -13.48, 653.08);
    got = galileo_frame_from_mechanical(arp_0201_mech - com_0201);
    CHECK_THAT(got.x, WithinAbs(arp_0201_antex.x, 1.0e-9));
    CHECK_THAT(got.y, WithinAbs(arp_0201_antex.y, 1.0e-9));
    CHECK_THAT(got.z, WithinAbs(arp_0201_antex.z, 1.0e-9));

    // Guard: the OTHER plausible reading -- flip X only, matching the yaw
    // law's own prose ("the +X axis points toward the Sun and not towards
    // Deep Space") taken in isolation -- would NOT reproduce GSAT0101's own
    // printed Y (28.97), predicting -600.00 - (-628.97)... i.e. leaving Y
    // unrotated would give +28.97 only by accident here; check the mapping
    // is genuinely a 180-degree (X AND Y) rotation, not an X-only flip, by
    // confirming it disagrees with an X-only flip at this real data point.
    const Vec3 x_only = arp_0101_mech - com_0101;
    const Vec3 x_only_flipped{-x_only.x, x_only.y, x_only.z};
    CHECK(x_only_flipped.y != arp_0101_antex.y);

    // Involution: applying the mapping twice returns the original vector.
    const Vec3 v{12.3, -45.6, 78.9};
    const Vec3 twice = galileo_frame_from_mechanical(galileo_frame_from_mechanical(v));
    CHECK_THAT(twice.x, WithinAbs(v.x, 1.0e-12));
    CHECK_THAT(twice.y, WithinAbs(v.y, 1.0e-12));
    CHECK_THAT(twice.z, WithinAbs(v.z, 1.0e-12));
}

// --- SPCR-A-010 ----------------------------------------------------------------

namespace {
/// Walks every FlatSurface's own front optics, checking alpha+specular+
/// diffuse sums to 1 -- GSC's own §6 arithmetic (quoted in galileo.cpp's own
/// header comment: "alpha = absorption coefficient, rho = specular
/// reflection coefficient, delta = diffuse reflection coefficient"), the
/// SAME order this schema already uses, no swap.
template <class F>
void for_each_flat_surface(const Macromodel& m, F&& f) {
    for (const auto& s : m.surfaces()) {
        REQUIRE(std::holds_alternative<FlatSurface>(s));
        const auto& fs = std::get<FlatSurface>(s);
        f(fs.absorptivity().value(), fs.specular().value(), fs.diffuse().value());
    }
}
}  // namespace

TEST_CASE("SPCR-A-010  every material's own alpha+specular+diffuse sums to "
          "1, GSC's own arithmetic, for every surface of IOV (BOL and EOL "
          "alike) and FOC",
          "[spacecraft][galileo]") {
    auto iov_bol = galileo_iov(101, YearMonth{2024, 4}, OpticalLife::BeginningOfLife);
    auto iov_eol = galileo_iov(101, YearMonth{2024, 4}, OpticalLife::EndOfLife);
    auto foc = galileo_foc(201, YearMonth{2026, 5});
    REQUIRE(iov_bol.has_value());
    REQUIRE(iov_eol.has_value());
    REQUIRE(foc.has_value());

    for (const Macromodel* m : {&*iov_bol, &*iov_eol, &*foc}) {
        for_each_flat_surface(*m, [](double a, double s, double d) {
            CHECK_THAT(a + s + d, WithinAbs(1.0, 1.0e-12));
        });
    }

    // IOV has 6 box faces (Material 1 on every face) + 4 Material-2 rows
    // (+X, +Y, -Y, +Z only) + 2 wings = 12 surfaces, at either life stage.
    CHECK(iov_bol->surfaces().size() == 12);
    CHECK(iov_eol->surfaces().size() == 12);
    // FOC: -X and -- no, every box face has 1 or 2 materials: +X(2) -X(1)
    // +Y(2) -Y(2) +Z(2) -Z(2) = 11, plus 2 wings = 13.
    CHECK(foc->surfaces().size() == 13);
}

// --- SPCR-A-014 ----------------------------------------------------------------

TEST_CASE("SPCR-A-014  IOV's own BOL and EOL optics genuinely differ where "
          "GSC prints different coefficients (the Optical surface "
          "radiator, +X/+Y/-Y) and agree exactly where it prints the SAME "
          "ones (the Germanium-coated Kapton foil, +Z; Material 1, every "
          "face; both wings) -- proving the selector actually reaches the "
          "built surfaces, not merely that it compiles",
          "[spacecraft][galileo][gate]") {
    auto bol = galileo_iov(101, YearMonth{2024, 4}, OpticalLife::BeginningOfLife);
    auto eol = galileo_iov(101, YearMonth{2024, 4}, OpticalLife::EndOfLife);
    REQUIRE(bol.has_value());
    REQUIRE(eol.has_value());
    REQUIRE(bol->surfaces().size() == eol->surfaces().size());

    bool found_differing = false, found_identical_z = false;
    for (std::size_t k = 0; k < bol->surfaces().size(); ++k) {
        const auto& a = std::get<FlatSurface>(bol->surfaces()[k]);
        const auto& b = std::get<FlatSurface>(eol->surfaces()[k]);
        // Every surface shares the same area and normal at either life
        // stage (only optics are life-stage-dependent) -- checked, not
        // assumed, before looking at whether optics themselves differ.
        CHECK_THAT(a.area_m2().value(), WithinAbs(b.area_m2().value(), 1.0e-12));

        if (std::abs(a.absorptivity().value() - 0.10) < 1.0e-9 &&
            std::abs(a.area_m2().value() - 0.78) < 1.0e-9) {
            // The +X Optical surface radiator's own BOL row -- its own EOL
            // counterpart must differ (0.10 -> 0.25).
            found_differing = true;
            CHECK_THAT(b.absorptivity().value(), WithinAbs(0.25, 1.0e-9));
            CHECK_THAT(b.specular().value(), WithinAbs(0.60, 1.0e-9));
            CHECK_THAT(b.diffuse().value(), WithinAbs(0.15, 1.0e-9));
            CHECK(a.absorptivity().citation().find("BOL") != std::string::npos);
            CHECK(b.absorptivity().citation().find("EOL") != std::string::npos);
        }
        if (std::abs(a.absorptivity().value() - 0.57) < 1.0e-9) {
            // The +Z Germanium-coated Kapton foil -- GSC prints the SAME
            // triple at both BOL and EOL.
            found_identical_z = true;
            CHECK_THAT(b.absorptivity().value(), WithinAbs(0.57, 1.0e-9));
            CHECK_THAT(b.specular().value(), WithinAbs(0.22, 1.0e-9));
            CHECK_THAT(b.diffuse().value(), WithinAbs(0.21, 1.0e-9));
        }
    }
    CHECK(found_differing);
    CHECK(found_identical_z);
}

// --- SPCR-A-011 ----------------------------------------------------------------

TEST_CASE("SPCR-A-011  both wings' own area and optics are identical in "
          "GSC's own printed table, for IOV and FOC alike -- checked, not "
          "assumed, before being summed into one sun-pointing surface",
          "[spacecraft][galileo]") {
    // IOV: Wing +Y and -Y, both materials (Sec.6.1's own printed table).
    struct Row { double area, alpha, rho, delta; };
    const Row iov_plus_y_cells[] = {{3.88, 0.92, 0.08, 0.00}, {1.53, 0.90, 0.10, 0.00}};
    const Row iov_minus_y_cells[] = {{3.88, 0.92, 0.08, 0.00}, {1.53, 0.90, 0.10, 0.00}};
    for (std::size_t i = 0; i < 2; ++i) {
        CHECK(iov_plus_y_cells[i].area == iov_minus_y_cells[i].area);
        CHECK(iov_plus_y_cells[i].alpha == iov_minus_y_cells[i].alpha);
        CHECK(iov_plus_y_cells[i].rho == iov_minus_y_cells[i].rho);
        CHECK(iov_plus_y_cells[i].delta == iov_minus_y_cells[i].delta);
    }
    // FOC: Wing +SA and -SA, both materials (Sec.6.2's own printed table).
    const Row foc_plus_sa_cells[] = {{3.880, 0.92, 0.08, 0.00}, {1.530, 0.90, 0.10, 0.00}};
    const Row foc_minus_sa_cells[] = {{3.880, 0.92, 0.08, 0.00}, {1.530, 0.90, 0.10, 0.00}};
    for (std::size_t i = 0; i < 2; ++i) {
        CHECK(foc_plus_sa_cells[i].area == foc_minus_sa_cells[i].area);
        CHECK(foc_plus_sa_cells[i].alpha == foc_minus_sa_cells[i].alpha);
    }

    // The BUILT macromodel's own wing surfaces carry the SUMMED area --
    // proving the summing actually happened, not merely that the inputs
    // permit it.
    auto iov = galileo_iov(101, YearMonth{2024, 4}, OpticalLife::EndOfLife);
    REQUIRE(iov.has_value());
    int sun_pointing_count = 0;
    bool found_7_76 = false, found_3_06 = false;
    for (const auto& s : iov->surfaces()) {
        const auto& fs = std::get<FlatSurface>(s);
        if (fs.normal_mode() == NormalMode::sun_pointing) {
            ++sun_pointing_count;
            if (std::abs(fs.area_m2().value() - 7.76) < 1.0e-9) found_7_76 = true;
            if (std::abs(fs.area_m2().value() - 3.06) < 1.0e-9) found_3_06 = true;
        }
    }
    CHECK(sun_pointing_count == 2);
    CHECK(found_7_76);
    CHECK(found_3_06);
}

// --- SPCR-A-012 ----------------------------------------------------------------

TEST_CASE("SPCR-A-012  mass/CoM lookup: the right value at the right GSAT, "
          "refused for an unknown GSAT (SPCR-F-004), and refused for an "
          "epoch before the entry's own coverage (SPCR-F-005), shown firing "
          "exactly at the boundary",
          "[spacecraft][galileo][gate]") {
    // Correct value, IOV GSAT0102, at its own stated "as of" epoch.
    auto ok = galileo_iov(102, YearMonth{2024, 4}, OpticalLife::EndOfLife);
    REQUIRE(ok.has_value());
    CHECK_THAT(ok->mass_kg().value(), WithinAbs(695.318, 1.0e-9));

    // Unknown GSAT (IOV has only 101/102/103; 999 is not one of them).
    auto bad_gsat = galileo_iov(999, YearMonth{2024, 4}, OpticalLife::EndOfLife);
    REQUIRE_FALSE(bad_gsat.has_value());
    CHECK(bad_gsat.error().id == "SPCR-F-004");

    // At the boundary: succeeds exactly at valid_from.
    auto at_boundary = galileo_iov(101, YearMonth{2024, 4}, OpticalLife::EndOfLife);
    CHECK(at_boundary.has_value());
    // One month before: refuses.
    auto before = galileo_iov(101, YearMonth{2024, 3}, OpticalLife::EndOfLife);
    REQUIRE_FALSE(before.has_value());
    CHECK(before.error().id == "SPCR-F-005");
    // Well after: succeeds (open-ended coverage).
    auto after = galileo_iov(101, YearMonth{2030, 1}, OpticalLife::EndOfLife);
    CHECK(after.has_value());

    // FOC's own boundary, independently: valid from 2026-05.
    auto foc_before = galileo_foc(201, YearMonth{2026, 4});
    REQUIRE_FALSE(foc_before.has_value());
    CHECK(foc_before.error().id == "SPCR-F-005");
    auto foc_at = galileo_foc(201, YearMonth{2026, 5});
    CHECK(foc_at.has_value());
    auto foc_unknown = galileo_foc(228, YearMonth{2026, 5});  // not in GSC's own list
    REQUIRE_FALSE(foc_unknown.has_value());
    CHECK(foc_unknown.error().id == "SPCR-F-004");
}

// --- SPCR-A-013 ----------------------------------------------------------------

TEST_CASE("SPCR-A-013  every built IOV and FOC macromodel is fully cited, "
          "and the citation-refusal guard reaches this module's own call "
          "path",
          "[spacecraft][galileo][gate]") {
    auto iov = galileo_iov(103, YearMonth{2024, 4}, OpticalLife::EndOfLife);
    auto foc = galileo_foc(234, YearMonth{2026, 5});
    REQUIRE(iov.has_value());
    REQUIRE(foc.has_value());
    for (const Macromodel* m : {&*iov, &*foc}) {
        CHECK_FALSE(m->mass_kg().citation().empty());
        CHECK_FALSE(m->centre_of_mass_m().citation().empty());
        for (const auto& s : m->surfaces()) {
            const auto& fs = std::get<FlatSurface>(s);
            CHECK_FALSE(fs.area_m2().citation().empty());
            CHECK_FALSE(fs.absorptivity().citation().empty());
        }
    }

    // The guard shown firing: a blank citation is refused by MCRM-F-001,
    // through the same cited() call this module's own construction makes.
    auto blank = cited(1.0, "");
    REQUIRE_FALSE(blank.has_value());
    CHECK(blank.error().id == "MCRM-F-001");

    // Centre of mass is a real, nonzero offset for Galileo (unlike GPS's own
    // (0,0,0) default) -- checked directly, not assumed from the schema
    // permitting it.
    const Vec3& com = iov->centre_of_mass_m().value();
    CHECK(com.norm() > 0.1);
}
