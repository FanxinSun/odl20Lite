#pragma once
// odl/spacecraft/jason.hpp — L5 step 4: Jason-2, Jason-3 (built), Jason-1
// (carried, refused), as cited data.
//
// SPEC-spacecraft.md, SPEC-jason-attitude.md. Every function here returns a
// macromodel::Macromodel built entirely from Cited<T> values (SPCR-R-006's
// own rule, unchanged for this file).
//
// Source: CNES, "DORIS satellites models implemented in POE processing," ref.
// SALP-NT-BORD-OP-16137-CN, Ed.1/Rev.20, dated 2026-09-09, fetched directly
// 2026-09-24 from ids-doris.org/documents/BC/satellites/DORISSatelliteModels.pdf
// (SHA256 c0e7f3884888eef06a74d36b049e62e05de5092395c4c9e87edec53647ffb619 --
// pinned, the same edition `sentinel6.hpp` cites, RS14's own secondary-source
// ruling extended to this note). Cites the paywalled Cerri et al. 2010
// (Marine Geodesy 33(sup1):379-418) as its own ref.[6] -- not obtained,
// paywalled everywhere this session reached (Tandfonline, ScienceDirect,
// ResearchGate, Academia.edu, all HTTP 403), this note the actual usable
// route to the same values, written by the SAME lead author. "External
// diffusion: web site of the International DORIS Service"; that site's own
// site-wide Legal Notice page is an unfilled placeholder -- recorded, not
// treated as a clean licence, the same finding `sentinel6.hpp` records.
//
// A FIRST DRAFT OF THIS FILE USED A STALE TABLE: this session's own earlier
// cached notes (from an intermediate research pass) held DIFFERENT Jason-2/-3
// body-face and solar-array coefficients than the CURRENT edition actually
// prints -- caught by re-reading the fetched text directly before writing
// this file's own data (not trusted from the cached summary alone), and the
// note's own revision history (INTRODUCTION section) explains why: the
// solar-array optics were tuned at least twice after the ORIGINAL model
// (0.3440/0.0060/0.6470 spec/diff/abs) -- once to an intermediate set
// (0.0600/0.4070/0.5330, matching this session's own stale cache) and again
// to the CURRENT printed set (0.1000/0.2950/0.6050) -- a real, admitted
// revision history, not a transcription slip on the note's own part. The
// values below are read from the CURRENT fetch, hash-pinned above, not any
// intermediate one.

#include <odl/core/result.hpp>
#include <odl/macromodel/macromodel.hpp>
#include <odl/spacecraft/spacecraft.hpp>  // SpacecraftError

namespace odl::spacecraft {

/// `ja2mass.txt`/`ja3mass.txt`'s own dense, thousands-of-row operational
/// offset log is NOT "Galileo's shape" (a small, stable, dated table,
/// `galileo.hpp`'s own `YearMonth`) -- REQUIRED, no default (the manager's
/// own instruction, "an explicitly named selector"), with exactly one legal
/// value today: a future round that adds real epoch-based lookup extends
/// this enum, forcing every call site to choose, rather than silently
/// keeping today's baseline-only behaviour.
enum class JasonMassSource { Baseline };

/// SPCR-R-021. Jason-2's own macromodel: 6 body-fixed bus `FlatSurface`s
/// plus 2 body-fixed solar-array `FlatSurface`s (`SALP-NT-BORD-OP-16137-CN`
/// §7.3's own table -- NOT `flat_surface_sun_pointing`: the source states a
/// FIXED normal, (+1,0,0)/(-1,0,0), for the array rows too, "in sat ref
/// frame" the same as every bus face -- a genuine difference from every
/// GPS/Galileo/QZSS panel this tree has built so far, all of which track
/// the Sun; this satellite family's own array tracking, if any beyond this
/// static reference orientation, is not part of this macromodel table
/// (`SPCR-Q`, this file's own open question)). Every row carries BOTH a
/// visible and an infrared `OpticalTriple`, the source's own table printing
/// both columns throughout (the same `BandedOptics` use `sentinel6()` makes,
/// its own first consumer). Mass and centre of mass are §7.1's own BASELINE
/// values (505.9 kg; (0.9768, 0.0001, 0.0011) m) under an EXPLICITLY NAMED
/// selector (`JasonMassSource`) -- `ja2mass.txt`'s own per-event offset log
/// IS openly retrievable (confirmed, fetched directly, 4134 lines) but its
/// own SHAPE -- a dense, ever-growing operational correction history, not a
/// small stable dated table -- is a poor match for the epoch-lookup pattern
/// `galileo_iov`/`_foc` already use (~30 stable rows); NOT embedded this
/// round, the manager's own explicitly offered fallback taken instead
/// (`SPCR-Q`). Always succeeds -- one satellite, one table, no per-epoch or
/// per-life-stage axis this source offers.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError>
jason2(JasonMassSource source);

/// SPCR-R-022. Jason-3's own macromodel -- `SALP-NT-BORD-OP-16137-CN`
/// §12.3's own table, IDENTICAL to Jason-2's own §7.3 table cell by cell
/// (checked directly, `SPCR-A-025`; the note's own words, both directions:
/// §7.3 "the macro-model is the same as for Jason-3," §12.1 "the a priori
/// SRP geometry and properties are identical for the two satellites") --
/// built from its OWN citation (§12.3, not a call into `jason2()`), the same
/// "independently cited, not silently inherited" treatment every other
/// shared-geometry case in this tree gets (`gps_block_iir_m`'s own citation
/// naming the inheritance explicitly, rather than this file reusing that
/// pattern by calling through). Mass and centre of mass: §12.1's own
/// baseline (509.6 kg; (1.0023, 0.0000, -0.0021) m), same `JasonMassSource`
/// selector and same reasoning as `jason2()`.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError>
jason3(JasonMassSource source);

/// SPCR-R-023. Jason-1 is carried, not built -- refuses `SPCR-F-007`
/// unconditionally: `SALP-NT-BORD-OP-16137-CN` §6.3 states directly its own
/// optics were "slightly modified by tuning" and carry "a scale factor
/// equal to 0.97... that multiplies the solar radiation pressure force" --
/// neither is what `MacromodelBuilder`'s own schema holds (an optical triple
/// this schema can represent sums to 1 by construction, `MCRM-R-004`'s own
/// implicit physical meaning every OTHER built triple in this tree satisfies,
/// `SPCR-A-001`/`SPCR-A-010`/etc.; a satellite-wide force-scale factor
/// outside any single surface's own optics has no field in this schema at
/// all). Checked directly, not assumed from the word "tuning" alone: every
/// one of Jason-1's own six body-face rows sums to something other than 1
/// (e.g. +X: 0.0938+0.2811+0.2078=0.5827; +Y: 1.1880-0.0113-0.0113=1.1654,
/// with NEGATIVE diffuse/absorptivity entries the schema's own physical
/// triple cannot hold either). No consumer needs Jason-1 currently (no
/// baseline in this tree reads this function) -- the same "no consumer"
/// closing reason BeiDou's and GPS-IIIA's own refusals already carry.
[[nodiscard]] odl::Result<macromodel::Macromodel, SpacecraftError> jason1();

}  // namespace odl::spacecraft
