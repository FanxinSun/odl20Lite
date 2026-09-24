// jason.cpp — SPEC-spacecraft, L5 step 4: Jason-2, Jason-3, Jason-1 (refused).
//
// Source throughout: CNES, "DORIS satellites models implemented in POE
// processing," SALP-NT-BORD-OP-16137-CN, Ed.1/Rev.20 (2026-09-09), Sec.7
// ("7. JASON-2"), Sec.12 ("12. JASON-3") and Sec.6 ("6. JASON-1"), fetched
// directly 2026-09-24, SHA256
// c0e7f3884888eef06a74d36b049e62e05de5092395c4c9e87edec53647ffb619 (pinned,
// this edition -- see jason.hpp's own header comment for why the pin matters
// here specifically: this session's own first draft used a stale, cached
// table before this file was written, caught by re-reading the fetch).

#include <odl/spacecraft/jason.hpp>

#include <odl/macromodel/body_direction.hpp>
#include <odl/macromodel/cited.hpp>

#include <string>
#include <string_view>

namespace odl::spacecraft {
namespace {

using macromodel::cited;
using macromodel::FlatSurface;
using macromodel::flat_surface_body_fixed;
using macromodel::Macromodel;
using macromodel::MacromodelBuilder;
using macromodel::MacromodelError;
using macromodel::OpticalTriple;
using macromodel::body_direction;

/// One row of the Jason-2/-3 shared table (both sections' own §.3 tables,
/// checked identical cell by cell, `SPCR-A-025`): area, unit normal (every
/// row of THIS table is already axis-aligned, unlike Sentinel-6's, so no
/// renormalisation is needed here), visible and infrared (spec, diff, abs).
struct Row {
    double area_m2;
    Vec3 normal;
    double vis_spec, vis_diff, vis_abs;
    double ir_spec, ir_diff, ir_abs;
};

/// `SALP-NT-BORD-OP-16137-CN` §7.3 ("the macro-model is the same as for
/// Jason-3") / §12.3 ("the original macromodel for the Jason-3 satellite"),
/// read directly from the CURRENT fetch, not this session's own earlier,
/// stale cache (`jason.hpp`'s own header comment). The solar-array rows
/// carry a FIXED body-frame normal, (+1,0,0)/(-1,0,0), exactly as the
/// source prints them -- NOT `flat_surface_sun_pointing` (this file's own
/// `build_row` below always builds body-fixed, deliberately, `jason.hpp`'s
/// own header comment).
constexpr Row kJason23Rows[] = {
    {0.783, Vec3{-1.0, 0.0, 0.0}, 0.2000, 0.7000, 0.1000, 0.0000, 0.9870, 0.0130},
    {0.783, Vec3{ 1.0, 0.0, 0.0}, 0.2000, 0.4000, 0.4000, 0.0000, 1.0000, 0.0000},
    {2.040, Vec3{ 0.0,-1.0, 0.0}, 0.5730, 0.3840, 0.0430, 0.1040, 0.5690, 0.3280},
    {2.040, Vec3{ 0.0, 1.0, 0.0}, 0.5390, 0.4240, 0.0370, 0.0890, 0.6270, 0.2830},
    {3.105, Vec3{ 0.0, 0.0,-1.0}, 0.2460, 0.7520, 0.0020, 0.0050, 0.9770, 0.0170},
    {3.105, Vec3{ 0.0, 0.0, 1.0}, 0.2130, 0.4530, 0.3340, 0.0370, 0.2870, 0.6760},
    // Solar array -- fixed normal as printed, see this file's own top comment.
    {9.8,   Vec3{ 1.0, 0.0, 0.0}, 0.1000, 0.2950, 0.6050, 0.0970, 0.0980, 0.8030},
    {9.8,   Vec3{-1.0, 0.0, 0.0}, 0.1000, 0.3000, 0.6000, 0.0350, 0.0350, 0.9310},
};

[[nodiscard]] odl::Result<FlatSurface, MacromodelError>
build_row(const Row& row, const std::string& citation) {
    auto area = cited(row.area_m2, citation);
    auto vis_abs = cited(row.vis_abs, citation);
    auto vis_spec = cited(row.vis_spec, citation);
    auto vis_diff = cited(row.vis_diff, citation);
    auto ir_abs = cited(row.ir_abs, citation);
    auto ir_spec = cited(row.ir_spec, citation);
    auto ir_diff = cited(row.ir_diff, citation);
    if (!area.has_value()) return odl::err(area.error());
    if (!vis_abs.has_value()) return odl::err(vis_abs.error());
    if (!vis_spec.has_value()) return odl::err(vis_spec.error());
    if (!vis_diff.has_value()) return odl::err(vis_diff.error());
    if (!ir_abs.has_value()) return odl::err(ir_abs.error());
    if (!ir_spec.has_value()) return odl::err(ir_spec.error());
    if (!ir_diff.has_value()) return odl::err(ir_diff.error());
    auto normal = body_direction(row.normal);
    if (!normal.has_value()) return odl::err(normal.error());

    OpticalTriple infrared{*ir_abs, *ir_spec, *ir_diff};
    return flat_surface_body_fixed(*area, *normal, *vis_abs, *vis_spec, *vis_diff, infrared);
}

[[nodiscard]] odl::Result<Macromodel, SpacecraftError>
build_jason23(std::string_view section_citation, double mass_kg, const Vec3& com_m) {
    MacromodelBuilder b;
    for (const Row& row : kJason23Rows) {
        auto s = build_row(row, std::string(section_citation));
        if (!s.has_value()) return odl::err(SpacecraftError{s.error().id, s.error().message});
        b.add_surface(*s);
    }
    const std::string mass_citation =
        std::string(section_citation) +
        ", baseline mass/CoM -- JasonMassSource::Baseline (jason.hpp's own header "
        "comment: no consumer needs the epoch-current value yet, so the per-event "
        "offset log's own ingestion is deferred, not built)";
    auto mass = cited(mass_kg, mass_citation);
    if (!mass.has_value()) return odl::err(SpacecraftError{mass.error().id, mass.error().message});
    auto com = cited(com_m, mass_citation);
    if (!com.has_value()) return odl::err(SpacecraftError{com.error().id, com.error().message});
    b.set_mass(*mass).set_centre_of_mass(*com);

    auto m = std::move(b).build();
    if (!m.has_value()) return odl::err(SpacecraftError{m.error().id, m.error().message});
    return *m;
}

}  // namespace

odl::Result<Macromodel, SpacecraftError> jason2(JasonMassSource source) {
    (void)source;  // exactly one legal value today, jason.hpp's own header comment
    return build_jason23(
        "SATMOD, CNES SALP-NT-BORD-OP-16137-CN Ed.1/Rev.20 (2026-09-09), SHA256 "
        "c0e7f3884888eef06a74d36b049e62e05de5092395c4c9e87edec53647ffb619, Sec.7.3/7.1",
        505.9, Vec3{0.9768, 0.0001, 0.0011});
}

odl::Result<Macromodel, SpacecraftError> jason3(JasonMassSource source) {
    (void)source;
    return build_jason23(
        "SATMOD, CNES SALP-NT-BORD-OP-16137-CN Ed.1/Rev.20 (2026-09-09), SHA256 "
        "c0e7f3884888eef06a74d36b049e62e05de5092395c4c9e87edec53647ffb619, Sec.12.3/12.1",
        509.6, Vec3{1.0023, 0.0000, -0.0021});
}

odl::Result<Macromodel, SpacecraftError> jason1() {
    return odl::err(SpacecraftError{"SPCR-F-007",
        "Jason-1 is carried, not built, in this version -- SALP-NT-BORD-OP-16137-CN Sec.6.3 "
        "states its own optics were \"slightly modified by tuning\" and carry an overall "
        "\"scale factor equal to 0.97... that multiplies the solar radiation pressure force\": "
        "neither a non-energy-conserving optical triple (checked directly, not assumed from "
        "the word \"tuning\" alone -- +X: 0.0938+0.2811+0.2078=0.5827; +Y: "
        "1.1880-0.0113-0.0113=1.1654, with NEGATIVE diffuse/absorptivity cells the schema's "
        "own physical triple cannot hold either) nor a satellite-wide force-scale factor is "
        "what MacromodelBuilder's own schema holds (SPEC-macromodel.md's own stated scope). "
        "No consumer needs Jason-1 currently -- no baseline in this tree reads this function, "
        "the same closing reason BeiDou's and GPS-IIIA's own refusals already carry."});
}

}  // namespace odl::spacecraft
