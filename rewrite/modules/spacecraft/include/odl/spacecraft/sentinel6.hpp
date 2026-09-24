#pragma once
// odl/spacecraft/sentinel6.hpp — L5 step 4: Sentinel-6 Michael Freilich, as cited data.
//
// SPEC-spacecraft.md, SPEC-sentinel6-attitude.md. Every function here returns a
// macromodel::Macromodel built entirely from Cited<T> values (SPCR-R-006's own
// rule, unchanged for this file).
//
// Source: CNES, "DORIS satellites models implemented in POE processing," ref.
// SALP-NT-BORD-OP-16137-CN, Ed.1/Rev.20, dated 2026-09-09, fetched directly
// 2026-09-24 from ids-doris.org/documents/BC/satellites/DORISSatelliteModels.pdf
// (SHA256 c0e7f3884888eef06a74d36b049e62e05de5092395c4c9e87edec53647ffb619 --
// pinned, since the note states it is revised over time, RS14's own secondary-
// source ruling, SPEC-spacecraft.md §2.2, extended 2026-09-24 to this note the
// same way it already covers RS14 itself). Written by the SAME lead author
// (Luca Cerri) as the paywalled Cerri et al. 2010 (Marine Geodesy 33(sup1):
// 379-418) this note itself cites as its own ref.[6] -- the rule-4 search
// found the 2010 paper paywalled everywhere reachable (Tandfonline, ScienceDirect,
// ResearchGate, Academia.edu, all HTTP 403) and this note as the actual usable
// route to the same values. States its own "External diffusion: web site of the
// International DORIS Service" (ids-doris.org); that site's own site-wide Legal
// Notice page (ids-doris.org/legal-notice.html) is an unfilled placeholder,
// verbatim "To be added... Last Updated: 29 June 2022" -- checked directly,
// recorded here per rule 4, not silently treated as a clean licence.
//
// FRAME: this file's own §16.3 macromodel table states each face's own normal
// "in sat ref frame" -- the SAME name the companion attitude-law note
// (`SwotAndSentinel6AttitudeLaws.pdf`, `modules/attitude`'s own Sentinel-6
// citation) uses for the platform axes its own rotation matrix builds
// (Rsat,Tsat,Nsat <-> -z,x,-y). Both documents are CNES's own, both name
// Sentinel-6, and the macromodel note's own §16.2 explicitly points to the
// attitude note as this satellite's law -- treated here as the SAME physical
// frame, no additional mapping applied. NOT independently verified by a
// printed coordinate pair or real attitude data the way Galileo's/QZSS's own
// frame mappings are (`SPCR-Q`, this file's own open question) -- a stated
// assumption, not a silently asserted one.

#include <odl/core/result.hpp>
#include <odl/macromodel/macromodel.hpp>
#include <odl/spacecraft/spacecraft.hpp>  // SpacecraftError

namespace odl::spacecraft {

/// SPCR-R-020 (`SPEC-spacecraft.md`). Sentinel-6's own macromodel: 12
/// body-fixed `FlatSurface`s, `SALP-NT-BORD-OP-16137-CN` §16.3's own table,
/// every row carrying BOTH a visible and an infrared `OpticalTriple` (the
/// note prints both columns for every row; `MCRM-R-004`'s own optional
/// infrared field, unused by every constellation built before this round,
/// is populated here for the first time). Two of the twelve normals
/// (0,0.616,-0.788) and (0.469,0,-0.833) as PRINTED do not renormalize to
/// unit length (1.0002 and 0.9560 respectively, checked directly against
/// the rendered PDF page, not an extraction artefact) -- `body_direction()`
/// requires exact unit length (`MCRM-F-002`), so every non-axis-aligned
/// normal is renormalised from its own printed components before
/// construction, cited as the source's own printed value with this
/// adjustment stated, not silently smoothed over. Mass and centre of mass
/// are `SALP-NT-BORD-OP-16137-CN` §16.1's own BASELINE values (1191.831 kg;
/// CoM (1.5274,-0.0073,0.0373) m) -- the note's own per-epoch offset file
/// (`s6amass.txt`, confirmed openly retrievable, ~30 data rows) is NOT wired
/// into an epoch lookup this round: no consumer need for sub-1%/sub-cm
/// precision is stated, and this round's own new work is the appendix
/// reproduction and the attitude law, not a new epoch-lookup capability for
/// this module (`SPCR-Q`, this file's own open question names the gap).
/// Always succeeds -- one satellite, one table, no per-epoch or per-life-stage
/// axis the source itself offers.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> sentinel6();

}  // namespace odl::spacecraft
