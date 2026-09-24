#pragma once
// odl/spacecraft/qzss.hpp — L5 step 3: QZSS (QZS-1), as cited data.
//
// SPEC-spacecraft.md, SPEC-qzss-attitude.md. Every function here returns a
// macromodel::Macromodel built entirely from Cited<T> values (SPCR-R-006's
// own rule, unchanged for this file).
//
// The Cabinet Office's own QZS-1 SPI ("SPI_QZS1_B") states its own body
// frame (Sec.2): origin at the launch-adapter-plane centre, +Z the L-ANT
// boresight (nadir, matching this tree's own +Z), +Y parallel to the solar
// panel rotation axis, +X completing the right-handed system. Unlike
// GALSC, no paired native/tree coordinates are printed to verify a mapping
// against directly -- the document instead states a TESTABLE PROPERTY
// (Sec.3(1): during yaw-steering, "the Sun is located in the negative
// hemisphere" of its own x) that pins the SAME 180-about-Z relation this
// tree's own attitude law derives and a real-data control confirms
// (`SPEC-qzss-attitude.md` §3) -- `qzss_frame_from_native()` below is that
// mapping, its own function (not a reuse of `galileo_frame_from_mechanical`,
// even though the two happen to share the same 180-about-Z form): QZSS's
// own mapping is an independent finding from a different source, verified
// its own way, not inherited from Galileo's.

#include <odl/core/result.hpp>
#include <odl/macromodel/macromodel.hpp>
#include <odl/spacecraft/spacecraft.hpp>  // SpacecraftError

namespace odl::spacecraft {

/// SPI_QZS1_B Table 1's own two dated mass/CoM entries -- REQUIRED, no
/// default (the manager's own instruction, "mirroring OpticalLife
/// exactly," `SPEC-spacecraft.md` SPCR-R-009's own precedent): BOL
/// ("Completion of QZS-orbit insertion") and EOL ("12 years after
/// launch") give GENUINELY DIFFERENT mass (2281 vs 2121 kg) and CoM,
/// unlike Galileo's own IOV mass/CoM (one dated entry only) -- a caller
/// must state which, not receive a silently-defaulted answer.
enum class QzssLife { BeginningOfLife, EndOfLife };

/// Rotates a unit or offset vector from QZSS's own native body frame
/// (SPI_QZS1_B Sec.2) to this tree's own convention: 180 deg about Z,
/// (x,y,z) -> (-x,-y,z), its own inverse (an involution) -- the SAME form
/// `galileo_frame_from_mechanical` has, independently derived and verified
/// for QZSS (this header's own top comment), not a call into that
/// function or an assumption the two constellations share a mapping.
[[nodiscard]] Vec3 qzss_frame_from_native(Vec3 native) noexcept;

/// SPCR-R-016. QZS-1's own macromodel (`SPI_QZS1_B`): six body-fixed
/// `FlatSurface`s per face's own material(s) (§6 Table 4, multi-material
/// faces built as separate co-normal surfaces, the same pattern
/// `galileo_iov`/`_foc` already use) plus one `flat_surface_sun_pointing`
/// (the SAP material, +Y and -Y areas summed -- Table 4's own footnote *2
/// states a row's "Location" does not necessarily name the direction
/// normal to it, and §2's own frame statement names +Y as the solar
/// panels' own ROTATION axis, not their normal: the SAP is the
/// Sun-tracking array, the same treatment GPS's and Galileo's own panels
/// already get). The +Z face's own "L-ANT Cover" material is OMITTED, not
/// approximated: Table 4 gives it no area at all (footnoted "(*1)"
/// instead), and states its own shape is a truncated cone, which this
/// schema cannot hold regardless -- a genuine gap, reported, not built
/// around (`SPEC-spacecraft.md` §3). `life` selects `SPI_QZS1_B` Table 1's
/// own BOL or EOL mass/CoM entry, REQUIRED, no default.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> qzss_1(QzssLife life);

}  // namespace odl::spacecraft
