// glonass.cpp — SPEC-spacecraft, L5 step 3: GLONASS, GLONASS-M, GLONASS-K.
//
// Source throughout: RS14 (Rodriguez-Solano 2014 dissertation) Tables 5.6,
// 5.7, 5.8 -- the SAME secondary-source ruling already covering GPS's own
// Block I/II/IIA/IIR/IIF (SPEC-spacecraft.md §2.2), extended to these three
// blocks 2026-09-24 (the manager's own ruling, `../plan/subplan_L5/L5-3.md`:
// "the RS14 ruling extends to its GLONASS, GLONASS-M and GLONASS-K tables:
// cited per value and marked as a secondary source"). The SAME
// formula-verified rho->specular, delta->diffuse mapping already established
// for GPS (SPEC-spacecraft.md §3, RHS12 Eq.6) applies identically -- RS14's
// own Sec.4.2 (this file's own cylinder-wing formula, below) independently
// restates the SAME rho=specular/delta=diffuse pairing, corroborating rather
// than reopening that mapping.

#include <odl/spacecraft/glonass.hpp>

#include <odl/macromodel/body_direction.hpp>
#include <odl/macromodel/cited.hpp>

#include <string>

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

/// SPEC-spacecraft.md §3's own IGS body frame, +x towards the Sun -- the
/// SAME six unit vectors `modules/spacecraft/src/spacecraft.cpp`'s own
/// `Face`/`face_normal` state, independently duplicated here rather than
/// shared (the established per-file pattern already seen between
/// spacecraft.cpp and galileo.cpp: each constellation's own file is
/// self-contained).
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

/// One RS14 table row -- the SAME shape `spacecraft.cpp`'s own (unexported,
/// anonymous-namespace) `RS14Row` is, duplicated here for the same reason
/// `Face` above is: area, RS14's own alpha/delta/rho, mapped by RHS12 Eq.6
/// (SPEC-spacecraft.md §3): rho -> this schema's specular, delta -> this
/// schema's diffuse.
struct RS14Row {
    double area_m2;
    double alpha;
    double rs14_delta;  ///< -> schema's diffuse
    double rs14_rho;    ///< -> schema's specular
};

/// RS14 Sec.4.2's own words, quoted: for GLONASS and GLONASS-M's own
/// +-X/+-Y bus faces, "the ratio of the sum of cylindrical areas w.r.t. sum
/// of flat areas is given in the 'shape' column, where 0 indicates flat and
/// 1 indicates cylindrical" -- a DISTINCT force formula, Eq.4.5, blends the
/// standard flat law (Eq.9 of RS14's own P-II, what this schema's
/// FlatSurface already implements) with a cylindrical-surface law (Eq.4.4,
/// Fliegel et al. 1992) the schema has no surface type for (no cylinder,
/// the same gap BeiDou's own curved surfaces hit, `SPEC-spacecraft.md` §1).
/// `shape_fraction` is 0 for every face this function is NOT called for
/// (+-Z, solar panels, and every GLONASS-K face -- RS14 Table 5.8 prints no
/// "shape" column at all): those need no caveat. Where `shape_fraction` >
/// 0, the citation states the blend fraction, states that only the flat-law
/// (s=0) special case is built, and states this UNDERSTATES the true
/// cylindrical contribution -- a stated, visible approximation (the
/// manager's own instruction, and this session's own established practice:
/// a schema gap is a finding reported, not silently built around, the SAME
/// treatment BeiDou's own curved surfaces get, just narrower in scope --
/// two face PAIRS of six, not the whole satellite, since RS14 itself gives
/// the flat-law (s=0) special case as one well-defined endpoint of its own
/// formula, unlike BeiDou's cylinders/rings/parabolic surfaces which have
/// no flat-law fallback in the CSNO standard at all).
[[nodiscard]] std::string with_shape_caveat(const std::string& base, double shape_fraction) {
    if (shape_fraction <= 0.0) return base;
    return base + " -- RS14 Sec.4.2's own 'shape' column states this face is a " +
           std::to_string(static_cast<int>(shape_fraction * 100.0 + 0.5)) +
           "% cylindrical / " + std::to_string(static_cast<int>((1.0 - shape_fraction) * 100.0 + 0.5)) +
           "% flat area blend (0%=flat, 100%=cylindrical, RS14's own words), with a DISTINCT force "
           "formula (RS14 Eq.4.5) for the blended fraction that this schema's own FlatSurface cannot "
           "represent (no cylinder surface type exists, SPEC-macromodel's own stated scope, the same "
           "gap BeiDou's own curved surfaces hit) -- built here under RS14's own flat-law (shape=0) "
           "special case ONLY, using RS14's own printed alpha/delta/rho as given; UNDERSTATES the true "
           "cylindrical contribution for this face; a stated, visible approximation, not a silent one, "
           "reported to the manager for a ruling (full record: "
           "~/.claude/handover/2026-09-24-odl-rewrite-L5.REPORT.md)";
}

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

[[nodiscard]] odl::Result<FlatSurface, MacromodelError>
solar_panels(const RS14Row& row, const std::string& area_citation, const std::string& optics_citation) {
    auto area = cited(row.area_m2, area_citation);
    auto absorptivity = cited(row.alpha, optics_citation);
    auto specular = cited(row.rs14_rho, optics_citation + " (RS14's own rho -- specular, formula-"
                                                            "verified, SPEC-spacecraft.md Sec.3)");
    auto diffuse = cited(row.rs14_delta, optics_citation + " (RS14's own delta -- diffuse, formula-"
                                                             "verified, SPEC-spacecraft.md Sec.3)");
    if (!area.has_value()) return odl::err(area.error());
    if (!absorptivity.has_value()) return odl::err(absorptivity.error());
    if (!specular.has_value()) return odl::err(specular.error());
    if (!diffuse.has_value()) return odl::err(diffuse.error());
    return flat_surface_sun_pointing(*area, *absorptivity, *specular, *diffuse);
}

struct GlonassFamilyRow {
    Face face;
    RS14Row row;
    double shape_fraction;  ///< 0 for GLONASS-K (no "shape" column at all)
};

[[nodiscard]] odl::Result<Macromodel, SpacecraftError>
assemble(const GlonassFamilyRow faces[6], const RS14Row& panels, const std::string& dims_citation,
        const std::string& optics_citation_base, double mass_kg, const std::string& mass_citation) {
    MacromodelBuilder b;
    for (int i = 0; i < 6; ++i) {
        const std::string optics_citation =
            with_shape_caveat(optics_citation_base, faces[i].shape_fraction);
        auto s = bus_face(faces[i].face, faces[i].row, dims_citation, optics_citation);
        if (!s.has_value()) return odl::err(SpacecraftError{s.error().id, s.error().message});
        b.add_surface(*s);
    }
    auto p = solar_panels(panels, dims_citation, optics_citation_base);
    if (!p.has_value()) return odl::err(SpacecraftError{p.error().id, p.error().message});
    b.add_surface(*p);

    auto mass = cited(mass_kg, mass_citation);
    if (!mass.has_value()) return odl::err(SpacecraftError{mass.error().id, mass.error().message});
    auto com = cited(Vec3{0.0, 0.0, 0.0},
                     "this tree's own default: RS14 states no centre-of-mass offset for this block, "
                     "the same treatment GPS's own RS14-sourced blocks get (SPEC-spacecraft.md §4)");
    if (!com.has_value()) return odl::err(SpacecraftError{com.error().id, com.error().message});
    b.set_mass(*mass).set_centre_of_mass(*com);

    auto m = std::move(b).build();
    if (!m.has_value()) return odl::err(SpacecraftError{m.error().id, m.error().message});
    return *m;
}

}  // namespace

odl::Result<Macromodel, SpacecraftError> glonass() {
    // RS14 Table 5.6, dimensions Revnivykh & Mitrikas (1998), optics RS14's
    // own generic Ziebart (2001) Sec.7.1 assumption (ASSUMED, not a
    // GLONASS-specific measurement -- RS14 states this itself, the SAME
    // status GPS-IIF's own optics carry, `gps_block_iif`).
    const GlonassFamilyRow faces[6] = {
        {Face::PlusX,  {3.310, 0.440, 0.448, 0.112}, 0.620},
        {Face::MinusX, {3.310, 0.440, 0.448, 0.112}, 0.620},
        {Face::PlusY,  {5.123, 0.440, 0.448, 0.112}, 0.494},
        {Face::MinusY, {5.123, 0.440, 0.448, 0.112}, 0.494},
        {Face::PlusZ,  {1.662, 0.374, 0.381, 0.245}, 0.0},
        {Face::MinusZ, {1.662, 0.705, 0.236, 0.059}, 0.0},
    };
    const RS14Row panels{23.616, 0.770, 0.035, 0.195};
    return assemble(faces, panels, "RS14 Table 5.6, dimensions Revnivykh and Mitrikas (1998)",
                    "RS14 Table 5.6's own generic assumption, same as Ziebart (2001) Sec.7.1 -- "
                    "ASSUMED for GLONASS, not a GLONASS-specific measurement (RS14 states this itself)",
                    1415.0, "RS14 Table 5.6's own caption");
}

odl::Result<Macromodel, SpacecraftError> glonass_m() {
    // RS14 Table 5.7, dimensions Mitrikas (2005).
    const GlonassFamilyRow faces[6] = {
        {Face::PlusX,  {4.530, 0.440, 0.448, 0.112}, 0.728},
        {Face::MinusX, {4.530, 0.440, 0.448, 0.112}, 0.728},
        {Face::PlusY,  {6.000, 0.440, 0.448, 0.112}, 0.550},
        {Face::MinusY, {6.000, 0.440, 0.448, 0.112}, 0.550},
        {Face::PlusZ,  {2.120, 0.409, 0.416, 0.175}, 0.0},
        {Face::MinusZ, {2.120, 0.791, 0.167, 0.042}, 0.0},
    };
    const RS14Row panels{30.850, 0.770, 0.035, 0.195};
    return assemble(faces, panels, "RS14 Table 5.7, dimensions Mitrikas (2005)",
                    "RS14 Table 5.7's own generic assumption, same as Ziebart (2001) Sec.7.1 -- "
                    "ASSUMED for GLONASS-M, not a GLONASS-M-specific measurement (RS14 states this "
                    "itself)",
                    1415.0, "RS14 Table 5.7's own caption");
}

odl::Result<Macromodel, SpacecraftError> glonass_k() {
    // RS14 Table 5.8: NO "shape" column at all (unlike Tables 5.6/5.7) --
    // an ordinary flat box-wing model, every shape_fraction 0. Dimensions:
    // "Mitrikas (personal communication, 2011)" -- RS14's own stated
    // source, the citation chain's own end, recorded the same way GPS-IIF's
    // "an unpublished document" is (`gps_block_iif`, SPEC-spacecraft.md §4).
    const GlonassFamilyRow faces[6] = {
        {Face::PlusX,  {2.210, 0.440, 0.448, 0.112}, 0.0},
        {Face::MinusX, {2.210, 0.440, 0.448, 0.112}, 0.0},
        {Face::PlusY,  {4.350, 0.440, 0.448, 0.112}, 0.0},
        {Face::MinusY, {4.350, 0.440, 0.448, 0.112}, 0.0},
        {Face::PlusZ,  {1.730, 0.402, 0.409, 0.189}, 0.0},
        {Face::MinusZ, {1.730, 0.540, 0.368, 0.092}, 0.0},
    };
    const RS14Row panels{16.960, 0.770, 0.035, 0.195};
    return assemble(faces, panels,
                    "RS14 Table 5.8, which itself states its own dimensions come from \"Mitrikas "
                    "(personal communication, 2011)\" -- this tree does not hold that communication; "
                    "recorded here as the end of the citation chain, not resolved further (the same "
                    "treatment GPS-IIF's own \"an unpublished document\" source gets, `gps_block_iif`)",
                    "RS14 Table 5.8's own generic assumption, same as Ziebart (2001) Sec.7.1 -- "
                    "ASSUMED for GLONASS-K, not a GLONASS-K-specific measurement (RS14 states this "
                    "itself)",
                    935.0, "RS14 Table 5.8's own caption");
}

}  // namespace odl::spacecraft
