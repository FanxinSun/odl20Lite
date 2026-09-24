#pragma once
// odl/spacecraft/galileo.hpp — L5 step 2: Galileo, as cited data.
//
// SPEC-spacecraft.md, SPEC-galileo-attitude.md. Every function here returns a
// macromodel::Macromodel built entirely from Cited<T> values (SPCR-R-006):
// this module adds no exemption to MCRM-R-004 and creates none.
//
// GSC's own body frame is NOT this tree's own convention (SPEC-spacecraft.md
// §3): +Z is nadir-pointing (toward the L-band antenna) in both -- this
// tree's own +Z is ALSO nadir (`nominal_yaw_steering`'s own `z_body =
// -r_hat`, `modules/attitude`; SPEC-spacecraft.md §3's own "(+z, anti-nadir)"
// parenthetical is corrected in place there, a labelling error, not a
// convention difference) -- but GSC's own +X points toward DEEP SPACE, this
// tree's own +X toward the SUN. Every face normal and the centre of mass
// below is stated in THIS TREE's own convention, mapped from GSC's own
// "mechanical RF" by a rotation VERIFIED against GSC's own side-by-side
// "Mechanical RF"/"ANTEX RF" columns (the ARP, PCO and LRR tables give the
// SAME physical point in both frames) -- `galileo_frame_from_mechanical()`,
// checked by `SPCR-A-009`, not trusted from the yaw-law's own prose alone
// (the manager's own instruction, and the IIR 180° precedent, `PROVENANCE.md`
// §30.19/30.20).

#include <odl/core/result.hpp>
#include <odl/macromodel/macromodel.hpp>
#include <odl/spacecraft/spacecraft.hpp>  // SpacecraftError

#include <compare>

namespace odl::spacecraft {

/// GSC's own dated mass/CoM entry validity is monthly at best ("as of April
/// 2024", no day or time stated) -- a full `odl::time::Epoch` would overstate
/// the precision this source actually gives, the same reasoning
/// `odl::atmosphere::Day` already applies to NRLMSISE-00's own daily input
/// (`space_weather.hpp`'s own header comment). A satellite's own entry is
/// valid FROM this year/month (GSC's own "as of" pin) with no stated end --
/// GSC's own most recent update, valid until superseded.
struct YearMonth {
    int year = 0;
    int month = 0;  ///< 1-12

    friend bool operator==(const YearMonth&, const YearMonth&) = default;
    friend auto operator<=>(const YearMonth&, const YearMonth&) = default;
};

/// Rotates a unit vector from GSC's own "mechanical RF" (+X toward deep
/// space) to this tree's own body-frame convention (+X toward the Sun,
/// `SPEC-spacecraft.md` §3): 180° about Z -- (x,y,z) -> (-x,-y,z). Its own
/// inverse (an involution), so the same function converts either direction.
/// VERIFIED against three independent real coordinate pairs GSC prints
/// itself (IOV's own ARP and LRR, FOC's own ARP -- each given in BOTH
/// "Mechanical RF" and "ANTEX RF" for the identical physical point,
/// `SPCR-A-009`), not derived from the yaw law's own stated sign convention
/// alone, per the manager's own instruction. SPCR-R-008.
[[nodiscard]] Vec3 galileo_frame_from_mechanical(Vec3 mechanical) noexcept;

/// SPCR-R-009. `gsat` naming one of GSC's own three IOV satellites (101,
/// 102, 103); refuses `SPCR-F-004` otherwise. `epoch` at or after the
/// satellite's own mass/CoM entry's stated "as of" date; refuses
/// `SPCR-F-005` otherwise -- the same shape as `odl::atmosphere`'s own
/// space-weather coverage refusal, `SPEC-spacecraft.md` §3. Surfaces and
/// optics are GSC's own IOV Beginning-Of-Life table (§6.1); its own
/// End-Of-Life coefficients, printed for the same materials, are NOT built
/// this version (`SPCR-Q-004`).
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError>
galileo_iov(int gsat, YearMonth epoch);

/// SPCR-R-010. `gsat` naming one of GSC's own 26 FOC satellites currently
/// listed (201-227 except 205, 228-231, 232-234); refuses
/// `SPCR-F-004`/`SPCR-F-005` on the same terms as `galileo_iov`. Surfaces and
/// optics are GSC's own FOC table (§6.2), which prints one set of
/// coefficients per material, not a separate BOL/EOL pair.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError>
galileo_foc(int gsat, YearMonth epoch);

}  // namespace odl::spacecraft
