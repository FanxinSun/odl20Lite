// galileo.cpp — SPEC-spacecraft, L5 step 2: Galileo.
//
// Source throughout: European GNSS Service Centre (GSC), "Galileo Satellite
// Metadata" (gsc-europa.eu/support-to-developers/galileo-satellite-metadata),
// fetched directly 2026-09-24 (SPEC-spacecraft.md §2 GALSC). Table cells
// re-parsed from the page's own HTML <table> structure (rowspan/colspan
// expanded), not from an earlier flattened-text pass that lost some
// multi-material row alignment -- the manager's own instruction.

#include <odl/spacecraft/galileo.hpp>

#include <odl/macromodel/body_direction.hpp>
#include <odl/macromodel/cited.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace odl::spacecraft {
namespace {

using macromodel::Cited;
using macromodel::cited;
using macromodel::FlatSurface;
using macromodel::flat_surface_body_fixed;
using macromodel::flat_surface_sun_pointing;
using macromodel::Macromodel;
using macromodel::MacromodelBuilder;
using macromodel::MacromodelError;
using macromodel::body_direction;

/// GSC's own "mechanical RF" face labels (§6), axis-aligned. NOT this tree's
/// own frame -- every use below is piped through
/// `galileo_frame_from_mechanical()` before reaching `body_direction()`, so
/// the X/Y relabeling the rotation performs is computed, never done by hand.
enum class Face { PlusX, MinusX, PlusY, MinusY, PlusZ, MinusZ };

[[nodiscard]] Vec3 mechanical_face_normal(Face f) noexcept {
    switch (f) {
        case Face::PlusX:  return Vec3{ 1.0, 0.0, 0.0};
        case Face::MinusX: return Vec3{-1.0, 0.0, 0.0};
        case Face::PlusY:  return Vec3{ 0.0, 1.0, 0.0};
        case Face::MinusY: return Vec3{ 0.0,-1.0, 0.0};
        case Face::PlusZ:  return Vec3{ 0.0, 0.0, 1.0};
        case Face::MinusZ: return Vec3{ 0.0, 0.0,-1.0};
    }
    return Vec3{0.0, 0.0, 0.0};
}

/// One (surface, material) row of GSC's own Geometry tables (§6.1, §6.2):
/// GSC's own alpha/rho/delta notation, quoted directly (§6's own intro):
/// "alpha = absorption coefficient, rho = specular reflection coefficient,
/// delta = diffuse reflection coefficient" -- matching this schema's own
/// absorptivity/specular/diffuse order EXACTLY, no swap (unlike `RS14`,
/// SPEC-spacecraft.md §3): GSC states the mapping this schema already uses,
/// not the reverse. `SPCR-A-010` checks this quote is not merely trusted:
/// every material row's own three coefficients sum to 1, cell by cell.
struct GalileoMaterial {
    double area_m2;
    double alpha;
    double rho;    ///< GSC's own rho -- specular, stated directly
    double delta;  ///< GSC's own delta -- diffuse, stated directly
};

/// One bus face's own citation-bearing FlatSurface, body-fixed (not
/// sun-tracking: the box does not rotate to track the Sun, only the wings
/// do, §3's own "rotating its solar panels around the Y axis"). `gsc_face`
/// names GSC's own mechanical-RF label; the normal is rotated to this
/// tree's own frame before `body_direction()` sees it.
[[nodiscard]] odl::Result<FlatSurface, MacromodelError>
galileo_box_face(Face gsc_face, const GalileoMaterial& m, const std::string& citation) {
    auto area = cited(m.area_m2, citation);
    auto absorptivity = cited(m.alpha, citation);
    auto specular = cited(m.rho, citation);
    auto diffuse = cited(m.delta, citation);
    if (!area.has_value()) return odl::err(area.error());
    if (!absorptivity.has_value()) return odl::err(absorptivity.error());
    if (!specular.has_value()) return odl::err(specular.error());
    if (!diffuse.has_value()) return odl::err(diffuse.error());
    auto normal = body_direction(galileo_frame_from_mechanical(mechanical_face_normal(gsc_face)));
    if (!normal.has_value()) return odl::err(normal.error());
    return flat_surface_body_fixed(*area, *normal, *absorptivity, *specular, *diffuse);
}

/// The sun-pointing solar-array surface, one per material: `m.area_m2` is
/// the SUM of both wings' own area for that material (GSC's own +Y/-Y, or
/// +SA/-SA, rows are identical in area and optics for every block this file
/// builds -- checked, not assumed, `SPCR-A-011`) -- both wings are always
/// equally sunlit by the SAME tracking mechanism the box does not have
/// (§3), the same "one combined array" treatment `modules/spacecraft`
/// already gives GPS's own two-sided panels.
[[nodiscard]] odl::Result<FlatSurface, MacromodelError>
galileo_wing(const GalileoMaterial& m, const std::string& citation) {
    auto area = cited(m.area_m2, citation);
    auto absorptivity = cited(m.alpha, citation);
    auto specular = cited(m.rho, citation);
    auto diffuse = cited(m.delta, citation);
    if (!area.has_value()) return odl::err(area.error());
    if (!absorptivity.has_value()) return odl::err(absorptivity.error());
    if (!specular.has_value()) return odl::err(specular.error());
    if (!diffuse.has_value()) return odl::err(diffuse.error());
    return flat_surface_sun_pointing(*area, *absorptivity, *specular, *diffuse);
}

/// One box face's own one or two materials (GSC's own table: a face with a
/// second material's own area does not, in general, equal the first --
/// e.g. IOV's own +X carries 0.54 m^2 of Kapton MLI and, separately, 0.78
/// m^2 of an optical surface radiator on the SAME physical face).
struct FaceRow {
    Face face;
    std::vector<GalileoMaterial> materials;
};

[[nodiscard]] odl::Result<void, MacromodelError>
add_box_faces(MacromodelBuilder& b, const std::vector<FaceRow>& rows, const std::string& citation) {
    for (const auto& row : rows) {
        for (const auto& m : row.materials) {
            auto s = galileo_box_face(row.face, m, citation);
            if (!s.has_value()) return odl::err(s.error());
            b.add_surface(*s);
        }
    }
    return {};
}

// --- IOV geometry (GSC §6.1) -----------------------------------------------

/// IOV's own Material 1, every box face (Carbon filled Kapton, external
/// MLI) -- "BOL & EOL" printed as ONE set: GSC's own table states these
/// values are unchanging over life, not merely unmeasured at EOL.
constexpr std::string_view kIovMat1Citation =
    "GSC Galileo Satellite Metadata Sec.6.1, IOV Table (Material 1)";
/// IOV's own Material 2 (present on +X/+Y/-Y/+Z only): GSC prints SEPARATE
/// BOL and EOL coefficients here. This version builds BOL (beginning of
/// life) only -- SPCR-Q-004 names EOL as a named, deliberate gap, not built.
constexpr std::string_view kIovMat2Citation =
    "GSC Galileo Satellite Metadata Sec.6.1, IOV Table (Material 2, BOL)";
constexpr std::string_view kIovWingCitation =
    "GSC Galileo Satellite Metadata Sec.6.1, IOV Table (Wing), +Y and -Y summed";

[[nodiscard]] odl::Result<Macromodel, SpacecraftError> galileo_iov_macromodel(
    double mass_kg, const Vec3& com_mechanical_mm, const std::string& mass_citation) {
    MacromodelBuilder b;

    const std::vector<FaceRow> rows = {
        {Face::MinusX, {{1.32, 0.94, 0.00, 0.06}}},
        {Face::PlusX,  {{0.54, 0.94, 0.00, 0.06}, {0.78, 0.10, 0.72, 0.18}}},
        {Face::PlusY,  {{1.00, 0.94, 0.00, 0.06}, {2.00, 0.10, 0.72, 0.18}}},
        {Face::MinusY, {{1.03, 0.94, 0.00, 0.06}, {1.97, 0.10, 0.72, 0.18}}},
        {Face::PlusZ,  {{1.72, 0.94, 0.00, 0.06}, {1.28, 0.57, 0.22, 0.21}}},
        {Face::MinusZ, {{3.00, 0.94, 0.00, 0.06}}},
    };
    // Material 1 and Material 2 carry different citations (different BOL/EOL
    // provenance), so each material's own row is built with its own -- not
    // a per-face loop over one shared string.
    for (const auto& row : rows) {
        auto s1 = galileo_box_face(row.face, row.materials[0], std::string(kIovMat1Citation));
        if (!s1.has_value()) return odl::err(SpacecraftError{s1.error().id, s1.error().message});
        b.add_surface(*s1);
        if (row.materials.size() > 1) {
            auto s2 = galileo_box_face(row.face, row.materials[1], std::string(kIovMat2Citation));
            if (!s2.has_value()) return odl::err(SpacecraftError{s2.error().id, s2.error().message});
            b.add_surface(*s2);
        }
    }

    // Wings: +Y (3.88 Solar Cells, 1.53 Kapton HN) and -Y (identical) summed.
    auto w1 = galileo_wing(GalileoMaterial{7.76, 0.92, 0.08, 0.00}, std::string(kIovWingCitation));
    if (!w1.has_value()) return odl::err(SpacecraftError{w1.error().id, w1.error().message});
    b.add_surface(*w1);
    auto w2 = galileo_wing(GalileoMaterial{3.06, 0.90, 0.10, 0.00}, std::string(kIovWingCitation));
    if (!w2.has_value()) return odl::err(SpacecraftError{w2.error().id, w2.error().message});
    b.add_surface(*w2);

    auto mass = cited(mass_kg, mass_citation);
    if (!mass.has_value()) return odl::err(SpacecraftError{mass.error().id, mass.error().message});
    // Centre of mass: GSC's own mechanical-RF coordinates (mm), rotated to
    // this tree's own frame by the SAME verified mapping the face normals
    // use, converted to metres -- this tree's own body-frame ORIGIN is
    // GSC's own mechanical RF origin, unchanged by the rotation (a pure
    // relabeling of axes, no translation): a stated choice, not something
    // GSC's own "ANTEX RF" columns give directly (those are ADDITIONALLY
    // recentred to CoM, a different, ANTEX-format-specific convention this
    // macromodel does not need or use).
    const Vec3 com_tree_m = 0.001 * galileo_frame_from_mechanical(com_mechanical_mm);  // UNIT-CROSSING: mm -> m (GSC prints CoM in mm)
    auto com = cited(com_tree_m, mass_citation);
    if (!com.has_value()) return odl::err(SpacecraftError{com.error().id, com.error().message});
    b.set_mass(*mass).set_centre_of_mass(*com);

    auto m = std::move(b).build();
    if (!m.has_value()) return odl::err(SpacecraftError{m.error().id, m.error().message});
    return *m;
}

// --- FOC geometry (GSC §6.2) ------------------------------------------------

constexpr std::string_view kFocCitation = "GSC Galileo Satellite Metadata Sec.6.2, FOC Table";
constexpr std::string_view kFocWingCitation =
    "GSC Galileo Satellite Metadata Sec.6.2, FOC Table (Wing), +SA and -SA summed";

[[nodiscard]] odl::Result<Macromodel, SpacecraftError> galileo_foc_macromodel(
    double mass_kg, const Vec3& com_mechanical_mm, const std::string& mass_citation) {
    MacromodelBuilder b;

    const std::vector<FaceRow> rows = {
        {Face::PlusX,  {{0.440, 0.93, 0.00, 0.07}, {0.880, 0.08, 0.73, 0.19}}},
        {Face::MinusX, {{1.320, 0.93, 0.00, 0.07}}},
        {Face::PlusY,  {{1.129, 0.93, 0.00, 0.07}, {1.654, 0.08, 0.73, 0.19}}},
        {Face::MinusY, {{1.244, 0.93, 0.00, 0.07}, {1.539, 0.08, 0.73, 0.19}}},
        // FOC's own +Z totals 3.022 m^2 (1.053 + 1.969) against the page's
        // own summary-table figure of 3.036 m^2 for "+-Z-panel" -- a small
        // (0.46%), genuine inconsistency between GSC's own summary and
        // detailed tables (-Z's own total, 3.036, DOES match the summary
        // exactly). Built from the detailed, itemised table (this schema's
        // own established preference for the finer-grained source, RS14's
        // own precedent), the gap recorded rather than silently resolved.
        {Face::PlusZ,  {{1.053, 0.93, 0.00, 0.07}, {1.969, 0.57, 0.22, 0.21}}},
        {Face::MinusZ, {{2.077, 0.93, 0.00, 0.07}, {0.959, 0.08, 0.73, 0.19}}},
    };
    auto r = add_box_faces(b, rows, std::string(kFocCitation));
    if (!r.has_value()) return odl::err(SpacecraftError{r.error().id, r.error().message});

    // Wings: +SA (3.880 Material E, 1.530 Material D) and -SA (identical) summed.
    auto w1 = galileo_wing(GalileoMaterial{7.760, 0.92, 0.08, 0.00}, std::string(kFocWingCitation));
    if (!w1.has_value()) return odl::err(SpacecraftError{w1.error().id, w1.error().message});
    b.add_surface(*w1);
    auto w2 = galileo_wing(GalileoMaterial{3.060, 0.90, 0.10, 0.00}, std::string(kFocWingCitation));
    if (!w2.has_value()) return odl::err(SpacecraftError{w2.error().id, w2.error().message});
    b.add_surface(*w2);

    auto mass = cited(mass_kg, mass_citation);
    if (!mass.has_value()) return odl::err(SpacecraftError{mass.error().id, mass.error().message});
    const Vec3 com_tree_m = 0.001 * galileo_frame_from_mechanical(com_mechanical_mm);  // UNIT-CROSSING: mm -> m (GSC prints CoM in mm)
    auto com = cited(com_tree_m, mass_citation);
    if (!com.has_value()) return odl::err(SpacecraftError{com.error().id, com.error().message});
    b.set_mass(*mass).set_centre_of_mass(*com);

    auto m = std::move(b).build();
    if (!m.has_value()) return odl::err(SpacecraftError{m.error().id, m.error().message});
    return *m;
}

// --- Mass and centre of mass, per satellite, dated (GSC §4) ----------------

struct MassComRow {
    int gsat;
    double mass_kg;
    Vec3 com_mechanical_mm;
};

/// GSC's own IOV table (§4.1), "as of April 2024" -- this session's own
/// pin, GSC's own most recent update at the time of the 2026-09-24 fetch,
/// valid from that stated month with no stated end.
constexpr YearMonth kIovValidFrom{2024, 4};
const std::vector<MassComRow> kIovMassCom = {
    {101, 696.802, {1205.84, 628.97, 553.44}},
    {102, 695.318, {1205.33, 628.81, 551.41}},
    {103, 697.632, {1205.29, 629.58, 552.81}},
};

/// GSC's own FOC table (§4.2), "as of May 2026". GSAT numbers not listed
/// here (205, 228-231) are simply absent from GSC's own current table --
/// not guessed at or filled in.
constexpr YearMonth kFocValidFrom{2026, 5};
const std::vector<MassComRow> kFocMassCom = {
    {201, 660.977, {316.89, -13.48, 561.92}},  {202, 662.141, {311.61, -12.60, 562.31}},
    {203, 705.685, {259.54, -9.24, 561.17}},   {204, 697.701, {269.65, -9.35, 561.29}},
    {206, 707.734, {259.24, -9.51, 565.27}},   {207, 706.643, {261.18, -9.52, 565.29}},
    {208, 709.135, {261.08, -10.43, 565.28}},  {209, 707.879, {261.50, -9.60, 565.00}},
    {210, 705.373, {263.51, -9.63, 565.31}},   {211, 707.634, {263.08, -10.24, 565.27}},
    {212, 709.858, {257.28, -9.88, 565.27}},   {213, 708.759, {262.67, -10.33, 565.27}},
    {214, 709.755, {261.61, -9.41, 565.27}},   {215, 708.500, {266.80, -11.31, 565.35}},
    {216, 710.475, {260.16, -10.24, 565.26}},  {217, 710.244, {262.30, -10.98, 565.28}},
    {218, 711.514, {261.15, -10.70, 565.28}},  {219, 709.694, {262.97, -9.88, 565.28}},
    {220, 710.795, {262.58, -9.79, 565.28}},   {221, 710.703, {261.70, -9.33, 565.27}},
    {222, 709.500, {262.20, -9.88, 565.28}},   {223, 711.410, {262.85, -10.26, 565.29}},
    {224, 710.670, {263.41, -10.06, 565.27}},  {225, 710.649, {260.52, -9.59, 565.26}},
    {226, 711.103, {256.80, -9.21, 565.22}},   {227, 708.301, {258.31, -10.14, 565.25}},
    {232, 704.246, {263.26, -10.38, 565.34}},  {233, 708.216, {257.70, -10.77, 565.25}},
    {234, 705.601, {260.98, -10.72, 565.31}},
};

/// SPCR-F-004/SPCR-F-005: not built from a citation chain at all (there is
/// nothing to cite for "this GSAT does not exist" or "this epoch predates
/// the source's own table") -- a plain refusal, the same shape
/// `odl::atmosphere::SpaceWeatherTable::sample` uses for a day outside its
/// own coverage.
[[nodiscard]] odl::Result<MassComRow, SpacecraftError>
find_mass_com(const std::vector<MassComRow>& table, YearMonth valid_from, int gsat,
             YearMonth epoch, const char* block_name) {
    const MassComRow* row = nullptr;
    for (const auto& r : table)
        if (r.gsat == gsat) { row = &r; break; }
    if (row == nullptr) {
        return odl::err(SpacecraftError{"SPCR-F-004",
            std::string("Galileo ") + block_name + " GSAT" + std::to_string(gsat) +
            " is not one of GSC's own currently-listed satellites (SPEC-spacecraft.md §2 GALSC)"});
    }
    if (epoch < valid_from) {
        return odl::err(SpacecraftError{"SPCR-F-005",
            std::string("epoch ") + std::to_string(epoch.year) + "-" + std::to_string(epoch.month) +
            " is before GSAT" + std::to_string(gsat) + "'s own mass/CoM entry, valid from " +
            std::to_string(valid_from.year) + "-" + std::to_string(valid_from.month) +
            " (GSC's own \"as of\" pin) -- outside this table's own coverage, the way "
            "odl::atmosphere refuses an epoch outside its own space-weather coverage"});
    }
    return *row;
}

}  // namespace

Vec3 galileo_frame_from_mechanical(Vec3 mechanical) noexcept {
    return Vec3{-mechanical.x, -mechanical.y, mechanical.z};
}

odl::Result<Macromodel, SpacecraftError> galileo_iov(int gsat, YearMonth epoch) {
    auto row = find_mass_com(kIovMassCom, kIovValidFrom, gsat, epoch, "IOV");
    if (!row.has_value()) return odl::err(row.error());
    const std::string mass_citation =
        "GSC Galileo Satellite Metadata Sec.4.1, IOV Table, GSAT" + std::to_string(gsat) +
        ", as of April 2024";
    return galileo_iov_macromodel(row->mass_kg, row->com_mechanical_mm, mass_citation);
}

odl::Result<Macromodel, SpacecraftError> galileo_foc(int gsat, YearMonth epoch) {
    auto row = find_mass_com(kFocMassCom, kFocValidFrom, gsat, epoch, "FOC");
    if (!row.has_value()) return odl::err(row.error());
    const std::string mass_citation =
        "GSC Galileo Satellite Metadata Sec.4.2, FOC Table, GSAT" + std::to_string(gsat) +
        ", as of May 2026";
    return galileo_foc_macromodel(row->mass_kg, row->com_mechanical_mm, mass_citation);
}

}  // namespace odl::spacecraft
