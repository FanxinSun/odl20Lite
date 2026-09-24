// l5_exit_gate.cpp — L5's own exit gate (PLAN.md §3.6, the manager's own
// words): "every value resolves to a citation, and the library refuses to
// build if any does not." Checked once, tree-wide, across every block and
// constellation `modules/spacecraft` can construct -- not only within each
// block's own per-block suite (SPCR-A-001/A-010/A-015/A-021/A-025/etc., each
// scoped to one family) and not only the one hand-picked value SPCR-A-002
// shows the inherited guard firing on. SPEC-spacecraft.md SPCR-R-026,
// SPCR-A-033/034.
//
// Linked here, not inside modules/spacecraft/tests/, on the SAME "one file,
// one cross-cutting purpose, one reference point" reasoning tests/l2_floors.cpp's
// own header states and tests/spot5_appendix_tests.cpp already applies to this
// exact module: the property under test spans every satellite family this
// module has, not one of them.

#include <catch2/catch_test_macros.hpp>

#include <odl/macromodel/cited.hpp>
#include <odl/macromodel/macromodel.hpp>
#include <odl/spacecraft/beidou.hpp>
#include <odl/spacecraft/galileo.hpp>
#include <odl/spacecraft/glonass.hpp>
#include <odl/spacecraft/jason.hpp>
#include <odl/spacecraft/qzss.hpp>
#include <odl/spacecraft/sentinel6.hpp>
#include <odl/spacecraft/spacecraft.hpp>

#include <string>
#include <type_traits>
#include <variant>

using namespace odl;
using namespace odl::macromodel;
using namespace odl::spacecraft;

namespace {

/// Every Cited<T> a built Macromodel can hold, visited once: mass, centre of
/// mass, and every surface's own area plus every OpticalTriple it carries.
/// front(Band::infrared)/back(Band::infrared) fall back to the visible
/// triple when no separate infrared row was supplied (BandedOptics::in()'s
/// own documented fallback) -- visited here regardless, since the fallback
/// value is itself an already-cited Cited<double>, not a gap this walk could
/// miss; back() is visited only when actually present (one-sided surfaces,
/// still the majority, correctly contribute nothing there).
template <class F>
void for_each_citation(const Macromodel& m, F&& f) {
    auto visit_triple = [&](const OpticalTriple& t) {
        f(t.absorptivity.citation());
        f(t.specular.citation());
        f(t.diffuse.citation());
    };
    f(m.mass_kg().citation());
    f(m.centre_of_mass_m().citation());
    for (const auto& surf : m.surfaces()) {
        std::visit([&](const auto& s) {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, FlatSurface>) {
                f(s.area_m2().citation());
                visit_triple(s.front(Band::visible));
                visit_triple(s.front(Band::infrared));
                if (auto bv = s.back(Band::visible)) visit_triple(*bv);
                if (auto bi = s.back(Band::infrared)) visit_triple(*bi);
            } else {
                f(s.cross_section_area_m2.citation());
                visit_triple(s.in(Band::visible));
                visit_triple(s.in(Band::infrared));
            }
        }, surf);
    }
}

enum class Expect { BuildsCited, Refuses };

/// SPCR-A-033's own shared path, one function for every entry this file
/// constructs: a successful entry has every citation walked and checked
/// non-blank (macromodel::is_blank -- the SAME predicate MCRM-F-001 itself
/// checks, cited.hpp, not a re-implementation that could silently drift from
/// it); a refusing entry is checked to actually refuse, and, when named,
/// with the right id. SPCR-A-034 runs a deliberately-broken entry through
/// this SAME function rather than a second, disconnected one -- the point
/// of that test is that THIS function catches it.
void audit_entry(odl::Result<Macromodel, SpacecraftError> r, Expect expect,
                  const char* refusal_id = nullptr) {
    if (expect == Expect::BuildsCited) {
        REQUIRE(r.has_value());
        for_each_citation(*r, [](const std::string& c) { CHECK_FALSE(is_blank(c)); });
    } else {
        REQUIRE_FALSE(r.has_value());
        if (refusal_id != nullptr) CHECK(r.error().id == refusal_id);
    }
}

}  // namespace

// --- SPCR-A-033 ----------------------------------------------------------------

TEST_CASE("SPCR-A-033  every library entry this module can construct is built "
          "(or, where it refuses in this version, shown refusing with its own "
          "reason) and every constructed value carries a citation -- checked "
          "once, tree-wide, across every block and constellation, not only "
          "within each block's own per-block suite",
          "[spacecraft][gate]") {
    SECTION("GPS: I (all 8 named SVNs), II, IIA, IIR, IIR-M, IIF; IIIA refuses") {
        for (int svn : {3, 4, 6, 8, 9, 10, 11}) audit_entry(gps_block_i(svn), Expect::BuildsCited);
        audit_entry(gps_block_ii_iia(false), Expect::BuildsCited);
        audit_entry(gps_block_ii_iia(true), Expect::BuildsCited);
        audit_entry(gps_block_iir(), Expect::BuildsCited);
        audit_entry(gps_block_iir_m(), Expect::BuildsCited);
        audit_entry(gps_block_iif(), Expect::BuildsCited);
        audit_entry(gps_block_iiia(), Expect::Refuses, "SPCR-F-003");
    }

    SECTION("Galileo: every currently-listed GSAT, IOV and FOC alike") {
        // After both kIovValidFrom (2024-04) and kFocValidFrom (2026-05), galileo.cpp
        // -- comfortably in-coverage for every satellite either table lists.
        constexpr YearMonth kInCoverage{2026, 9};

        int iov_built = 0, iov_refused = 0;
        for (int gsat = 95; gsat <= 110; ++gsat) {
            for (OpticalLife life : {OpticalLife::BeginningOfLife, OpticalLife::EndOfLife}) {
                auto m = galileo_iov(gsat, kInCoverage, life);
                if (m.has_value()) ++iov_built; else ++iov_refused;
                audit_entry(std::move(m), m.has_value() ? Expect::BuildsCited : Expect::Refuses,
                           m.has_value() ? nullptr : "SPCR-F-004");
            }
        }
        // GSC's own IOV table, this round: exactly GSAT 101/102/103, both life stages.
        CHECK(iov_built == 3 * 2);
        // The scanned range also brackets known-absent GSATs (104-110, 95-100).
        CHECK(iov_refused > 0);

        int foc_built = 0, foc_refused = 0;
        for (int gsat = 195; gsat <= 240; ++gsat) {
            auto m = galileo_foc(gsat, kInCoverage);
            if (m.has_value()) ++foc_built; else ++foc_refused;
            audit_entry(std::move(m), m.has_value() ? Expect::BuildsCited : Expect::Refuses,
                       m.has_value() ? nullptr : "SPCR-F-004");
        }
        // GSC's own 29 currently-listed FOC satellites, this round: 201-227
        // except 205, plus 232-234 (kFocMassCom, galileo.cpp) -- a regression
        // guard the same shape SPCR-A-010's own "12 for IOV, 13 for FOC"
        // surface-count check already is, one level up.
        CHECK(foc_built == 29);
        CHECK(foc_refused > 0);   // 205, 228-231, and everything outside the table

        // Named, not only swept: an explicitly unknown GSAT (SPCR-F-004) ...
        audit_entry(galileo_foc(999, kInCoverage), Expect::Refuses, "SPCR-F-004");
        // ... and an explicitly out-of-coverage epoch on a KNOWN GSAT
        // (SPCR-F-005) -- a different refusal reason from the one above.
        audit_entry(galileo_iov(101, YearMonth{2020, 1}, OpticalLife::BeginningOfLife),
                   Expect::Refuses, "SPCR-F-005");
    }

    SECTION("GLONASS, GLONASS-M, GLONASS-K") {
        audit_entry(glonass(), Expect::BuildsCited);
        audit_entry(glonass_m(), Expect::BuildsCited);
        audit_entry(glonass_k(), Expect::BuildsCited);
    }

    SECTION("QZSS (QZS-1), both life stages") {
        audit_entry(qzss_1(QzssLife::BeginningOfLife), Expect::BuildsCited);
        audit_entry(qzss_1(QzssLife::EndOfLife), Expect::BuildsCited);
    }

    SECTION("BeiDou -- carried, refuses unconditionally") {
        audit_entry(beidou(), Expect::Refuses, "SPCR-F-006");
    }

    SECTION("Sentinel-6") {
        audit_entry(sentinel6(), Expect::BuildsCited);
    }

    SECTION("Jason-2, Jason-3 -- Jason-1 carried, refuses unconditionally") {
        audit_entry(jason2(JasonMassSource::Baseline), Expect::BuildsCited);
        audit_entry(jason3(JasonMassSource::Baseline), Expect::BuildsCited);
        audit_entry(jason1(), Expect::Refuses, "SPCR-F-007");
    }
}

// --- SPCR-A-034 ----------------------------------------------------------------

TEST_CASE("SPCR-A-034  the guard shown firing once more, through THIS file's "
          "own audit path (rule 5): an injected entry with a blank citation "
          "is caught by audit_entry, the SAME function every real entry "
          "above is checked through, not a separate, disconnected check",
          "[spacecraft][gate]") {
    // The SAME deliberately-blank citation SPCR-A-002 already shows cited()
    // itself refusing (modules/macromodel's own MCRM-F-001), wrapped here as
    // a Result<Macromodel, SpacecraftError> -- an "entry" shape -- so it can
    // be run through audit_entry rather than checked in isolation a second
    // time. What THIS test proves that SPCR-A-002 does not: that a refusing
    // entry, reached through SPCR-A-033's own audit path, is recognised as a
    // refusal with the right id -- not silently treated as success, and not
    // skipped because it doesn't have citations to walk. The SAME "prove the
    // checker itself catches the shape" rule 5 discipline this tree's other
    // checkers already use (speccheck.py's own duplicate-id injections,
    // MCRM-A-016's energy-conservation boundary), applied here to this
    // file's own C++ audit helper rather than a Python script.
    auto area = cited(4.250, "");  // blank, deliberately
    REQUIRE_FALSE(area.has_value());
    odl::Result<Macromodel, SpacecraftError> broken =
        odl::err(SpacecraftError{area.error().id, area.error().message});

    audit_entry(std::move(broken), Expect::Refuses, "MCRM-F-001");
}
