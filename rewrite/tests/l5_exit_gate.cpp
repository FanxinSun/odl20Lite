// l5_exit_gate.cpp — L5's own exit gate (PLAN.md §3.6, the manager's own
// words): "every value resolves to a citation, and the library refuses to
// build if any does not." Checked once, tree-wide, across every block and
// constellation `modules/spacecraft` can construct -- not only within each
// block's own per-block suite (SPCR-A-001/A-010/A-015/A-021/A-025/etc., each
// scoped to one family) and not only the one hand-picked value SPCR-A-002
// shows the inherited guard firing on. SPEC-spacecraft.md SPCR-R-026,
// SPCR-A-033/034/035.
//
// RESOLVES, NOT JUST EXISTS (the manager's own fourth-review correction): a
// non-blank citation that names no source this tree has actually registered
// -- "see above", a typo'd source name -- would pass a bare non-blank check.
// Every citation is checked to CONTAIN at least one key from
// SPEC-spacecraft.md's own §2 (Normative sources) table -- read directly
// from the spec file at test time (`registered_source_keys`, below), not
// copied into this file by hand, so the key list is structurally incapable
// of drifting from the spec the way a hardcoded copy could. Scoped to
// SPEC-spacecraft.md alone, not every spec's own source table: every
// citation this module can produce is checked, this round, to name one of
// ITS OWN module's registered sources (RS14, GALSC, SPI_QZS1_B, SATMOD,
// IGSMETA, MSGA15, SMSD24, FLGA92, FLGA96) -- the attitude specs
// (SPEC-jason-attitude.md and its siblings) register their own, for
// modules/attitude's own citations, a different module's own concern this
// file does not construct.
//
// Linked here, not inside modules/spacecraft/tests/, on the SAME "one file,
// one cross-cutting purpose, one reference point" reasoning tests/l2_floors.cpp's
// own header states and tests/spot5_appendix_tests.cpp already applies to this
// exact module: the property under test spans every satellite family this
// module has, not one of them.

#include <catch2/catch_test_macros.hpp>

#include <odl/macromodel/body_direction.hpp>
#include <odl/macromodel/cited.hpp>
#include <odl/macromodel/macromodel.hpp>
#include <odl/spacecraft/beidou.hpp>
#include <odl/spacecraft/galileo.hpp>
#include <odl/spacecraft/glonass.hpp>
#include <odl/spacecraft/jason.hpp>
#include <odl/spacecraft/qzss.hpp>
#include <odl/spacecraft/sentinel6.hpp>
#include <odl/spacecraft/spacecraft.hpp>

#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

using namespace odl;
using namespace odl::macromodel;
using namespace odl::spacecraft;

#ifndef ODL_SPEC_DIR
#error "ODL_SPEC_DIR must be supplied by tests/CMakeLists.txt"
#endif

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

/// Every source key SPEC-spacecraft.md's own section 2 (Normative sources)
/// registers, read directly from the spec file, not copied by hand -- the
/// same "the denominator is read, never guessed" discipline speccheck.py's
/// own module docstring states, translated here to C++. Scoped to section 2
/// specifically -- bounded by the next "## " heading, the SAME "split by
/// heading region" technique speccheck.py's own split_coverage() already
/// uses for its own Coverage section -- not the whole file, so a table row
/// elsewhere that happens to look like a bare `KEY` (none currently do; a
/// requirement id like `SPCR-R-001` contains hyphens and cannot match this
/// pattern regardless) is never mistaken for a registered source.
std::vector<std::string> registered_source_keys(const std::string& spec_path) {
    std::ifstream in(spec_path);
    REQUIRE(in.is_open());
    std::stringstream buf;
    buf << in.rdbuf();
    const std::string text = buf.str();

    const auto sec2 = text.find("\n## 2.");
    REQUIRE(sec2 != std::string::npos);
    const auto sec3 = text.find("\n## ", sec2 + 1);
    const std::string section =
        text.substr(sec2, sec3 == std::string::npos ? std::string::npos : sec3 - sec2);

    static const std::regex kKeyRe(R"(^\|\s*`([A-Z][A-Z0-9_]{1,15})`\s*\|)");
    std::vector<std::string> keys;
    std::istringstream lines(section);
    std::string line;
    while (std::getline(lines, line)) {
        std::smatch m;
        if (std::regex_search(line, m, kKeyRe)) keys.push_back(m[1].str());
    }
    REQUIRE(keys.size() >= 9);  // FLGA92, FLGA96, RS14, MSGA15, IGSMETA, SMSD24, GALSC, SPI_QZS1_B, SATMOD -- a sanity floor, not the exact count, so a genuinely new source added later doesn't need this test edited too
    return keys;
}

/// A citation RESOLVES when it names at least one source this tree has
/// actually registered -- containing the key as a substring, not equalling
/// it, since every citation in this module states more than the bare key
/// (a table number, a section, a reason) around it.
bool resolves(const std::string& citation, const std::vector<std::string>& keys) {
    for (const auto& k : keys)
        if (citation.find(k) != std::string::npos) return true;
    return false;
}

enum class Expect {
    BuildsCited,             ///< succeeds; every citation is non-blank AND resolves
    Refuses,                 ///< fails, optionally with a named id
    CitationDoesNotResolve,  ///< SPCR-A-035 only: succeeds, but at least one non-blank citation does NOT resolve
};

/// SPCR-A-033's own shared path, one function for every entry this file
/// constructs: a successful entry has every citation walked, checked
/// non-blank (macromodel::is_blank -- the SAME predicate MCRM-F-001 itself
/// checks, cited.hpp, not a re-implementation that could silently drift from
/// it) AND checked to RESOLVE (`resolves`, above) -- a citation has to name
/// a source this tree has registered, not merely exist. A refusing entry is
/// checked to actually refuse, and, when named, with the right id.
/// SPCR-A-034/035 each run a deliberately-broken entry through this SAME
/// function rather than a second, disconnected one -- the point of both
/// tests is that THIS function catches it.
void audit_entry(odl::Result<Macromodel, SpacecraftError> r, Expect expect,
                  const char* refusal_id = nullptr) {
    static const std::vector<std::string> kKeys =
        registered_source_keys(std::string(ODL_SPEC_DIR) + "/SPEC-spacecraft.md");
    if (expect == Expect::BuildsCited) {
        REQUIRE(r.has_value());
        for_each_citation(*r, [&](const std::string& c) {
            CHECK_FALSE(is_blank(c));
            CAPTURE(c);
            CHECK(resolves(c, kKeys));
        });
    } else if (expect == Expect::Refuses) {
        REQUIRE_FALSE(r.has_value());
        if (refusal_id != nullptr) CHECK(r.error().id == refusal_id);
    } else {
        REQUIRE(r.has_value());
        bool found_unresolved = false;
        for_each_citation(*r, [&](const std::string& c) {
            if (!is_blank(c) && !resolves(c, kKeys)) found_unresolved = true;
        });
        CHECK(found_unresolved);
    }
}

}  // namespace

// --- SPCR-A-033 ----------------------------------------------------------------

TEST_CASE("SPCR-A-033  every library entry this module can construct is built "
          "(or, where it refuses in this version, shown refusing with its own "
          "reason) and every constructed value carries a citation that "
          "RESOLVES to a source this tree has registered, not merely a "
          "non-blank string -- checked once, tree-wide, across every block "
          "and constellation, not only within each block's own per-block suite",
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

// --- SPCR-A-035 ----------------------------------------------------------------

namespace {

/// SPCR-A-035's own injected entry: every citation is non-blank -- `cited()`
/// has nothing to refuse -- but none of them names a source this tree has
/// actually registered, the exact shape a copy-pasted "see above" or a
/// typo'd source name would take. Mirrors a real single-face construction
/// closely enough to be a fair "entry" (MacromodelBuilder, one FlatSurface,
/// a cited mass and centre of mass), the same standard SPCR-A-002's own
/// bus_face()/assemble() mirror already sets, energy-conserving
/// (0.5+0.3+0.2=1) so MCRM-F-007 has no reason to refuse it either --
/// isolating the ONE thing this entry is wrong about.
odl::Result<Macromodel, SpacecraftError> entry_with_unregistered_citation() {
    const std::string citation = "Wikipedia, accessed 2026-09-25 -- not a source this tree registers";
    auto area = cited(1.0, citation);
    auto absorptivity = cited(0.5, citation);
    auto specular = cited(0.3, citation);
    auto diffuse = cited(0.2, citation);
    auto normal = body_direction(Vec3{0.0, 0.0, 1.0});
    if (!area.has_value()) return odl::err(SpacecraftError{area.error().id, area.error().message});
    if (!absorptivity.has_value())
        return odl::err(SpacecraftError{absorptivity.error().id, absorptivity.error().message});
    if (!specular.has_value())
        return odl::err(SpacecraftError{specular.error().id, specular.error().message});
    if (!diffuse.has_value())
        return odl::err(SpacecraftError{diffuse.error().id, diffuse.error().message});
    if (!normal.has_value()) return odl::err(SpacecraftError{normal.error().id, normal.error().message});

    auto surf = flat_surface_body_fixed(*area, *normal, *absorptivity, *specular, *diffuse);
    if (!surf.has_value()) return odl::err(SpacecraftError{surf.error().id, surf.error().message});
    MacromodelBuilder b;
    b.add_surface(*surf);
    auto mass = cited(1000.0, citation);
    auto com = cited(Vec3{0.0, 0.0, 0.0}, citation);
    if (!mass.has_value()) return odl::err(SpacecraftError{mass.error().id, mass.error().message});
    if (!com.has_value()) return odl::err(SpacecraftError{com.error().id, com.error().message});
    b.set_mass(*mass).set_centre_of_mass(*com);

    auto m = std::move(b).build();
    if (!m.has_value()) return odl::err(SpacecraftError{m.error().id, m.error().message});
    return *m;
}

}  // namespace

TEST_CASE("SPCR-A-035  RESOLVES, not just EXISTS: an injected entry whose "
          "citations are all non-blank but name no source this tree has "
          "registered is caught by audit_entry -- the SAME function every "
          "real entry in SPCR-A-033 is checked through -- proving the "
          "resolve check fires and is not a tautology, the same rule-5 "
          "discipline SPCR-A-034 already applies to a blank citation",
          "[spacecraft][gate]") {
    audit_entry(entry_with_unregistered_citation(), Expect::CitationDoesNotResolve);
}
