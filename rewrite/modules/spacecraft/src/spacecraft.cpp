// spacecraft.cpp — SPEC-spacecraft, L5 step 1: GPS.

#include <odl/spacecraft/spacecraft.hpp>

#include <odl/macromodel/body_direction.hpp>
#include <odl/macromodel/cited.hpp>

namespace odl::spacecraft {
namespace {

using macromodel::BodyDirection;
using macromodel::Cited;
using macromodel::cited;
using macromodel::FlatSurface;
using macromodel::flat_surface_body_fixed;
using macromodel::flat_surface_sun_pointing;
using macromodel::Macromodel;
using macromodel::MacromodelBuilder;
using macromodel::MacromodelError;
using macromodel::body_direction;

/// One bus face's own six-sided position (+X, -X, +Y, -Y, +Z, -Z), IGS
/// frame -- SPEC-spacecraft.md §3, +x towards the Sun. Shared by every
/// block this file builds; the geometry these unit vectors state is common
/// to the schema itself, not any one source's own citation.
enum class Face { PlusX, MinusX, PlusY, MinusY, PlusZ, MinusZ };

[[nodiscard]] Vec3 face_normal(Face f) noexcept {
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
/// One RS14 table row: area [m^2], RS14's own alpha, delta ("reflection"),
/// rho ("diffusion"). Mapped at the ONE place this file maps them
/// (SPEC-spacecraft.md §3): RS14's delta -> this schema's specular, RS14's
/// rho -> this schema's diffuse.
struct RS14Row {
    double area_m2;
    double alpha;
    double rs14_delta;  ///< RS14's own "reflection" -> schema's specular
    double rs14_rho;    ///< RS14's own "diffusion" -> schema's diffuse
};

/// Builds one body-fixed FlatSurface from one RS14Row, citing `table` (e.g.
/// "RS14 Table 5.3, averaged from FLGA92") for every one of its four
/// values, at `face`.
[[nodiscard]] odl::Result<FlatSurface, MacromodelError>
bus_face(Face face, const RS14Row& row, const std::string& table) {
    auto area = cited(row.area_m2, table);
    auto absorptivity = cited(row.alpha, table);
    auto specular = cited(row.rs14_delta, table + " (RS14's own delta/'reflection', mapped to "
                                                    "specular per SPEC-spacecraft.md Sec.3)");
    auto diffuse = cited(row.rs14_rho, table + " (RS14's own rho/'diffusion', mapped to diffuse "
                                                "per SPEC-spacecraft.md Sec.3)");
    if (!area.has_value()) return odl::err(area.error());
    if (!absorptivity.has_value()) return odl::err(absorptivity.error());
    if (!specular.has_value()) return odl::err(specular.error());
    if (!diffuse.has_value()) return odl::err(diffuse.error());
    auto normal = body_direction(face_normal(face));
    if (!normal.has_value()) return odl::err(normal.error());
    return flat_surface_body_fixed(*area, *normal, *absorptivity, *specular, *diffuse);
}

/// The sun-pointing solar-panel surface, same row shape and mapping.
[[nodiscard]] odl::Result<FlatSurface, MacromodelError>
solar_panels(const RS14Row& row, const std::string& table) {
    auto area = cited(row.area_m2, table);
    auto absorptivity = cited(row.alpha, table);
    auto specular = cited(row.rs14_delta, table + " (RS14's own delta/'reflection', mapped to "
                                                    "specular per SPEC-spacecraft.md Sec.3)");
    auto diffuse = cited(row.rs14_rho, table + " (RS14's own rho/'diffusion', mapped to diffuse "
                                                "per SPEC-spacecraft.md Sec.3)");
    if (!area.has_value()) return odl::err(area.error());
    if (!absorptivity.has_value()) return odl::err(absorptivity.error());
    if (!specular.has_value()) return odl::err(specular.error());
    if (!diffuse.has_value()) return odl::err(diffuse.error());
    return flat_surface_sun_pointing(*area, *absorptivity, *specular, *diffuse);
}

/// Assembles one block's own six bus faces plus panels into a Macromodel,
/// citing `mass_kg` and the (0,0,0) centre of mass separately (RS14 states
/// no offset; this tree's own default, stated as such, not RS14's own).
[[nodiscard]] odl::Result<Macromodel, SpacecraftError>
assemble(const RS14Row faces[6], const RS14Row& panels, const std::string& table,
        double mass_kg, const std::string& mass_citation) {
    MacromodelBuilder b;
    constexpr Face order[6] = {Face::PlusX, Face::MinusX, Face::PlusY,
                                Face::MinusY, Face::PlusZ, Face::MinusZ};
    for (int i = 0; i < 6; ++i) {
        auto s = bus_face(order[i], faces[i], table);
        if (!s.has_value()) return odl::err(SpacecraftError{s.error().id, s.error().message});
        b.add_surface(*s);
    }
    auto p = solar_panels(panels, table);
    if (!p.has_value()) return odl::err(SpacecraftError{p.error().id, p.error().message});
    b.add_surface(*p);

    auto mass = cited(mass_kg, mass_citation);
    if (!mass.has_value()) return odl::err(SpacecraftError{mass.error().id, mass.error().message});
    auto com = cited(Vec3{0.0, 0.0, 0.0},
                     "this tree's own default: RS14 states no centre-of-mass offset for this block");
    if (!com.has_value()) return odl::err(SpacecraftError{com.error().id, com.error().message});
    b.set_mass(*mass).set_centre_of_mass(*com);

    auto m = std::move(b).build();
    if (!m.has_value()) return odl::err(SpacecraftError{m.error().id, m.error().message});
    return *m;
}

}  // namespace

odl::Result<Macromodel, SpacecraftError> gps_block_i(int svn) {
    double mass_kg;
    switch (svn) {
        case 3: case 4: case 6: mass_kg = 450.0; break;
        case 8: case 9: case 10: case 11: mass_kg = 520.0; break;
        default:
            return odl::err(SpacecraftError{"SPCR-F-001",
                "GPS Block I SVN " + std::to_string(svn) + " is not one of RS14 Table 5.2's own "
                "eight (03, 04, 06 at 450 kg; 08, 09, 10, 11 at 520 kg)"});
    }
    // RS14 Table 5.2, order +Z,-Z,+Y,-Y,+X,-X to match assemble()'s own
    // {+X,-X,+Y,-Y,+Z,-Z} consumption order below.
    const RS14Row faces[6] = {
        /*+X*/ {2.047, 0.432, 0.134, 0.434},
        /*-X*/ {2.047, 0.432, 0.134, 0.434},
        /*+Y*/ {2.275, 0.393, 0.146, 0.461},
        /*-Y*/ {2.275, 0.393, 0.146, 0.461},
        /*+Z*/ {1.510, 0.140, 0.215, 0.645},
        /*-Z*/ {1.510, 0.535, 0.093, 0.372},
    };
    const RS14Row panels{6.053, 0.722, 0.042, 0.236};
    return assemble(faces, panels, "RS14 Table 5.2, averaged from FLGA92", mass_kg,
                    "RS14 Table 5.2's own caption");
}

odl::Result<Macromodel, SpacecraftError> gps_block_ii_iia(bool is_iia) {
    const RS14Row faces[6] = {
        /*+X*/ {2.719, 0.500, 0.400, 0.100},
        /*-X*/ {2.719, 0.500, 0.400, 0.100},
        /*+Y*/ {3.383, 0.511, 0.391, 0.098},
        /*-Y*/ {3.383, 0.511, 0.391, 0.098},
        /*+Z*/ {2.881, 0.440, 0.448, 0.112},
        /*-Z*/ {2.881, 0.582, 0.335, 0.083},
    };
    const RS14Row panels{11.851, 0.746, 0.057, 0.197};
    return assemble(faces, panels, "RS14 Table 5.3, averaged from FLGA92",
                    is_iia ? 975.0 : 880.0, "RS14 Table 5.3's own caption");
}

odl::Result<Macromodel, SpacecraftError> gps_block_iir() {
    const RS14Row faces[6] = {
        /*+X*/ {4.110, 0.940, 0.060, 0.000},
        /*-X*/ {4.110, 0.940, 0.060, 0.000},
        /*+Y*/ {4.460, 0.940, 0.060, 0.000},
        /*-Y*/ {4.460, 0.940, 0.060, 0.000},
        /*+Z*/ {4.250, 0.940, 0.060, 0.000},
        /*-Z*/ {4.250, 0.940, 0.060, 0.000},
    };
    const RS14Row panels{13.920, 0.707, 0.044, 0.249};
    return assemble(faces, panels, "RS14 Table 5.4, averaged from FLGA96", 1100.0,
                    "RS14 Table 5.4's own caption");
}

odl::Result<Macromodel, SpacecraftError> gps_block_iir_m() {
    // SPCR-R-005: IIR-M's own geometry, per MSGA15's own IIR/IIR-M grouping
    // and its own phase-center-only distinction -- an inference, not an
    // IIR-M-specific measurement, stated in every value's own citation.
    const RS14Row faces[6] = {
        /*+X*/ {4.110, 0.940, 0.060, 0.000},
        /*-X*/ {4.110, 0.940, 0.060, 0.000},
        /*+Y*/ {4.460, 0.940, 0.060, 0.000},
        /*-Y*/ {4.460, 0.940, 0.060, 0.000},
        /*+Z*/ {4.250, 0.940, 0.060, 0.000},
        /*-Z*/ {4.250, 0.940, 0.060, 0.000},
    };
    const RS14Row panels{13.920, 0.707, 0.044, 0.249};
    return assemble(faces, panels,
                    "= gps_block_iir(), RS14 Table 5.4, per MSGA15's own IIR/IIR-M grouping "
                    "(Fig. 4) and its own phase-center-only distinction (Table 3) -- not "
                    "independently measured for IIR-M",
                    1100.0,
                    "= gps_block_iir()'s own mass; no IIR-M-specific mass is published in any "
                    "source this spec holds, assumed unchanged");
}

odl::Result<Macromodel, SpacecraftError> gps_block_iif() {
    return odl::err(SpacecraftError{"SPCR-F-002",
        "GPS Block IIF has no citable published per-surface dimension or optical source "
        "(SPEC-spacecraft.md Sec.2, Sec.4 SPCR-R-004; full search recorded in "
        "~/.claude/handover/2026-09-24-odl-rewrite-L5.REPORT.md) -- RS14's own IIF table "
        "(Sec.5.5) states its dimensions come from \"an unpublished document,\" which this tree "
        "does not hold. Refuses rather than building from an uncitable source."});
}

}  // namespace odl::spacecraft
