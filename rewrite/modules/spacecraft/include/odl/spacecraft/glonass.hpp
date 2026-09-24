#pragma once
// odl/spacecraft/glonass.hpp — L5 step 3: GLONASS, GLONASS-M, GLONASS-K, as
// cited data (RS14's own secondary-source ruling, extended from GPS to these
// three blocks, 2026-09-24, `../plan/subplan_L5/L5-3.md`).
//
// Every function here returns a macromodel::Macromodel built entirely from
// Cited<T> values (SPCR-R-006's own rule, unchanged for this file). The body
// frame is the SAME IGS convention every other block in this tree uses, +x
// towards the Sun, +z nadir (RS14 Sec.5.1.1 states this identically, not
// re-derived -- SPEC-spacecraft.md §3).
//
// A GENUINE, REPORTED LIMITATION (not silently built around): RS14 Sec.4.2
// states the GLONASS and GLONASS-M bus is "actually" a cylinder-wing model,
// not a box-wing one -- its own +-X/+-Y bus faces are a "shape"-weighted
// BLEND of a flat surface and a cylindrical one (0=flat, 1=cylindrical,
// RS14's own words), with a DISTINCT force formula (Eq.4.5) for the blend
// that this schema's own FlatSurface cannot represent (no cylinder surface
// type exists, `SPEC-macromodel.md`'s own stated scope, the SAME gap
// BeiDou's own curved surfaces hit). Built here using ONLY RS14's own
// printed alpha/delta/rho numbers under the flat-law special case (s=0),
// UNDERSTATING the true cylindrical contribution for these two face pairs
// -- a stated, visible approximation, not a silent one, flagged in each
// affected value's own citation string and reported to the manager for a
// ruling (SPEC-spacecraft.md §3). GLONASS-K's own table carries no "shape"
// column at all (RS14 Sec.5.8): built as an ordinary flat box-wing model,
// no approximation needed.

#include <odl/core/result.hpp>
#include <odl/macromodel/macromodel.hpp>
#include <odl/spacecraft/spacecraft.hpp>  // SpacecraftError

namespace odl::spacecraft {

/// RS14 Table 5.6 (mass 1415 kg, dimensions Revnivykh & Mitrikas 1998):
/// six bus faces plus solar panels. The +-X/+-Y bus faces carry RS14's own
/// stated "shape" blend (0.620/0.494 respectively) -- see this header's own
/// top comment; built here under the flat-law (s=0) special case, cited as
/// such, not the full cylinder-blended Eq.4.5 force.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> glonass();

/// RS14 Table 5.7 (mass 1415 kg, dimensions Mitrikas 2005): same shape and
/// caveat as `glonass()`, different areas/optics/shape fractions
/// (0.728/0.550).
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> glonass_m();

/// RS14 Table 5.8 (mass 935 kg): six ORDINARY flat bus faces plus solar
/// panels, no "shape" column (RS14 prints none for this block) -- no
/// cylinder-blend approximation needed. Dimensions cited to RS14's own
/// stated source, "Mitrikas (personal communication, 2011)" -- the
/// citation chain's own end, recorded the same way GPS-IIF's "an
/// unpublished document" is (`gps_block_iif`, `SPEC-spacecraft.md` §4).
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> glonass_k();

}  // namespace odl::spacecraft
