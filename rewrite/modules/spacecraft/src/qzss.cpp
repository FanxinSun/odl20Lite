// qzss.cpp — SPEC-spacecraft, L5 step 3: QZSS.
//
// Source throughout: Cabinet Office, Government of Japan, National Space
// Policy Secretariat, "QZS-1 Satellite Information" ("SPI_QZS1_B", rev. B,
// 2022-03-24), fetched directly 2026-09-24 from
// qzss.go.jp/en/technical/qzssinfo/khp0mf0000000wuf-att/spi-qzs1_b.pdf.
// Redistribution terms (qzss.go.jp/en/technical/qzssinfo/index.html, quoted
// in full in the rule-4 search): "freely available to any user... shall
// indicate proper credit."

#include <odl/spacecraft/qzss.hpp>

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

/// SPI_QZS1_B's own native face labels (§6 Table 4's own "Location"
/// column), NOT this tree's own convention -- every use below is piped
/// through `qzss_frame_from_native()` before reaching `body_direction()`.
enum class Face { PlusX, MinusX, PlusY, MinusY, PlusZ, MinusZ };

[[nodiscard]] Vec3 native_face_normal(Face f) noexcept {
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

/// One (surface, material) row of Table 4 -- SPI_QZS1_B's own columns are
/// LITERALLY "Absorption"/"Specular"/"Diffuse" (English words, Sec.6), no
/// Greek-letter notation to resolve or cross-check by formula the way
/// RS14's alpha/delta/rho needed: read directly, zero ambiguity.
struct QzssMaterial {
    double area_m2;
    double absorption;
    double specular;
    double diffuse;
};

[[nodiscard]] odl::Result<FlatSurface, MacromodelError>
qzss_box_face(Face face, const QzssMaterial& m, const std::string& citation) {
    auto area = cited(m.area_m2, citation);
    auto absorptivity = cited(m.absorption, citation);
    auto specular = cited(m.specular, citation);
    auto diffuse = cited(m.diffuse, citation);
    if (!area.has_value()) return odl::err(area.error());
    if (!absorptivity.has_value()) return odl::err(absorptivity.error());
    if (!specular.has_value()) return odl::err(specular.error());
    if (!diffuse.has_value()) return odl::err(diffuse.error());
    auto normal = body_direction(qzss_frame_from_native(native_face_normal(face)));
    if (!normal.has_value()) return odl::err(normal.error());
    return flat_surface_body_fixed(*area, *normal, *absorptivity, *specular, *diffuse);
}

/// The SAP (solar array panel) material -- sun-pointing, this header's own
/// top comment explains why (Table 4's own footnote *2, and §2's own
/// "+Y parallel to the rotation axis of the solar panels").
[[nodiscard]] odl::Result<FlatSurface, MacromodelError>
qzss_sap(const QzssMaterial& m, const std::string& citation) {
    auto area = cited(m.area_m2, citation);
    auto absorptivity = cited(m.absorption, citation);
    auto specular = cited(m.specular, citation);
    auto diffuse = cited(m.diffuse, citation);
    if (!area.has_value()) return odl::err(area.error());
    if (!absorptivity.has_value()) return odl::err(absorptivity.error());
    if (!specular.has_value()) return odl::err(specular.error());
    if (!diffuse.has_value()) return odl::err(diffuse.error());
    return flat_surface_sun_pointing(*area, *absorptivity, *specular, *diffuse);
}

constexpr std::string_view kOpticsCitation =
    "Cabinet Office SPI_QZS1_B (rev. B, 2022-03-24) Sec.6 Table 4, columns literally "
    "\"Absorption\"/\"Specular\"/\"Diffuse\" -- read directly, no notation to resolve";

constexpr std::string_view kSapCitation =
    "Cabinet Office SPI_QZS1_B Sec.6 Table 4's own SAP row, +Y and -Y areas summed (both wings, "
    "the same 'one combined sun-pointing array' treatment GPS's and Galileo's own panels get) -- "
    "Table 4's own footnote *2 states a row's own \"Location\" does not necessarily indicate the "
    "direction normal to it, and Rev.B's own revision history states these are \"Optical properties "
    "when the SAPs do not generate the electric power\" (superseding Rev.A's own generating-state "
    "figures, not reprinted here, this tree not holding Rev.A)";

}  // namespace

Vec3 qzss_frame_from_native(Vec3 native) noexcept {
    return Vec3{-native.x, -native.y, native.z};
}

odl::Result<Macromodel, SpacecraftError> qzss_1(QzssLife life) {
    MacromodelBuilder b;

    const std::string optics_citation(kOpticsCitation);
    // Table 4, every row this schema CAN hold. Multi-material faces built
    // as separate co-normal FlatSurfaces (galileo_iov's own precedent),
    // SAP built separately below (sun-pointing, not body-fixed).
    struct Row { Face face; QzssMaterial m; };
    const Row rows[] = {
        {Face::PlusX,  {9.0, 0.95, 0.0025, 0.0475}},                 // MLI
        {Face::MinusX, {9.0, 0.95, 0.0025, 0.0475}},                 // MLI
        {Face::PlusY,  {3.0, 0.95, 0.0025, 0.0475}},                 // MLI
        {Face::PlusY,  {7.0, 0.08, 0.8280, 0.0920}},                 // Radiator
        {Face::MinusY, {5.0, 0.95, 0.0025, 0.0475}},                 // MLI
        {Face::MinusY, {5.0, 0.08, 0.8280, 0.0920}},                 // Radiator
        {Face::PlusZ,  {3.0, 0.95, 0.0025, 0.0475}},                 // MLI
        // +Z's own "L-ANT Cover" material is OMITTED -- Table 4 gives it
        // no area (footnoted "(*1)" instead of a number) and states its
        // own shape is a truncated cone, which this schema cannot hold
        // regardless of area -- a genuine gap, not built around
        // (SPEC-spacecraft.md §3, the QZSS entry).
        {Face::MinusZ, {5.6, 0.95, 0.0025, 0.0475}},                 // MLI
    };
    for (const auto& row : rows) {
        auto s = qzss_box_face(row.face, row.m, optics_citation);
        if (!s.has_value()) return odl::err(SpacecraftError{s.error().id, s.error().message});
        b.add_surface(*s);
    }

    // SAP: +Y (22.5 m^2) and -Y (22.5 m^2) summed, one combined sun-pointing surface.
    auto sap = qzss_sap(QzssMaterial{22.5 + 22.5, 0.76, 0.216, 0.024}, std::string(kSapCitation));
    if (!sap.has_value()) return odl::err(SpacecraftError{sap.error().id, sap.error().message});
    b.add_surface(*sap);

    const double mass_kg = (life == QzssLife::BeginningOfLife) ? 2281.0 : 2121.0;
    const Vec3 com_native_mm = (life == QzssLife::BeginningOfLife)
        ? Vec3{-1.1, 1.6, 1818.4}
        : Vec3{-1.2, 1.7, 1849.6};
    const std::string mass_citation =
        std::string("Cabinet Office SPI_QZS1_B Sec.4 Table 1, ") +
        (life == QzssLife::BeginningOfLife
             ? "BOL (\"Completion of QZS-orbit insertion\")"
             : "EOL (\"12 years after launch\")") +
        " -- \"Prediction as a design\" (Table 1's own caption), not a measured in-orbit value";

    auto mass = cited(mass_kg, mass_citation);
    if (!mass.has_value()) return odl::err(SpacecraftError{mass.error().id, mass.error().message});
    const Vec3 com_tree_m = 0.001 * qzss_frame_from_native(com_native_mm);  // UNIT-CROSSING: mm -> m (SPI_QZS1_B prints CoM in mm)
    auto com = cited(com_tree_m, mass_citation);
    if (!com.has_value()) return odl::err(SpacecraftError{com.error().id, com.error().message});
    b.set_mass(*mass).set_centre_of_mass(*com);

    auto m = std::move(b).build();
    if (!m.has_value()) return odl::err(SpacecraftError{m.error().id, m.error().message});
    return *m;
}

}  // namespace odl::spacecraft
