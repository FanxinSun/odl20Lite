#pragma once
// odl/spacecraft/spacecraft.hpp — L5 step 1: GPS, as cited data.
//
// SPEC-spacecraft.md. Every function here returns a macromodel::Macromodel
// built entirely from Cited<T> values (SPCR-R-006): this module adds no
// exemption to MCRM-R-004 and creates none. The body frame is the IGS
// convention, +x towards the Sun, for every block (SPEC-spacecraft.md §3).
//
// RS14's own alpha/delta/rho notation is mapped by the FORMULA RHS12
// (reprinted inside RS14 as P-II) itself writes, Eq.6, not by RS14's own
// Appendix Sec.5.1.2 prose (which is internally inconsistent with RHS12's
// own reprinted prose a few pages earlier in the SAME document): rho ->
// this schema's specular, delta -> this schema's diffuse. See
// SPEC-spacecraft.md §3 for the full statement, the formula, and the
// physical cross-check (glass-covered solar panels are predominantly
// specular).

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

/// SPCR-R-003. Mass 1080 kg, IGSMETA's own SATELLITE/MASS (SVN50, this
/// tree's own reference IIR-M satellite's block) -- documented for
/// non-gravitational force modelling, not launch mass; RS14's own 1100 kg
/// is the cross-check. See SPEC-spacecraft.md §4 for the full basis.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> gps_block_iir();

/// SPCR-R-005. gps_block_iir()'s own geometry AND (as of this version) mass
/// -- see the header comment above and SPEC-spacecraft.md §4 for the basis
/// and its own stated limits.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> gps_block_iir_m();

/// SPCR-R-004. RS14 Table 5.5's own dimensions and optics: dimensions cited
/// to RS14's own stated source, "an unpublished document" this tree does
/// not hold (the citation chain's own end, recorded rather than resolved
/// further); optical properties marked ASSUMED, RS14's own generic
/// (Ziebart 2001 §7.1) fallback, not an IIF-specific measurement. Mass
/// 1633 kg, IGSMETA's own SATELLITE/MASS for SVN63 -- documented for
/// non-gravitational force modelling, not launch mass (manager's ruling,
/// 2026-09-24, after the earlier launch-vs-on-orbit hypothesis was
/// withdrawn); RS14's own 1555 kg is the cross-check. The panel-span
/// aggregate check was sought and not completed, see SPEC-spacecraft.md §4.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> gps_block_iif();

/// Refuses SPCR-F-003 unconditionally in this version: one search performed,
/// no citable published per-surface source for Block IIIA was found
/// (SPEC-spacecraft.md §2, §4). No baseline consumes this function yet.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> gps_block_iiia();

}  // namespace odl::spacecraft
