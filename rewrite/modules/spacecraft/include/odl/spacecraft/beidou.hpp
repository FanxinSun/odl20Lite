#pragma once
// odl/spacecraft/beidou.hpp — L5 step 3: BeiDou, carried and refused.
//
// SPEC-spacecraft.md §3/§10 (SPCR-Q's own BeiDou entry). Rule-4/licence
// search performed 2026-09-24 against the CSNO 2019 standard (BD
// 420025-2019, "Definitions and descriptions of BDS/GNSS satellite
// parameters for high precision application"), the only source found for
// BeiDou's own per-satellite geometry, optics and attitude. `beidou()`
// REFUSES unconditionally -- a schema is not extended, nor a source used
// past its own stated limits, for a client that does not yet exist
// (`../plan/PLAN.md` §3.6's own principle, already applied to GPS-IIIA).

#include <odl/core/result.hpp>
#include <odl/macromodel/macromodel.hpp>
#include <odl/spacecraft/spacecraft.hpp>  // SpacecraftError

namespace odl::spacecraft {

/// SPCR-R-019. Refuses `SPCR-F-006` unconditionally, in every version until
/// this header's own stated conditions change: see the refusal's own
/// message (`beidou.cpp`) for the full, itemised reasoning -- surface
/// shapes this schema cannot hold, no populated centre-of-mass or any
/// other per-satellite numeric VALUE anywhere in the one source found (the
/// standard is a file-FORMAT specification, not a data product), one
/// attitude mode's own equations unread (OCR-garbled), no stated
/// redistribution terms, and no current consumer.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> beidou();

}  // namespace odl::spacecraft
