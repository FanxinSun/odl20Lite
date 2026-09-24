#pragma once
// odl/spacecraft/spacecraft.hpp — L5 step 1: GPS, as cited data.
//
// SPEC-spacecraft.md. Every function here returns a macromodel::Macromodel
// built entirely from Cited<T> values (SPCR-R-006): this module adds no
// exemption to MCRM-R-004 and creates none. The body frame is the IGS
// convention, +x towards the Sun, for every block (SPEC-spacecraft.md §3).
//
// RS14's own alpha/delta/rho notation is NOT this tree's own
// absorptivity/specular/diffuse order -- RS14's delta (its own "reflection
// coefficient") maps to this schema's specular, and RS14's rho (its own
// "diffusion coefficient") maps to this schema's diffuse, the OPPOSITE
// letter-to-meaning pairing from RHS12's own restated form already used
// elsewhere in this tree. See SPEC-spacecraft.md §3 for the full statement
// and why the swap is checked against the source directly, not assumed.

#include <odl/core/result.hpp>
#include <odl/macromodel/macromodel.hpp>

namespace odl::spacecraft {

using SpacecraftError = odl::Diagnostic;

/// SPCR-R-001. `svn` must be one of RS14 Table 5.2's own eight GPS Block I
/// satellites (450 kg: 03, 04, 06; 520 kg: 08, 09, 10, 11); refuses
/// SPCR-F-001 otherwise.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> gps_block_i(int svn);

/// SPCR-R-002. Same surface geometry for II and IIA (RS14's own single
/// table); `is_iia` selects only the mass, 880 kg / 975 kg.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> gps_block_ii_iia(bool is_iia);

/// SPCR-R-003.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> gps_block_iir();

/// SPCR-R-005. gps_block_iir()'s own geometry, mass unchanged -- see the
/// header comment above and SPEC-spacecraft.md §4 for the basis and its own
/// stated limits.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> gps_block_iir_m();

/// SPCR-R-004. Refuses SPCR-F-002 unconditionally in this version: no
/// citable published per-surface source for Block IIF was found
/// (SPEC-spacecraft.md §2, §4).
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> gps_block_iif();

}  // namespace odl::spacecraft
