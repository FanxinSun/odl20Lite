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
/// One RS14 table row: area [m^2], RS14's own alpha, delta, rho -- printed
/// in that column order (SPEC-spacecraft.md §3). Mapped at the ONE place
/// this file maps them, by the FORMULA RHS12 (reprinted in RS14 as P-II)
/// itself writes, Eq.6: f = -(A*S0/Mc)[cos(theta)*(1-rho)*e_D +
/// 2*(delta/3 + rho*cos(theta))*e_N] -- rho carries the "2*rho*cos(theta)"
/// mirror-like term (specular), delta the "2*delta/3" Lambertian term
/// (diffuse). RS14's own prose definition list in its Appendix Sec.5.1.2
/// ("delta: reflection... rho: diffusion") is the OPPOSITE of this, and of
/// RHS12's own reprinted prose definition a few pages earlier in the same
/// document (Sec.4.1, "P-II") and of Sec.4.2's own GLONASS cylinder-wing
/// definitions -- an internal inconsistency in RS14 itself, not a
/// convention this file follows. The formula, not the words, is what
/// resolves it (manager's own instruction, 2026-09-24), cross-checked
/// physically too: glass-covered solar panels are predominantly specular,
/// and every block's own panel row has the "rho" column an order of
/// magnitude larger than "delta" -- consistent only with rho = specular.
struct RS14Row {
    double area_m2;
    double alpha;
    double rs14_delta;  ///< RHS12 Eq.6's own "2*delta/3" term -> schema's diffuse
    double rs14_rho;    ///< RHS12 Eq.6's own "2*rho*cos(theta)" term -> schema's specular
};

/// Builds one body-fixed FlatSurface from one RS14Row, citing
/// `area_citation` for the area and `optics_citation` for alpha/specular/
/// diffuse -- separate strings, since a block's own dimensions and optical
/// properties can trace to different sources (GPS-IIF's own do, §4 below).
[[nodiscard]] odl::Result<FlatSurface, MacromodelError>
bus_face(Face face, const RS14Row& row, const std::string& area_citation,
        const std::string& optics_citation) {
    auto area = cited(row.area_m2, area_citation);
    auto absorptivity = cited(row.alpha, optics_citation);
    auto specular = cited(row.rs14_rho, optics_citation + " (RS14's own rho, RHS12 Eq.6's own "
                                                            "'2*rho*cos theta' term -- specular, "
                                                            "per SPEC-spacecraft.md Sec.3, "
                                                            "formula-verified)");
    auto diffuse = cited(row.rs14_delta, optics_citation + " (RS14's own delta, RHS12 Eq.6's own "
                                                             "'2*delta/3' term -- diffuse, per "
                                                             "SPEC-spacecraft.md Sec.3, "
                                                             "formula-verified)");
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
solar_panels(const RS14Row& row, const std::string& area_citation,
            const std::string& optics_citation) {
    auto area = cited(row.area_m2, area_citation);
    auto absorptivity = cited(row.alpha, optics_citation);
    auto specular = cited(row.rs14_rho, optics_citation + " (RS14's own rho, RHS12 Eq.6's own "
                                                            "'2*rho*cos theta' term -- specular, "
                                                            "per SPEC-spacecraft.md Sec.3, "
                                                            "formula-verified)");
    auto diffuse = cited(row.rs14_delta, optics_citation + " (RS14's own delta, RHS12 Eq.6's own "
                                                             "'2*delta/3' term -- diffuse, per "
                                                             "SPEC-spacecraft.md Sec.3, "
                                                             "formula-verified)");
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
assemble(const RS14Row faces[6], const RS14Row& panels, const std::string& area_citation,
        const std::string& optics_citation, double mass_kg, const std::string& mass_citation) {
    MacromodelBuilder b;
    constexpr Face order[6] = {Face::PlusX, Face::MinusX, Face::PlusY,
                                Face::MinusY, Face::PlusZ, Face::MinusZ};
    for (int i = 0; i < 6; ++i) {
        auto s = bus_face(order[i], faces[i], area_citation, optics_citation);
        if (!s.has_value()) return odl::err(SpacecraftError{s.error().id, s.error().message});
        b.add_surface(*s);
    }
    auto p = solar_panels(panels, area_citation, optics_citation);
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
    return assemble(faces, panels, "RS14 Table 5.2, averaged from FLGA92",
                    "RS14 Table 5.2, averaged from FLGA92", mass_kg,
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
                    "RS14 Table 5.3, averaged from FLGA92",
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
    return assemble(faces, panels, "RS14 Table 5.4, averaged from FLGA96",
                    "RS14 Table 5.4, averaged from FLGA96", 1100.0,
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
    const std::string inherited =
        "= gps_block_iir(), RS14 Table 5.4, per MSGA15's own IIR/IIR-M grouping "
        "(Fig. 4) and its own phase-center-only distinction (Table 3) -- not "
        "independently measured for IIR-M";
    return assemble(faces, panels, inherited, inherited,
                    1100.0,
                    "= gps_block_iir()'s own mass; no IIR-M-specific mass is published in any "
                    "source this spec holds, assumed unchanged");
}

odl::Result<Macromodel, SpacecraftError> gps_block_iif() {
    // SPCR-R-004, per the manager's own ruling 2026-09-24: RS14 Table 5.5
    // publishes per-surface dimensions AND optics for IIF, so this builds
    // from it like every other block -- but the two halves of that table
    // trace to genuinely different provenance, cited separately:
    //  - dimensions: RS14's own Table 5.5 states its OWN source is "an
    //    unpublished document" it does not reprint -- a secondary source
    //    whose own chain ends at a document this tree does not hold.
    //  - optics: RS14 Table 5.5's own footnote states these are its
    //    GENERIC assumption (= Ziebart (2001) Sec.7.1's own default),
    //    not an IIF-specific measurement -- marked ASSUMED, not measured.
    // RS14 Table 5.5, order +Z,-Z,+Y,-Y,+X,-X to match assemble()'s own
    // {+X,-X,+Y,-Y,+Z,-Z} consumption order below.
    const RS14Row faces[6] = {
        /*+X*/ {5.720, 0.440, 0.448, 0.112},
        /*-X*/ {5.720, 0.440, 0.448, 0.112},
        /*+Y*/ {7.010, 0.440, 0.448, 0.112},
        /*-Y*/ {7.010, 0.440, 0.448, 0.112},
        /*+Z*/ {5.400, 0.440, 0.448, 0.112},
        /*-Z*/ {5.400, 1.000, 0.000, 0.000},
    };
    const RS14Row panels{22.250, 0.770, 0.035, 0.195};
    return assemble(faces, panels,
                    "RS14 Table 5.5, which itself states its own dimensions come from "
                    "\"an unpublished document\" -- this tree does not hold that document; "
                    "recorded here as the end of the citation chain, not resolved further "
                    "(full search: ~/.claude/handover/2026-09-24-odl-rewrite-L5.REPORT.md)",
                    "RS14 Table 5.5's own generic assumption, same as Ziebart (2001) Sec.7.1 -- "
                    "ASSUMED for IIF, not an IIF-specific measurement (RS14 states this itself)",
                    1555.0,
                    "RS14 Table 5.5's own caption (1555 kg). Aggregate cross-check: IGS Satellite "
                    "Metadata SINEX (files.igs.org/pub/station/general/igs_satellite_metadata.snx, "
                    "fetched 2026-09-24), SATELLITE/MASS for SVN63/G063/NAVSTAR-66, gives 1633 kg "
                    "-- about 5% higher, consistent with launch mass (SINEX's own field) exceeding "
                    "on-orbit dry mass (RS14's own figure) after expendables; not a contradiction. "
                    "A second aggregate check (panel span, against a published figure) was sought "
                    "and NOT completed: no independently verified, directly-fetched citable source "
                    "was found for a GPS-IIF panel span figure, so none is recorded or relied on "
                    "here (see the report file for the full account, including a wrong-satellite "
                    "search dead-end -- USA-66 vs NAVSTAR-66 -- discarded before use).");
}

odl::Result<Macromodel, SpacecraftError> gps_block_iiia() {
    return odl::err(SpacecraftError{"SPCR-F-003",
        "GPS Block IIIA has no citable published per-surface dimension or optical source. One "
        "search performed 2026-09-24 (SPEC-spacecraft.md Sec.2, full record in "
        "~/.claude/handover/2026-09-24-odl-rewrite-L5.REPORT.md): its one plausible lead, a "
        "ScienceDirect paper on a GPS III box-wing model ('GPS III Vespucci: Results of half a "
        "year in orbit'), returned HTTP 403 to an automated fetch -- the same publisher-blocking "
        "pattern this tree has hit repeatedly (AGU, AIAA/DTIC, IEEE Xplore, Wiley, TUM mediaTUM, "
        "web.archive.org); no per-surface IIIA table was independently confirmed. Refuses rather "
        "than building from an uncitable or unverified source. No baseline currently consumes "
        "gps_block_iiia(); this refusal blocks nothing critical in this version."});
}

}  // namespace odl::spacecraft
