// beidou.cpp — SPEC-spacecraft, L5 step 3: BeiDou, carried and refused.
//
// Source: BD 420025-2019, "Definitions and descriptions of BDS/GNSS
// satellite parameters for high precision application" (CSNO, China
// Satellite Navigation Office, 2019), obtained via an ILRS mirror
// (ilrs.gsfc.nasa.gov/docs/2019/BeiDou_MetaData_191201.cn.en.pdf), fetched
// directly 2026-09-24. The only source found this session naming BeiDou's
// own per-satellite geometry, optics AND attitude in one place.

#include <odl/spacecraft/beidou.hpp>

namespace odl::spacecraft {

odl::Result<macromodel::Macromodel, SpacecraftError> beidou() {
    return odl::err(SpacecraftError{"SPCR-F-006",
        "BeiDou is carried, not built, in this version -- five independent reasons, any one of "
        "which alone would be sufficient, found reading BD 420025-2019 (CSNO 2019) closely, "
        "2026-09-24 (full record: ~/.claude/handover/2026-09-24-odl-rewrite-L5.REPORT.md):\n"
        "(a) CURVED SURFACES this schema cannot hold: Sec.5.3, quoted, states the satellite's own "
        "surface members are \"planes, cylinders, rings, parabolic, etc.\" -- FlatSurface and "
        "SphericalSurface (SPEC-macromodel.md's own stated scope) hold neither cylinders, rings nor "
        "a parabolic reflector; no source-side fallback (unlike RS14's own GLONASS cylinder-wing "
        "tables, which print a usable flat-law special case) was found in this document.\n"
        "(b) NO POPULATED PER-SATELLITE VALUE of ANY kind is printed anywhere in this document, for "
        "mass, centre of mass, area, or optics alike -- a REFINEMENT of an earlier, narrower reading "
        "that stopped at 'no centre-of-mass field': Sec.5.2's own normative text enumerates the "
        "'basic parameters' as mass, satellite type and laser-reflector position only, naming no "
        "centroid; Appendix A's own INFORMATIVE Table A.1 does print a 'Centroid coordinates' column "
        "alongside mass, for two illustrative satellites (C01, C02) -- but every cell in EVERY "
        "Appendix A table (A.1 through A.4, mass, laser-reflector, surface area, optics alike) holds "
        "only a checkmark (present-in-format), never an actual number: this document is a FILE-"
        "FORMAT specification, not a populated data product, so even a schema able to hold every "
        "shape BeiDou uses would have nothing here to build a real macromodel FROM.\n"
        "(c) MANEUVER-YAW MODE's own equations (Sec.5.4.4, Eq.2/Eq.3, one of the source's own three "
        "named attitude modes) extracted OCR-garbled ('...0.1/Sox)...05236...') -- needs the "
        "rendered PDF page, not attempted this round, so this mode is unverified regardless of the "
        "other four reasons.\n"
        "(d) NO REDISTRIBUTION TERMS stated: checked directly (a search of the full document text "
        "and of beidou.gov.cn's own generic footer), no licence or copyright notice of any kind "
        "found either place.\n"
        "(e) NO CONSUMER currently needs BeiDou -- no baseline in this tree reads this function; L8's "
        "own GNSS campaign is the three GPS baselines (`../plan/PLAN.md`). A schema is not extended, "
        "nor a source used past its own stated limits, for a client that does not yet exist."});
}

}  // namespace odl::spacecraft
