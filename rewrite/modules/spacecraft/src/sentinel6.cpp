// sentinel6.cpp — SPEC-spacecraft, L5 step 4: Sentinel-6 Michael Freilich.
//
// Source throughout: CNES, "DORIS satellites models implemented in POE
// processing," SALP-NT-BORD-OP-16137-CN, Ed.1/Rev.20 (2026-09-09), Sec.16
// ("16. SENTINEL-6 MICHAEL FREILICH"), fetched directly 2026-09-24 from
// ids-doris.org/documents/BC/satellites/DORISSatelliteModels.pdf, SHA256
// c0e7f3884888eef06a74d36b049e62e05de5092395c4c9e87edec53647ffb619 (pinned,
// this edition, per the RS14 secondary-source ruling extended to this note).
// Every row of Sec.16.3's own table checked against the rendered PDF page 39
// (not the flattened-text extraction alone), since two of its own normals do
// not renormalise cleanly -- see sentinel6.hpp's own header comment.

#include <odl/spacecraft/sentinel6.hpp>

#include <odl/macromodel/body_direction.hpp>
#include <odl/macromodel/cited.hpp>

#include <cmath>
#include <string>
#include <string_view>

namespace odl::spacecraft {
namespace {

using macromodel::Band;
using macromodel::BandedOptics;
using macromodel::cited;
using macromodel::FlatSurface;
using macromodel::flat_surface_body_fixed;
using macromodel::Macromodel;
using macromodel::MacromodelBuilder;
using macromodel::MacromodelError;
using macromodel::OpticalTriple;
using macromodel::body_direction;

constexpr std::string_view kCitation =
    "SATMOD, CNES SALP-NT-BORD-OP-16137-CN Ed.1/Rev.20 (2026-09-09), SHA256 "
    "c0e7f3884888eef06a74d36b049e62e05de5092395c4c9e87edec53647ffb619, Sec.16.3 "
    "(\"External diffusion: web site of the International DORIS Service\"; that "
    "site's own Legal Notice page is an unfilled placeholder, checked directly)";

constexpr std::string_view kCitationRenormalised =
    "SATMOD, CNES SALP-NT-BORD-OP-16137-CN Ed.1/Rev.20 (2026-09-09), SHA256 "
    "c0e7f3884888eef06a74d36b049e62e05de5092395c4c9e87edec53647ffb619, Sec.16.3 -- "
    "normal RENORMALISED from the source's own printed components, checked against "
    "the rendered PDF page (not the extraction alone): the printed 3-decimal value "
    "does not itself sum to unit length";

/// One row of Sec.16.3's own table: area, normal (as PRINTED -- not
/// necessarily unit length, see this file's own top comment), then
/// visible and infrared (spec, diff, abs) triples.
struct Row {
    double area_m2;
    Vec3 normal_raw;
    double vis_spec, vis_diff, vis_abs;
    double ir_spec, ir_diff, ir_abs;
};

/// `Sec.16.3`'s own twelve rows, in table order. Two normals repeat the
/// SAME direction (0,0,1): the source's own row 3 (area 11.83, the main
/// +Z deck) and row 12 (area 0.8123, a smaller +Z-facing element) -- built
/// as two separate co-normal FlatSurfaces, the same multi-material-per-face
/// pattern `galileo_iov`'s own +X/+Y/-Y/+Z faces already use.
constexpr Row kRows[] = {
    {4.149,  Vec3{ 1.0,   0.0,    0.0  }, 0.349, 0.041, 0.610, 0.100, 0.800, 0.100},
    {3.941,  Vec3{-1.0,   0.0,    0.0  }, 0.546, 0.042, 0.412, 0.100, 0.800, 0.100},
    {11.83,  Vec3{ 0.0,   0.0,    1.0  }, 0.571, 0.016, 0.413, 0.100, 0.800, 0.100},
    {2.072,  Vec3{ 0.0,   0.0,   -1.0  }, 0.660, 0.030, 0.310, 0.100, 0.800, 0.100},
    {8.65,   Vec3{ 0.0,   0.616, -0.788}, 0.139, 0.316, 0.545, 0.100, 0.800, 0.100},
    {8.65,   Vec3{ 0.0,  -0.616, -0.788}, 0.139, 0.316, 0.545, 0.100, 0.800, 0.100},
    {3.76,   Vec3{ 0.0,   0.616,  0.788}, 0.013, 0.164, 0.823, 0.100, 0.800, 0.100},
    {3.76,   Vec3{ 0.0,  -0.616,  0.788}, 0.013, 0.164, 0.823, 0.100, 0.800, 0.100},
    {1.329,  Vec3{ 0.0,   1.0,    0.0  }, 0.506, 0.040, 0.454, 0.100, 0.800, 0.100},
    {1.329,  Vec3{ 0.0,  -1.0,    0.0  }, 0.506, 0.040, 0.454, 0.100, 0.800, 0.100},
    {0.92,   Vec3{ 0.469, 0.0,   -0.833}, 0.000, 0.080, 0.920, 0.100, 0.800, 0.100},
    {0.8123, Vec3{ 0.0,   0.0,    1.0  }, 0.190, 0.560, 0.250, 0.100, 0.800, 0.100},
};

[[nodiscard]] odl::Result<FlatSurface, MacromodelError> build_row(const Row& row) {
    const double n = row.normal_raw.norm();
    const bool already_unit = std::abs(n - 1.0) <= 1e-9;
    const Vec3 unit_normal{row.normal_raw.x / n, row.normal_raw.y / n, row.normal_raw.z / n};
    const std::string citation(already_unit ? kCitation : kCitationRenormalised);

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
    auto normal = body_direction(unit_normal);
    if (!normal.has_value()) return odl::err(normal.error());

    OpticalTriple infrared{*ir_abs, *ir_spec, *ir_diff};
    return flat_surface_body_fixed(*area, *normal, *vis_abs, *vis_spec, *vis_diff, infrared);
}

}  // namespace

odl::Result<Macromodel, SpacecraftError> sentinel6() {
    MacromodelBuilder b;
    for (const Row& row : kRows) {
        auto s = build_row(row);
        if (!s.has_value()) return odl::err(SpacecraftError{s.error().id, s.error().message});
        b.add_surface(*s);
    }

    const std::string mass_citation =
        "SATMOD, CNES SALP-NT-BORD-OP-16137-CN Ed.1/Rev.20 (2026-09-09), SHA256 "
        "c0e7f3884888eef06a74d36b049e62e05de5092395c4c9e87edec53647ffb619, Sec.16.1's own "
        "baseline -- the note's own per-epoch offset file (s6amass.txt, confirmed openly "
        "retrievable, ~30 rows) is NOT wired into an epoch lookup this round (SPCR-Q, "
        "SPEC-spacecraft.md), the baseline used unqualified";
    auto mass = cited(1191.831, mass_citation);
    if (!mass.has_value()) return odl::err(SpacecraftError{mass.error().id, mass.error().message});
    auto com = cited(Vec3{1.5274, -0.0073, 0.0373}, mass_citation);
    if (!com.has_value()) return odl::err(SpacecraftError{com.error().id, com.error().message});
    b.set_mass(*mass).set_centre_of_mass(*com);

    auto m = std::move(b).build();
    if (!m.has_value()) return odl::err(SpacecraftError{m.error().id, m.error().message});
    return *m;
}

}  // namespace odl::spacecraft
