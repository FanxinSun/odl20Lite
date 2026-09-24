# SPEC-spacecraft — the macromodel library, as cited data

| | |
|---|---|
| **Spec ID** | `SPCR` |
| **Status** | **draft** 2026-09-24, for review |
| **Version** | 2.5 — L5 step 4 opens and closes: `sentinel6()` built from CNES's own DORIS satellite-models note, the first macromodel in this tree to populate `BandedOptics`'s own infrared field; `jason2()`/`jason3()` built from the SAME note (a stale, cached earlier draft of this table CAUGHT and corrected before commit, see §3); `jason1()` refuses, non-energy-conserving optics and a scale factor the schema cannot hold |
| **Date** | 2026-09-24 |
| **Layer** | L5 `spacecraft` (`../plan/PLAN.md` §3.6), steps 1 (GPS), 2 (Galileo), 3 (GLONASS, QZSS, BeiDou) and 4 (Sentinel-6, Jason-2, Jason-3, Jason-1) |
| **Depends on** | `macromodel` (the schema this spec populates, not extends) |
| **Depended on by** | L7's own box-wing fit, which reads this library |

**Derivation declaration (plan R1).** Written from the sources §2 names, and from no
implementation of this module beyond `macromodel`'s own already-built, already-gated schema.

**Predecessor access.** No file under the repository root's `src/`, `include/`, `res/`,
`scripts/`, `analysis/`, `analyses/`, `REVIVAL.md` or `PROVENANCE.md` was opened, read, listed,
searched or otherwise inspected during this specification's preparation, per `../plan/PLAN.md`
§3.6's own explicit refusal of the predecessor's hand-built surface models.

---

## 1. Purpose and scope

**Named GPS and Galileo satellite constructors**, each returning a `macromodel::Macromodel` built
from **published values, each with its own citation** — this layer's own exit gate
(`../plan/PLAN.md` §3.6, amended 2026-09-24): "every value resolves to a citation, and the library
refuses to build if any does not." This spec covers:

- **L5 step 1, GPS**, across its own six blocks: I, II, IIA, IIR, IIR-M, and IIF — plus one
  further block, IIIA, searched once and refused (`SPCR-R-007`) rather than built, no citable
  per-surface source having been found for it.
- **L5 step 2, Galileo**, across its own two blocks: IOV and FOC (`SPCR-R-009`/`-010`) — the
  operator's own published metadata, first-party, per-satellite, and dated: a satellite's own
  macromodel is returned for a stated epoch, refusing one the source's own table does not cover
  (§3, §4).
- **L5 step 3, GLONASS**, across its own three blocks: GLONASS, GLONASS-M and GLONASS-K
  (`SPCR-R-012`–`-014`) — `RS14`'s own secondary-source ruling (§2.2), extended from GPS to these
  three tables 2026-09-24 (the manager's own ruling, `../plan/subplan_L5/L5-3.md`). A genuine
  schema-fidelity limitation, found while building GLONASS's and GLONASS-M's own bus faces and
  REPORTED rather than silently built around, is recorded in full at §3 below and `SPCR-Q-007`.
- **L5 step 3, QZSS**, QZS-1 (`SPCR-R-016`) — the Cabinet Office's own first-party, per-satellite
  SPI document, geometry and optics stated in plain English (no notation to resolve), mass/CoM
  explicit at beginning or end of life (`QzssLife`, REQUIRED, no default, mirroring Galileo's own
  `OpticalLife`). A second genuine schema-fidelity limitation, found the SAME day as GLONASS's own
  (§3 below): the +Z face's own "L-ANT Cover" material has no printed area at all and is stated to
  be a cone shape this schema cannot hold regardless — OMITTED, not approximated.
- **L5 step 4, altimetry: Sentinel-6, Jason-2 and Jason-3** (`SPCR-R-020`–`-022`) — a CNES technical
  note (`SATMOD`, the actual usable route to the paywalled Cerri et al. 2010, the same rule-4 finding
  QZSS's own `SPI_QZS1_B` search pattern already established for a different constellation), used
  under the `RS14` secondary-source ruling extended to this note (§2.4 below). The FIRST constellation
  in this tree to populate `BandedOptics`'s own infrared field (`MCRM-R-004`'s own optional band,
  built at L4 step 2 but unused by every constellation before this round, §3 below). Jason-2's and
  Jason-3's own macromodel is the SAME table (checked cell by cell, not merely quoted) — including
  their own solar array, which the source states as BODY-FIXED, not Sun-tracking, unlike every panel
  this tree has built before this round. **Jason-1 is carried, refused** (`SPCR-R-023`) — tuned,
  non-energy-conserving optics and an overall force-scale factor, neither of which this schema holds.

**Not in scope.** The schema itself (`SPEC-macromodel.md`, L4 step 2) — this spec populates it,
never extends it; a value the schema cannot hold is a finding reported to the manager, not a
silent schema change (`../plan/PLAN.md` §3.6's own instruction). Estimating any parameter from
observations (`SPEC-dynamics`, L7) — this library states values, it does not fit them. The
ATTITUDE LAW's own equations (Galileo's, GLONASS-M's and QZSS's own laws included) — code, not
cited data, built in `modules/attitude` and specified in `SPEC-galileo-attitude.md`/
`SPEC-glonass-attitude.md`/`SPEC-qzss-attitude.md`, beside GPS's own `SPEC-thrust-yaw.md`; this
spec's own §3 states only the FRAME the macromodel's own face normals are stated in, which that
law's own output must agree with (checked, `SPCR-A-009`/`SPCR-A-020`, not merely assumed by the
specs matching prose). BeiDou (carried, refused — `SPEC-spacecraft` §2's own rule-4 search names
the reasons, `SPCR-F-006`). The force law itself (`srp_analytic`,
`photon_force`) — this spec's own output is consumed by that module, unchanged. Sentinel-6's and
Jason's own ATTITUDE laws — code, not cited data, `SPEC-sentinel6-attitude.md`/
`SPEC-jason-attitude.md`, the same split GPS's/Galileo's/GLONASS-M's/QZSS's own laws already have
from this spec.

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `FLGA92` | Fliegel, H. F., Gallini, T. E., Swift, E. R. | *Global Positioning System radiation force model for geodetic applications* | J. Geophysical Research 97(B1): 559–568, 1992 | DOI (paywalled) | **not obtained** — see §2.1 | normative for Block I, II, IIA; not read directly, reached only through `RS14`'s own derived table |
| `FLGA96` | Fliegel, H. F., Gallini, T. E. | *Solar force modeling of block IIR Global Positioning System satellites* | J. Spacecraft and Rockets 33(6): 863–866, 1996 | DOI `10.2514/3.26851` (paywalled) | **not obtained** — see §2.1 | normative for Block IIR; not read directly, reached only through `RS14`'s own derived table |
| `RS14` | Rodríguez-Solano, C. J. | *(doctoral dissertation)*, TU München | 2014 | `data/literature/rodriguez-solano-2014-dissertation/719708.pdf`, retrieved at L4 step 2 (`PROVENANCE.md` §26.1) | **secondary** — retrievable, no login; redistribution terms not stated by the source, see §2.2 | the actual source of every Block I/II/IIA/IIR numeric value this spec states — a derivation from `FLGA92`/`FLGA96`, not a transcription of either. **Extended 2026-09-24** (the manager's own ruling, `../plan/subplan_L5/L5-3.md`) to GLONASS, GLONASS-M and GLONASS-K (its own Tables 5.6–5.8, §5.4 below) — the SAME secondary-source status, cited per value |
| `MSGA15` | Montenbruck, O., Schmid, R., Mercier, F., Steigenberger, P. *et al.* | *GNSS satellite geometry and attitude models* | Adv. Space Res. 56: 1015–1029, 2015 | `https://elib.dlr.de/97732/1/ASR_151015_GNSS_SatGeomAtt.pdf` | **primary**, already used at L4 step 6 | the IGS body-frame convention (§3), and IIR-M's own basis for sharing IIR's geometry (§4, `SPCR-R-005`) |
| `IGSMETA` | Steigenberger, P., Montenbruck, O. (maintainers); IGS | *IGS Satellite Metadata (SINEX)* | continuously updated; this pin 2026-09-24 | `https://files.igs.org/pub/station/general/igs_satellite_metadata.snx` | **primary**, open with attribution (IGS's own open data policy, re-checked this session for the redistribution bar, not only retrievability) | which physical block each frozen baseline SVN is (already used, `PROVENANCE.md` §30.1); **as of this version, the primary mass source for `gps_block_iir`/`_iir_m`/`_iif`** (§3, §4 `SPCR-R-003`/`-004`/`-005`) — its own `SATELLITE/MASS` field, per-satellite, SVN50/SVN63 |
| `SMSD24` | Steigenberger, P., Montenbruck, O. | *IGS Satellite Metadata File Description*, v1.10 | 30 September 2024, DOI `10.57677/metadata-sinex` | `https://files.igs.org/pub/resource/working_groups/multi_gnss/Metadata_SINEX_1.10.pdf`, fetched directly 2026-09-24 (same `files.igs.org` domain as `IGSMETA`; redistribution not separately re-verified beyond that) | **primary**, obtained | states what `IGSMETA`'s own `SATELLITE/MASS` field IS — "in-orbit satellite mass," required "to compute the acceleration caused by non-gravitational forces... at ~1% accuracy" (§1.1) — settling `SPCR-Q-003` and its own Table 5 (§4.3), the block-level figures §3 below cites |
| `GALSC` | European GNSS Service Centre (GSC); EUSPA/EU | *Galileo Satellite Metadata* | continuously updated; this pin 2026-09-24 | `https://www.gsc-europa.eu/support-to-developers/galileo-satellite-metadata`, fetched directly (`curl`, HTTP 200, no login) | **primary**, first-party, open with attribution — see §2.3 | the actual source of EVERY Galileo value this spec states for IOV/FOC: reference frame (§2), yaw-steering law (§3, consumed by `SPEC-galileo-attitude.md`, not this spec), mass and centre-of-mass history per satellite (§4), geometry and optical coefficients per surface (§6) |
| `SPI_QZS1_B` | Cabinet Office, Government of Japan, National Space Policy Secretariat | *QZS-1 Satellite Information* | rev. B, 2022-03-24 | `https://qzss.go.jp/en/technical/qzssinfo/khp0mf0000000wuf-att/spi-qzs1_b.pdf`, fetched directly 2026-09-24 | **primary**, first-party, "freely available to any user... shall indicate proper credit" (`qzss.go.jp/en/technical/qzssinfo/index.html`, quoted in full) | the actual source of EVERY QZS-1 value this spec states: reference frame (§2), attitude law (§3, consumed by `SPEC-qzss-attitude.md`, not this spec), mass and CoM at beginning/end of life (§4), geometry and optical coefficients per face (§6) |
| `SATMOD` | CNES | *DORIS satellites models implemented in POE processing* | Ed.1/Rev.20, 2026-09-09 | `SALP-NT-BORD-OP-16137-CN`, `https://ids-doris.org/documents/BC/satellites/DORISSatelliteModels.pdf`, fetched directly 2026-09-24, SHA256 `c0e7f3884888eef06a74d36b049e62e05de5092395c4c9e87edec53647ffb619` | **secondary** — the `RS14` ruling extended, §2.4 below | Sentinel-6's own mass/CoM (§16.1) and 12-surface macromodel (§16.3); Jason-1's/-2's/-3's own mass/CoM (§6.1/§7.1/§12.1) and macromodel (§6.3/§7.3/§12.3); Appendix 1's own numerical SRP worked example, consumed by `SPEC-srp-analytic.md`'s own `SRPA-A-011`–`A-013`, not this spec |

### 2.1 `FLGA92`/`FLGA96`: the search, and the one route this session added

Neither paper's full text was reached. AGU (`FLGA92`'s own publisher) runs a rolling 24-month
free-access embargo from **1997** onward and explicitly excludes its own pre-1997 "backfile" —
checked directly this session, at the manager's own instruction to try a publisher's own
free-access-after-embargo route before concluding. `FLGA92` (1992) is five years outside that
window; the route does not apply. AIAA (`FLGA96`'s own publisher) was not separately re-checked
under this route, `FLGA96` being newer but still pre-1997 and AIAA's own embargo policy, where one
exists, not expected to reach 1996 either — recorded as not separately verified, not as a second
finding. A DTIC technical report that *cites* `FLGA92` (`apps.dtic.mil/sti/tr/pdf/ADA485820.pdf`,
not a copy of the paper itself) returned HTTP 403 to an automated fetch. No account and no request
form was used, per this layer's own standing instruction.

### 2.2 `RS14`: what was checked, and what remains unresolved

`RS14` is retrievable (`mediatum.ub.tum.de`, no login, no request form — currently served behind
an automated bot challenge that blocks a fresh automated re-check, unchanged from L4 step 2's own
finding, `PROVENANCE.md` §26.1). Its own redistribution terms: TU München's own published policy
pages (`ub.tum.de/en/publishing-mediatum`, `/en/theses`, `/en/copyright-law`,
`/en/copyright-declaration`) state that a **publication-based thesis** — which `RS14` is, since it
reprints `RHS12` in full as one chapter — does not have its third-party rights cleared by TUM on
the author's behalf; the German research exception (§60c UrhG) covers personal scientific
reproduction up to 15% of a work, narrower than redistribution inside a software library. No
statement permitting general redistribution of `RS14`'s own content, or of values derived from it,
was found either way.

**Ruled 2026-09-24** (`../plan/subplan_L5/L5-1.md`, `c6f6e0b`): used anyway. This library was
designed as published values, each with its own citation — the plan names `FLGA92`/`FLGA96`,
published literature, as this step's own source — and `../plan/PLAN.md` §3.6's own database-right
warning is about the predecessor's hand-built hundred-plus-surface compilation, not about citing a
handful of published physical parameters, which is how this field cites its models. `RS14`'s own
tables here are six or seven rows each (§4), far from that scale, and are themselves a
**derivation** from `FLGA92`/`FLGA96` (an area-weighted average over each surface's own flat and
cylindrical elements, `RS14` §5.1.3), not a verbatim transcription of either paper's own printed
table — a materially different case from the predecessor's own compilation. Recorded here, on the
record, rather than only in a report, per the ruling's own instruction.

### 2.3 `GALSC`: rule-4 and licence search, reported before Galileo was built

Performed 2026-09-24, before any Galileo code existed, per the manager's own instruction ("as for
GPS"). **What it prints**: the reference frame per IOV and FOC; the yaw-steering law's own
equations, in GSC's own native frame AND again converted to the GPS/ANTEX convention (§3.2 of the
page itself); per-satellite dated mass and centre-of-mass (§4, "as of" a stated month, updated in
place as GSC's own most recent measurement, not a full multi-entry history); per-surface,
multi-material optical coefficients (§6); antenna, laser-retroreflector and signal-bias data not
consumed by this spec. Everything is printed directly ON the page (an HTML table structure,
re-parsed with rowspan/colspan expanded, not from an earlier flattened-text pass that lost some
row alignment), not behind a separate linked PDF.

**Retrievable without an account**: yes — direct `curl`, HTTP 200, no login, no bot challenge, the
same plain-request pattern that worked for `IGSMETA` and did NOT work for most sources L5 step 1
needed (AGU, AIAA/DTIC, IEEE Xplore, Wiley, TUM mediaTUM, ScienceDirect).

**Redistribution terms**, GSC's own Terms of Use (`https://www.gsc-europa.eu/terms-of-use`, fetched
directly), Copyright Notice, quoted: *"All the materials and the documents published on the
Website... are fully and exclusively owned by the European Union (EU) (© EU 2011-2026), or by
third parties as indicated on the Website. Unless otherwise stated, downloading, reproduction and
use of all the materials and documents published on the Website are authorised provided the source
is acknowledged as follows: © EU 2011-2026."* The metadata page itself carries no third-party
attribution of its own (checked directly) — EUSPA/EU's own first-party operational data, not
licensed-in third-party content. **Result: general redistribution with attribution is authorised —
clean, unlike `RS14`'s own genuinely unclear case, no ruling needed to use it.** Reported before
building anyway, per instruction, not because the result turned out ambiguous.

### 2.4 `SATMOD`: the rule-4/licence search, and the `RS14` ruling extended a second time

Performed 2026-09-24, before any Sentinel-6/Jason code existed this round. Cerri et al. 2010 (Marine
Geodesy 33(sup1):379-418), the paper `SATMOD` itself cites as its own ref.[6], is paywalled everywhere
this session reached: Tandfonline HTTP 403, ScienceDirect (a related 2025 paper) HTTP 403, ResearchGate
HTTP 403, Academia.edu HTTP 403 — no open preprint or institutional-repository deposit found. `SATMOD`
itself is the actual working route to the same values: a CNES technical note written by Cerri himself
(DCT/SB/OR) with A. Couhert and P. Ferrage, freely retrievable (`ids-doris.org`, HTTP 200, no login),
printing every box-wing value directly. States its own "External diffusion: web site of the
International DORIS Service" — that site's own site-wide Legal Notice page
(`ids-doris.org/legal-notice.html`) is an unfilled placeholder, verbatim "Legal notice To be added...
Last Updated: 29 June 2022," checked directly, not treated as a clean licence merely because the note
names external diffusion.

**Ruled under the SAME `RS14` reasoning (§2.2), extended a second time** (first to GLONASS/GLONASS-M/
GLONASS-K, `../plan/subplan_L5/L5-3.md`; now to `SATMOD`): a published-values note, cited per value,
edition PINNED BY HASH since the note is explicitly revised over time (its own revision-history table,
most recently touched 2026 per its own header) — the SAME "used anyway, on the record" treatment `RS14`
itself received, not a fresh, separately-argued ruling. A SESSION-INTERNAL finding, recorded here for
completeness rather than hidden: an EARLIER pass this round cached a summary of `SATMOD`'s own Jason-2/
Jason-3 table from what turned out to be a STALE source (this session's own earlier notes, likely built
from the older archived `SatelliteModels_Ed1Rev10.pdf`, 2016, also fetched this round for comparison) —
caught before any code was written, by re-reading the CURRENT, hash-pinned fetch directly rather than
trusting the cached summary (`PROVENANCE.md`'s own L5 step 4 section carries the full before/after
numbers).

---

## 3. Definitions and conventions

- **The body frame is the IGS convention, +x towards the Sun, +z nadir (toward Earth), for every
  GPS block** (`MSGA15` Fig. 3/4; ruled at L4 step 6 from `MSGA15` and CODE's own G05 attitude —
  `SPEC-thrust-yaw.md`, and what `odl::attitude` emits in every regime: `nominal_yaw_steering`'s
  own `z_body = -r_hat`, `modules/attitude/src/attitude.cpp`, its own comment labelling this
  "nadir"). `RS14` §5.1.1 defines its own XYZ frame identically in substance, independently
  stated, not copied from `MSGA15`: "X normal to the surface of the satellite which is always
  illuminated by the Sun" (+x, Sun-facing), "Z opposite to the radial direction" (+z; **corrected
  2026-09-24**, L5 step 2 — this is NADIR, `-r_hat`, not "anti-nadir" as an earlier version of
  this line labelled it: "the radial direction" is the outward `+r_hat`, so "opposite" it is
  `-r_hat`, matching `RS14`'s own words to the code's own `z_body = -r_hat` exactly; the earlier
  parenthetical gloss was a labelling error caught while pinning Galileo's own frame against GSC's
  own real coordinate pairs, `SPEC-spacecraft.md` §2 GALSC, not a convention this tree ever
  actually used backwards — the schema's own `body_fixed_normal` for each face is stated in this
  same frame, §4). The two sources' own frame conventions agree; this spec's own values are stated
  in it directly, no rotation applied.
- **`RS14`'s own α/δ/ρ notation, resolved by FORMULA, not by prose — and RS14 is internally
  inconsistent about it.** `RS14` §5.1.2's own Appendix prose, citing Milani et al. (1987) —
  `SPEC-macromodel`'s own `MSPB87`, already an informative source there — states: "α absorption
  coefficient... **δ reflection coefficient**, fraction of reflected photons... **ρ diffusion
  coefficient**, fraction of diffusely scattered photons," with α + δ + ρ = 1. A first pass at this
  spec (2026-09-24) trusted that sentence alone and mapped δ→`specular`, ρ→`diffuse` accordingly.
  **This was wrong, caught by the manager's own instruction to verify by the FORCE EQUATION, not
  the words.** `RHS12`'s own Eq. 6, reprinted in full inside `RS14` as its own "P-II" chapter
  (pp. 85–101, `PROVENANCE.md` §26.2) — the actual force law both papers use, not a paraphrase of
  it — states: `f = -(A·S₀/Mc)[cosθ(1−ρ)ê_D + 2(δ/3 + ρ·cosθ)ê_N]`. **ρ carries the "2·ρ·cosθ" term
  — the mirror-like, specular reflection — and δ carries the "2·δ/3" term — the Lambertian, diffuse
  term.** This is the OPPOSITE of §5.1.2's own prose, and matches `RHS12`'s own separately-stated
  prose a few pages earlier in the same chapter and `RS14` §4.2's own GLONASS cylindrical-surface
  formula (Fliegel et al. 1992's own model, independently restating the same ρ=specular/δ=diffuse
  pairing) — `RS14` §5.1.2 disagrees with its OWN reprinted primary source and its OWN §4.2, an
  authorial inconsistency internal to `RS14`, not a deliberate alternate convention. Cross-confirmed
  by `RS14`'s own partial-derivative section (Eq. 10: for a solar panel at cosθ=1,
  `∂f/∂(1+ρ+2δ/3) = -(A_SP·S₀/Mc)ê_D`), matching this tree's own already-derived flat-plate
  coefficient `1+ρ+2δ/3` (`PROVENANCE.md` §26.2) term-for-term only under ρ=specular, δ=diffuse.
  Physically cross-checked too: GPS solar panels are glass-covered and predominantly SPECULAR
  reflectors, and every block's own panel row in `RS14` (§4 below) has its "ρ" column an order of
  magnitude larger than its "δ" column — consistent only with ρ=specular (`SPCR-A-003`,
  `SPCR-A-004` each check this against a live surface). Every value in §4 below maps `RS14`'s own
  **ρ → this schema's `specular`**, and `RS14`'s own **δ → this schema's `diffuse`** — the
  corrected mapping, stated here so the direction is checked against the formula and cell-by-cell
  numbers, not assumed from either source's own prose (`SPCR-A-001` checks the sum
  α+specular+diffuse = 1 at every surface, which either mapping direction satisfies equally and so
  does not by itself distinguish them — the mapping itself is checked by matching this spec's own
  numbers to `RS14`'s own printed table cell by cell, `SPCR-A-003`/`SPCR-A-004`, plus the physical
  specular-panel cross-check, neither of which the sum-to-1 identity alone would catch).
  **`SPEC-macromodel.md`'s own convention was checked and found already correct** (line ~120: "α,
  ρ, δ — absorbed, specularly reflected, diffusely scattered fractions") — this was a transcription
  error made populating THIS spec's own tables from `RS14`, not a wrong convention inherited from
  L4's own schema; no L4 spec needed correction.
- **Mass is per-block, not per-SVN**, in this version, for every block — `RS14`'s own Block I
  table already varies by SVN (§4), II/IIA/IIR/IIR-M/IIF do not, each one mass. Where that one
  mass comes from now differs by block, corrected this round:
- **Mass source, corrected 2026-09-24: `IGSMETA` primary for IIR/IIR-M/IIF; `RS14` primary for
  I/II/IIA — the two are not interchangeable, and neither is right for every block.** A first pass
  used `RS14` for every block's own mass, with `IGSMETA`'s own figure recorded only as an aggregate
  cross-check for IIF, explained by a HYPOTHESIS — launch mass (`IGSMETA`'s own field) exceeding
  on-orbit dry mass (`RS14`'s own figure) — that was never checked against what the field is
  actually documented to be. **The manager asked for that check.** `IGSMETA`'s own SINEX header
  states plainly: `SATELLITE/MASS` is "In-orbit satellite mass." `SMSD24`, the format's own
  description document (fetched directly, DOI `10.57677/metadata-sinex`), states why: "Knowledge of
  the mass of a GNSS satellite is required to compute the acceleration caused by non-gravitational
  forces (such as solar radiation pressure...). In line with the quality of other model parameters,
  a 1% accuracy is typically deemed adequate for this purpose" (§1.1) — this field exists FOR this
  library's own purpose, not launch mass. **The launch-vs-on-orbit hypothesis is WITHDRAWN**, not
  merely dropped — it was checked and found false. The ~5% (IIF) / ~1.8% (IIR, IIR-M) gap between
  `RS14`'s own figure and `IGSMETA`'s own is recorded as genuinely unexplained, not smoothed into a
  tidier story than the evidence supports.

  This does NOT mean `IGSMETA` is simply "the better source" everywhere: `SMSD24` §4.3 states
  explicitly that Block I/II/IIA's own individually-varying masses (`FLGA92`'s own per-satellite
  table) are **"currently not considered in the `SATELLITE/MASS` block"** — for these three blocks
  `IGSMETA`'s own figures are flat per-block defaults (e.g. every Block II SVN alike at 843 kg),
  LESS specific than `RS14`'s own `FLGA92`-derived, genuinely per-SVN figures (450/520 kg for Block
  I alone, §4 `SPCR-R-001`). So for I/II/IIA, `RS14` stays primary — unchanged. For IIR, IIR-M and
  IIF, `IGSMETA` carries a real per-satellite entry sourced independently of `RS14`
  (`SATELLITE/MASS`'s own reference codes: `[MA03]` Hegarty 2017 for IIR/IIR-M, `[MA04]` a Boeing
  technical-specifications page for IIF — neither is `RS14`), so it becomes primary there and
  `RS14`'s own figure the cross-check, inverted from the first pass.

  Two specific satellites this tree already treats as its own canonical reference for these blocks
  (`PROVENANCE.md` §30.1: G01 = SVN63 = Block IIF, G05 = SVN50 = Block IIR-M) give the actual
  figures used: **SVN63, 1633 kg** (`gps_block_iif`, `RS14`'s own 1555 kg now the cross-check);
  **SVN50, 1080 kg** (`gps_block_iir`/`_iir_m` alike, `RS14`'s own 1100 kg now the cross-check —
  `SMSD24`'s own Table 5 gives the SAME 1080 kg as a general IIR/IIR-M figure, independently
  matching SVN50's own individual row exactly). Since neither `gps_block_iif()` nor
  `gps_block_iir()`/`_iir_m()` takes an SVN parameter (their own dimensions and optics are already
  block-generic, not per-satellite, `RS14`'s own tables not distinguishing individual SVNs either),
  and SVN63/SVN50 are already this tree's own sole reference satellite for each block everywhere
  else it matters, no signature change is needed for L7 to receive their own specific mass: calling
  the existing function IS calling it for the reference satellite. A genuine per-SVN mass table,
  for satellites OTHER than these two references, remains unbuilt — named in §10 as before, now
  narrower in scope.
- **Galileo's own body frame is NOT this tree's own convention, and the mapping is TESTED, not
  trusted from either source's own prose** (the manager's own instruction, and the IIR 180°
  precedent, `PROVENANCE.md` §30.19/30.20 — a case where prose alone, even the primary source's
  own, hid a real sign defect). `GALSC` §2 states plainly: both IOV and FOC have **+Z nadir**
  (toward the L-band antenna, matching this tree's own +Z exactly, corrected above) but **+X toward
  DEEP SPACE** — the opposite of this tree's own +X (toward the Sun) — its own words: "this does
  not meet the GPS block II/IIA attitude convention." The mapping (180° about Z: `(x,y,z) ->
  (-x,-y,z)`, its own inverse) is VERIFIED against `GALSC`'s own printed numbers, not derived from
  its prose: its own antenna-reference-point, phase-centre and laser-retroreflector tables print
  the SAME physical point in BOTH "Mechanical RF" (its own native frame) and "ANTEX RF" (this
  tree's own convention, CoM-relative) columns, for both IOV and FOC — three such pairs, checked
  directly, `SPCR-A-009`. `galileo_frame_from_mechanical()` (`modules/spacecraft`) is this mapping;
  every Galileo face normal and centre of mass in §4/§6 is built through it.
- **Mass and centre of mass are dated, per satellite, in `GALSC`'s own table — the schema holds
  ONE value — so the library returns a macromodel for a satellite AT AN EPOCH, ruled 2026-09-24**
  (`../plan/subplan_L5/L5-2.md`): surfaces and optics come from the satellite's own BLOCK (IOV or
  FOC, §6, identical across every satellite of that block); mass and centre of mass come from
  `GALSC`'s own table entry valid at that epoch, refusing an epoch the table does not cover —
  the SAME shape `odl::atmosphere::SpaceWeatherTable::sample` already uses for a day outside its
  own space-weather coverage (`space_weather.hpp`'s own header comment). `GALSC`'s own entries are
  monthly-precision at best ("as of April 2024", no day or time stated) — modelled with a local
  `YearMonth`, not the tree's own full `odl::time::Epoch` (which would overstate the source's own
  precision, the same reasoning `odl::atmosphere::Day` already applies to NRLMSISE-00's own daily
  input), valid from that stated month with no stated end (`GALSC`'s own most recent update, valid
  until superseded). **This same shape will serve GPS's own per-satellite masses later
  (`SPCR-Q-002`) — not retrofitted now, only noted that it fits.**
- **`GALSC`'s own α/ρ/δ notation is quoted directly, and checked by arithmetic, not merely
  trusted** — `RS14` showed a source's own letters can be backwards relative to its own formula
  (§3 above). `GALSC` §6's own intro states: *"α ≡ absorption coefficient, ρ ≡ specular reflection
  coefficient, δ ≡ diffuse reflection coefficient"* — matching this schema's own
  absorptivity/specular/diffuse order EXACTLY, no swap needed. Checked anyway: every material row
  `GALSC` prints, for both IOV and FOC, has its own three coefficients summing to exactly 1 —
  verified cell by cell for every row (`SPCR-A-010`), the same independent arithmetic check
  `SPCR-A-001` already applies to every GPS surface, not a substitute for reading the quoted
  definition but a second, independent confirmation of it.
- **A genuine schema-fidelity limitation, found building GLONASS and GLONASS-M, REPORTED rather
  than silently built around (`SPCR-Q-007`).** `RS14` §4.2 states plainly: "the bus of GLONASS and
  GLONASS-M satellites have a characteristic cylindrical shape. Therefore for these satellites
  actually cylinder-wing models were constructed... The only surfaces of the satellite that are
  affected by this change are the ±Y and ±X surfaces" — its own Tables 5.6/5.7 print a "shape"
  column for exactly those four faces, and state its own meaning directly: "the ratio of the sum
  of cylindrical areas w.r.t. sum of flat areas is given in the 'shape' column, where 0 indicates
  flat and 1 indicates cylindrical" (0.620/0.494 for GLONASS, 0.728/0.550 for GLONASS-M). `RS14`'s
  own Eq. 4.5 gives a DISTINCT force formula for the blended fraction — a weighted sum of the
  standard flat law (Eq. 9 of `RS14`'s own P-II, what this schema's `FlatSurface` already
  implements) and a genuinely different cylindrical-surface law (Eq. 4.4, Fliegel et al. 1992) —
  which this schema cannot represent (no cylinder surface type exists, `SPEC-macromodel.md`'s own
  stated scope, the SAME gap BeiDou's own curved surfaces hit, narrower here: two face PAIRS of
  six, not a whole satellite, and `RS14` itself gives the flat-law, shape=0 case as one
  well-defined endpoint of its own formula, unlike BeiDou's cylinders/rings/parabolic surfaces,
  which have no flat-law fallback in the CSNO standard at all). **Built here under RS14's own
  flat-law (shape=0) special case ONLY**, using `RS14`'s own printed alpha/delta/rho for those four
  faces as given — UNDERSTATING the true cylindrical contribution — a judgment call made to keep
  GLONASS and GLONASS-M buildable (the manager's own ruling was "BUILD IT," and the majority of
  each satellite — mass, ±Z bus, solar panels, and the WHOLE of GLONASS-K, whose own Table 5.8
  prints no "shape" column at all — is unaffected), stated EXPLICITLY and VISIBLY in each affected
  value's own citation string (`SPCR-A-016` checks the citation differs from an uncaveated one),
  not silently normalized away. GLONASS-K needs no such caveat: its own dimensions come from a
  DIFFERENT source (`RS14`'s own "Mitrikas, personal communication, 2011," §4 below) with no
  "shape" column printed at all.
- **The flat-law approximation's own error, BOUNDED with a number — RULED (manager, 2026-09-24
  review): keep the flat law, quantify it.** `RS14`'s own Eq. 4.5, read from the rendered PDF page
  (not the first pass's own OCR extraction, which failed its own internal consistency check against
  Eq. 4.4/Eq. 9's own limiting cases and was not trusted): `f = -(A·S0/Mc)·cosθ·[(α+δ)·(e_D +
  (π/6·s + 2/3·(1-s))·e_N) + (4/3·s + 2(1-s))·ρ·cosθ·e_N]` — this formula IS "Eq. 9 of P-II" at
  `s=0` and Eq. 4.4 at `s=1`, by RS14's own construction ("the total acceleration... as the sum of
  the respective accelerations for flat... and cylindrical surfaces... weighted with the 'shape'
  factor"), so the flat-law (built) and true-blend (RS14's own intent) forces are computed
  SELF-CONSISTENTLY from this one formula, evaluated at `s=0` and at RS14's own printed `s`, rather
  than cross-checked against a second, separately-transcribed formula. At the manager's own stated
  geometry (Sun along the +X face normal, and at 45° toward +X/+Z), summing all six bus faces plus
  the panel: the difference between the true-blend and flat-law TOTAL SRP force is **0.57–0.90% of
  the whole-satellite force for GLONASS, 0.70–1.10% for GLONASS-M** (the larger figure at each block
  is the 0°, direct-illumination case; the smaller is the 45° case) — small, and a genuine bound,
  not an order-of-magnitude guess. Computed in a standalone script (not committed — a one-off
  numeric check, the same status GLNY's own convergence sweep and BeiDou's own force-magnitude
  comparisons have), self-checked by confirming the whole-satellite difference equals the affected-
  faces-only difference exactly (the unaffected ±Z faces and panel are identical either way, by
  construction). **Curved surfaces remain ONE carried, ungated question together with BeiDou's own**
  (the manager's own ruling): the photon-pressure kernel (`modules/photon_pressure`) is itself gated
  for flat (and spherical) panels only, so a cylinder surface type is not added speculatively for
  GLONASS's own sake — decided when a consumer needs it, the same "no consumer" reasoning GPS-IIIA's
  own refusal and BeiDou's own carry already state.
- **QZSS's own body frame is NOT this tree's own convention — the mapping is HYPOTHESISED from the
  source's own stated property, then CONFIRMED against real attitude data (the manager's own
  instruction), not merely trusted from prose.** `SPI_QZS1_B` §2 prints no paired native/tree
  coordinate examples the way `GALSC` does, so the frame mapping cannot be checked the same way
  Galileo's was. Instead, `SPEC-qzss-attitude.md` §3 derives the SAME 180-about-Z relation from two
  independent readings of the source (the yaw-steering mode's own stated Sun hemisphere, and its own
  y-axis definition) and confirms it with `tools/orbex_qzss_check.cpp`: four real-data checks, two
  satellites, matched to 0.00003-0.00019 deg -- the tightest real-data agreement any frame mapping in
  this tree has had. `qzss_frame_from_native()` (`modules/spacecraft`) is this mapping; every QZS-1
  face normal and centre of mass in §4/§6 below is built through it.
- **A second genuine schema-fidelity limitation, found the same day as GLONASS's own, REPORTED not
  built around.** `SPI_QZS1_B` §6 Table 4 gives the +Z face's own "L-ANT Cover" material NO area at
  all -- footnoted "(*1)" in place of a number -- and states its own shape directly: "L-ANT shape is
  approximately depicted as a truncated circular cone comprised of 1.5m and 1.8m diameter circles
  0.8m apart." Even if an area were printed, this schema cannot hold a cone (no such surface type,
  `SPEC-macromodel.md`'s own stated scope, the same gap GLONASS's own cylinder-wing faces and
  BeiDou's own curved surfaces hit) -- but here there is not even a number to build an approximation
  from, so the material is OMITTED entirely, not approximated under any special case. The document
  itself names a more detailed alternative (a "box-wing-hat model," Ikari et al. 2014, Ref. [3]) for
  exactly this shape -- not pursued, since the schema could not hold its own output either.
- **BeiDou is carried, not built -- `BD 420025-2019` (CSNO 2019) reads as a FILE-FORMAT
  specification, not a populated per-satellite data product, a finding that REFINES an earlier,
  narrower reading rather than merely repeating it.** Sec.5.2's own NORMATIVE text enumerates the
  "basic parameters" as mass, satellite type and laser-reflector position only -- no centroid named.
  Appendix A (explicitly labelled "(Informative)") does print a "Centroid coordinates" column in its
  own Table A.1, alongside mass, for two illustrative satellites (C01, C02) -- an apparent
  inconsistency between the standard's own normative body and its own informative appendix, the same
  CLASS of internal disagreement RS14's own alpha/delta/rho notation had (SPEC-spacecraft.md's own
  earlier finding), reported rather than silently resolved either way. It does not matter which
  reading is "correct," though: every cell in EVERY Appendix A table (mass, centroid, laser-
  reflector, surface area, optics alike, Tables A.1 through A.4) holds only a checkmark meaning "this
  field belongs in the format," never an actual number -- so no real per-satellite value, of ANY
  field the format nominatively supports, is printed anywhere in this 16-page document for a real
  BeiDou satellite. Combined with Sec.5.3's own stated curved surface types (planes, cylinders,
  rings, parabolic) this schema cannot hold, one of the source's own three attitude modes (maneuver
  yaw, Sec.5.4.4) extracted with its own equations OCR-garbled, and no redistribution terms found
  anywhere -- `beidou()` refuses unconditionally, `SPCR-R-019`, every reason named in its own
  refusal message.
- **Sentinel-6 is the first constellation in this tree to populate `BandedOptics`'s own infrared
  field.** `MCRM-R-004`'s own optional per-surface infrared `OpticalTriple` was built at L4 step 2 but
  left unused by GPS, Galileo, GLONASS and QZSS alike (every one of those sources prints visible-band
  optics only) — `SATMOD` §16.3 prints BOTH visible and infrared columns for every one of Sentinel-6's
  own twelve rows, and Jason-2's/Jason-3's own §7.3/§12.3 tables do the same for their own eight rows
  — the natural first real consumer of a capability this tree already had.
- **Two of Sentinel-6's own twelve printed face normals do not renormalise to unit length — checked
  directly against the rendered PDF page, not an extraction artefact, and RENORMALISED before
  construction (`body_direction`'s own `MCRM-F-002` requires exact unit length).** `SATMOD` §16.3's
  own row 5/6/7/8 normal, `(0, 0.616, -0.788)`/`(0, 0.616, 0.788)`, has printed norm 1.0002 — a small,
  plausibly rounding-level deviation. Row 11's own normal, `(0.469, 0, -0.833)`, has printed norm
  0.9560 — a MATERIAL, 4.4% deviation, confirmed against a directly rendered image of the source's own
  page 39 (not merely the flattened-text extraction), the same "render the page, do not trust OCR/
  extraction alone" discipline `RS14`'s own Eq. 4.5 finding already established for a different
  source. Every non-axis-aligned normal is built from its own printed components, renormalised, cited
  as the source's own value with this adjustment stated (`sentinel6.cpp`'s own citation string names
  which rows were renormalised).
- **Jason-2's and Jason-3's own macromodel is the SAME table — checked cell by cell, not merely
  quoted** (`SATMOD` §7.3: "the macro-model is the same as for Jason-3"; §12.1: "the a priori SRP
  geometry and properties are identical for the two satellites") — built from each satellite's own
  section independently (`jason2()`/`jason3()` each carry their own citation naming their own section
  number), so the two functions' own values agreeing is a CHECKED fact (`SPCR-A-025`), not a silent
  call-through the way `gps_block_iir_m()` explicitly inherits `gps_block_iir()`'s own geometry.
- **Jason's own solar array is BODY-FIXED, not Sun-tracking — a genuine difference from every panel
  this tree has built before this round.** `SATMOD` §7.3/§12.3 print a FIXED normal, `(+1,0,0)`/
  `(-1,0,0)`, "in sat ref frame" — the SAME frame every bus face is stated in — for the solar-array
  rows, unlike GPS's, Galileo's and QZSS's own panels, which this tree already builds
  `flat_surface_sun_pointing` because their own sources describe active Sun tracking. Built here as
  TWO ADDITIONAL body-fixed `FlatSurface`s, exactly as the source states, not assumed to track the Sun
  merely because it is called "solar array" (`SPCR-Q`, this spec's own open question names the gap
  this leaves: the array's own real rotation, if any beyond this static reference orientation, is not
  part of this macromodel table).
- **Jason-1 is carried, refused — its own optics are non-energy-conserving and carry an overall force-
  scale factor, neither of which this schema holds.** `SATMOD` §6.3 states directly: its own model
  "was slightly modified by tuning the optical coefficients of the +/-Y faces and of the +X faces and
  by setting a scale factor equal to 0.97... meant to be a factor that multiplies the solar radiation
  pressure force." Checked directly, not assumed from the word "tuning" alone: every one of the six
  body-face rows sums to something other than 1 (e.g. +X: 0.0938+0.2811+0.2078=0.5827; +Y:
  1.1880-0.0113-0.0113=1.1654, with NEGATIVE diffuse/absorptivity cells the schema's own physical
  triple cannot hold either way). A satellite-wide force-scale factor, outside any single surface's
  own optics, has no field in this schema at all (`SPEC-macromodel.md`'s own stated scope). No
  consumer needs Jason-1 currently.
- **Mass and centre of mass, for Sentinel-6 and Jason-2/-3 alike, are each source section's own
  BASELINE value, not an epoch lookup — a deliberate choice, differently justified for each.** Every
  one of the three satellites' own operational offset files (`s6amass.txt`, `ja2mass.txt`,
  `ja3mass.txt`) is confirmed openly retrievable (fetched directly this round). Sentinel-6's own file
  is small (~30 data rows, `SATMOD` §16.1) — comparable in scale to Galileo's own ~30-row dated table
  — but the ruling for THIS round names no epoch requirement for Sentinel-6's mass/CoM specifically
  (unlike Jason's own explicit "epoch lookup... or otherwise the baseline" instruction), so it follows
  the existing single-baseline pattern GPS/GLONASS/QZSS already use, the gap named as an open question
  rather than built speculatively. Jason-2's/Jason-3's own files are NOT "Galileo's shape": thousands
  of rows each (4134/3917 lines), an ever-growing PER-MANEUVER OPERATIONAL LOG spanning each
  satellite's entire multi-year history, not a small, stable, dated snapshot table — embedding this
  literally would be both impractical at this layer's own established scale (no existing table in this
  tree exceeds ~30 rows) and a poor engineering proxy for what is more honestly a NOT-YET-BUILT
  ancillary-file-ingestion capability. Both satellites use the manager's own explicitly offered
  fallback (`JasonMassSource::Baseline`, REQUIRED, no default, an explicitly named selector per the
  manager's own instruction) instead.

---

## 4. Required behaviour

- **SPCR-R-001.** `gps_block_i(svn) -> Result<Macromodel, SpacecraftError>`, for `svn` naming one
  of the Block I satellites `RS14` Table 5.2 distinguishes by mass (450 kg: SVN 03, 04, 06; 520 kg:
  SVN 08, 09, 10, 11) — refuses (`SPCR-F-001`) for any other SVN. Six body-fixed `FlatSurface`s
  (±X, ±Y, ±Z bus faces) plus one `flat_surface_sun_pointing` (solar panels), each area and optical
  triple exactly `RS14` Table 5.2's own row, cited `"RS14 Table 5.2, averaged from FLGA92"` per
  value, mapped per §3's own δ/ρ rule.
- **SPCR-R-002.** `gps_block_ii_iia(is_iia) -> Result<Macromodel, SpacecraftError>` — `RS14`
  Table 5.3's own six bus faces and solar panel, identical surface geometry for II and IIA
  (`RS14`'s own single table), `is_iia` selecting only the mass (880 kg / 975 kg respectively).
  Cited `"RS14 Table 5.3, averaged from FLGA92"` per value.
- **SPCR-R-003.** `gps_block_iir() -> Result<Macromodel, SpacecraftError>` — `RS14` Table 5.4's
  own six bus faces and solar panel, cited `"RS14 Table 5.4, averaged from FLGA96"` per value. Mass
  **1080 kg, `IGSMETA`'s own `SATELLITE/MASS` for SVN50** (§3's own mass-source correction) — `RS14`
  Table 5.4's own 1100 kg is the cross-check, ~1.8% higher, recorded in the same citation.
- **SPCR-R-004.** `gps_block_iif() -> Result<Macromodel, SpacecraftError>` **ruled and built**
  (manager, 2026-09-24, `../plan/subplan_L5/L5-1.md`): `RS14` Table 5.5 itself publishes six
  per-surface bus rows plus a solar-panel row for Block IIF, area and optical triple exactly as
  printed, mapped per §3's own corrected δ/ρ rule — the earlier `SPCR-F-002` unconditional refusal
  (§2's search having found no OTHER, cleaner source) is withdrawn now that `RS14` Table 5.5's own
  in-table values are used directly, with two DIFFERENT citations per value, since the table's own
  two halves have different provenance:
  - **dimensions** (area, each face's own extent): cited to `"RS14 Table 5.5, which itself states
    its own dimensions come from \"an unpublished document\""` — `RS14`'s own text (§5.5) names no
    further source; this is the citation chain's own end, recorded rather than resolved further.
  - **optical properties** (α, the mapped specular/diffuse pair): cited to `"RS14 Table 5.5's own
    generic assumption, same as Ziebart (2001) §7.1"`, marked **ASSUMED**, not an IIF-specific
    measurement — `RS14` states this itself (its own Table 5.5 footnote), so the spec states it too
    rather than presenting an assumed value as measured.

  Mass **1633 kg, `IGSMETA`'s own `SATELLITE/MASS` for SVN63** (§3's own mass-source correction,
  reversing this round's own earlier draft) — `RS14` Table 5.5's own 1555 kg (its own caption) is
  now the cross-check, ~5% lower; a second, independent figure, a U.S. Space Force fact sheet's own
  3439 lb = 1559.7 kg (agrees with `RS14` to 0.3%, found before this round), sits alongside it,
  agreeing with `RS14`'s own reading rather than `IGSMETA`'s — recorded as a genuine, unresolved
  three-way spread (1555 / 1559.7 / 1633 kg), not smoothed into one preferred number by any of the
  three sources being quietly dropped. The launch-vs-on-orbit explanation this spec's own earlier
  draft gave for the `RS14`/`IGSMETA` gap is **WITHDRAWN**: `SMSD24` (§3, §2) states the
  `SATELLITE/MASS` field is in-orbit mass, for non-gravitational force modelling, not launch mass —
  checked, not merely re-asserted, and found to rule the withdrawn explanation out. A second
  aggregate check — a published panel-span figure, independently verified — was sought and **not
  completed**: no
  directly-fetched, citable source was found (a 43.1 ft / 13.11 m figure appeared only in a search
  engine's own synthesized summary, never independently confirmed at a primary source, and is not
  used; a specific lead, "USA-66," was found to be a different, unrelated satellite — USA-NNN and
  NAVSTAR-NNN are separate numbering systems — and discarded before use, not carried into any
  citation). Recorded here as an incomplete check, not silently dropped or filled with an unverified
  number.
- **SPCR-R-005.** `gps_block_iir_m() -> Result<Macromodel, SpacecraftError>` returns
  **`gps_block_iir()`'s own geometry** — basis: `MSGA15` states IIR-A/IIR-B/IIR-M are distinguished
  specifically by "different phase center locations" (the antenna, `MSGA15` Table 3's own separate
  IIR-A / IIR-B-M rows), grouping all three under one body-frame figure (`MSGA15` Fig. 4's own
  caption, "IIR/IIR-M") — an inference that the BUS/PANEL geometry this schema holds is shared,
  stated as an inference from what IS enumerated as different, not a direct "IIR-M's bus equals
  IIR's" sentence in the source. Every geometry value this function returns carries a citation
  stating exactly this — `"= gps_block_iir(), per MSGA15's own IIR/IIR-M grouping and its own
  phase-center-only distinction; not independently measured for IIR-M"` — so a reader sees the
  inference, not a bare number. **Mass 1080 kg**, as of this round **not** inherited from
  `gps_block_iir()`'s own citation string but independently sourced to `IGSMETA`'s own
  `SATELLITE/MASS` row for **SVN50** — this tree's own canonical IIR-M reference satellite (§3,
  `PROVENANCE.md` §30.1) — which happens to equal `gps_block_iir()`'s own (now also `IGSMETA`-based,
  SVN50-sourced) figure exactly, so the two blocks' own masses agree by shared sourcing, not by one
  inheriting the other's assumption the way the pre-2026-09-24 version did.
- **SPCR-R-006.** Every numeric value in `SPCR-R-001` through `-R-003`, every value `SPCR-R-004`
  states for IIF, and every value `SPCR-R-005` states as inherited, is a `Cited<double>` or
  `Cited<Vec3>` (`SPEC-macromodel`'s own `MCRM-R-004`) — **this spec adds no exemption to that rule
  and creates none**: the schema's own existing refusal (`MCRM-F-001`, an empty or blank citation
  refused) is what this layer's own exit gate rests on, unchanged.
- **SPCR-R-007.** `gps_block_iiia() -> Result<Macromodel, SpacecraftError>` **refuses**
  (`SPCR-F-003`) unconditionally in this version: one search performed 2026-09-24 (§2) found no
  citable published per-surface dimension or optical source for Block IIIA. Its one plausible lead
  — a ScienceDirect paper on a GPS III box-wing model — returned HTTP 403 to an automated fetch, the
  same publisher-blocking pattern this tree has hit repeatedly elsewhere; no per-surface table was
  independently confirmed, so none is used. **No baseline consumes `gps_block_iiia()` in this
  version** — the refusal blocks nothing currently critical, recorded for completeness at L5 step
  1's own close, not because IIIA is on this version's own critical path.
- **SPCR-R-008.** `galileo_frame_from_mechanical(mechanical: Vec3) -> Vec3` rotates a unit or
  offset vector from `GALSC`'s own "Mechanical RF" to this tree's own body-frame convention: 180°
  about Z, `(x,y,z) -> (-x,-y,z)`. Its own inverse (an involution). VERIFIED against three real
  coordinate pairs `GALSC` prints itself (§3), `SPCR-A-009`.
- **SPCR-R-009.** `galileo_iov(gsat: int, epoch: YearMonth, life: OpticalLife) -> Result<Macromodel, SpacecraftError>`
  — `gsat` one of `GALSC`'s own three currently-listed IOV satellites (101, 102, 103), else refuses
  `SPCR-F-004`; `epoch` at or after that satellite's own mass/CoM entry's stated month, else
  refuses `SPCR-F-005` (§3). Six body-fixed `FlatSurface`s (one per face per material — some faces
  carry two, `GALSC` §6.1) plus one `flat_surface_sun_pointing` (both wings' own area summed per
  material, §3), mapped through `SPCR-R-008`, cited to `GALSC` §6.1 per material. Mass and centre
  of mass cited to `GALSC` §4.1's own dated entry for `gsat`. **`life` is REQUIRED, no default**
  (**ruled 2026-09-24**, resolving `SPCR-Q-004`): `GALSC`'s own Material 2 rows (present on
  +X/+Y/-Y/+Z only) print SEPARATE Beginning-Of-Life and End-Of-Life coefficients, and every IOV
  satellite is long past early life as of 2026 (all three launched 2011–2012), so a silently
  defaulted BOL would be the wrong answer for present use — plan §5 constraint 10 (`SPEC-macromodel`
  `MCRM-Q-001`'s own reasoning: a value's own meaning belongs in its type, not an unstated
  convention). Material 1 (every face) is printed "BOL & EOL" as one unchanging set, unaffected by
  `life` either way — `SPCR-A-014` checks both: BOL and EOL genuinely differ where `GALSC` prints
  different coefficients, and agree exactly where it prints the same ones.
- **SPCR-R-010.** `galileo_foc(gsat: int, epoch: YearMonth) -> Result<Macromodel, SpacecraftError>`
  — `gsat` one of `GALSC`'s own 26 currently-listed FOC satellites, else refuses `SPCR-F-004`;
  `epoch` on the same terms as `SPCR-R-009`, else refuses `SPCR-F-005`. Surfaces mapped and cited
  the same way, from `GALSC` §6.2's own table. **No `life` parameter, deliberately**: `GALSC`'s own
  FOC table prints exactly ONE set of coefficients per material, with NO "BOL"/"EOL" column header
  or caption of any kind — checked directly (the manager's own instruction, "say... in the
  source's own words"), not assumed. `GALSC`'s own §6 intro defines both terms ("Note EOL means
  'End Of Life' and BOL means 'Beginning Of Life'") in the context of IOV's own separate columns,
  but its own FOC table (§6.2) carries neither label — the source itself does not state which life
  stage its own single FOC set represents, so this spec does not guess: FOC's own optics are
  reported as `GALSC` prints them, unqualified, not asserted to be BOL, EOL, or an average of the
  two. `GALSC`'s own +Z panel total (1.053 + 1.969 = 3.022 m²) disagrees with its own summary-table
  figure (3.036 m²) by 0.46% — built from the detailed, itemised table (this spec's own established
  preference for the finer-grained source, `RS14`'s own precedent), the gap recorded rather than
  silently resolved either way.
- **SPCR-R-011.** Every numeric value `SPCR-R-008`–`SPCR-R-010` state is a `Cited<double>` or
  `Cited<Vec3>` (`SPEC-macromodel`'s own `MCRM-R-004`), the same rule `SPCR-R-006` states for GPS —
  this spec adds no exemption for Galileo either.
- **SPCR-R-012.** `glonass() -> Result<Macromodel, SpacecraftError>` — `RS14` Table 5.6's own six
  bus faces and solar panel (mass 1415 kg, `RS14`'s own caption), dimensions cited to Revnivykh and
  Mitrikas (1998), optics marked ASSUMED (`RS14`'s own generic Ziebart (2001) §7.1 fallback, the
  SAME status GPS-IIF's own optics carry). The ±X/±Y bus faces carry the additional shape-blend
  caveat §3 states, built under the flat-law special case; ±Z and the panels do not. Always
  succeeds — `RS14` gives one table for this block, no per-satellite or per-epoch parameter.
- **SPCR-R-013.** `glonass_m() -> Result<Macromodel, SpacecraftError>` — `RS14` Table 5.7, the same
  shape as `SPCR-R-012`, dimensions cited to Mitrikas (2005), mass 1415 kg.
- **SPCR-R-014.** `glonass_k() -> Result<Macromodel, SpacecraftError>` — `RS14` Table 5.8's own six
  ORDINARY flat bus faces (no "shape" column at all, §3) and solar panel, mass 935 kg. Dimensions
  cited to `RS14`'s own stated chain end, "Mitrikas (personal communication, 2011)" — recorded, not
  resolved further, the same treatment GPS-IIF's own "an unpublished document" source gets
  (`SPCR-R-004`).
- **SPCR-R-015.** Every numeric value `SPCR-R-012`–`SPCR-R-014` state is a `Cited<double>` or
  `Cited<Vec3>`, the same rule `SPCR-R-006`/`SPCR-R-011` state for GPS and Galileo — this spec adds
  no exemption for GLONASS either; the shape-blend caveat (§3) is carried INSIDE the optics
  citation string, not a separate uncited annotation.
- **SPCR-R-016.** `qzss_1(life: QzssLife) -> Result<Macromodel, SpacecraftError>` — `SPI_QZS1_B` §6
  Table 4's own body-fixed faces (multi-material faces built as separate co-normal surfaces, mapped
  through `qzss_frame_from_native`, §3) plus one `flat_surface_sun_pointing` (the SAP material, +Y
  and -Y areas summed, §3's own footnote-based reasoning). The +Z face's own "L-ANT Cover" material
  is OMITTED (§3). `life` selects `SPI_QZS1_B` Table 1's own BOL or EOL mass/CoM entry, REQUIRED,
  no default (mirroring `SPCR-R-009`'s own `OpticalLife` selector exactly, the manager's own
  instruction) — every optical coefficient is a single, undated set (Table 4 carries no BOL/EOL
  split, unlike IOV's own Material 2), so `life` affects ONLY mass and CoM, not surfaces. Always
  succeeds for either `life` value — `SPI_QZS1_B` gives one table, no per-epoch coverage boundary to
  refuse against (unlike `GALSC`'s own per-satellite dated entries).
- **SPCR-R-017.** `qzss_frame_from_native(native: Vec3) -> Vec3` rotates a unit or offset vector
  from `SPI_QZS1_B`'s own native body frame to this tree's own convention: 180° about Z, `(x,y,z) ->
  (-x,-y,z)`. Its own inverse (an involution). HYPOTHESISED from the source's own stated Sun-
  hemisphere property (§3), CONFIRMED against real attitude data (`tools/orbex_qzss_check.cpp`,
  `SPEC-qzss-attitude.md` `QZSY-R-004`) — not verified against printed coordinate pairs the way
  `SPCR-R-008` is, `SPI_QZS1_B` printing none.
- **SPCR-R-018.** Every numeric value `SPCR-R-016`/`SPCR-R-017` state is a `Cited<double>` or
  `Cited<Vec3>`, the same rule every other block in this spec states — no exemption for QZSS either.
- **SPCR-R-019.** `beidou() -> Result<Macromodel, SpacecraftError>` **refuses** (`SPCR-F-006`)
  unconditionally in this version: the rule-4/licence search (§2) found five independent reasons,
  any one alone sufficient — curved surfaces (`BD 420025-2019` §5.3's own "planes, cylinders, rings,
  parabolic, etc.") this schema cannot hold; no POPULATED per-satellite value of any kind anywhere
  in the one source found (§3's own full account — the document is a file-format specification, not
  a data product, a refinement of an earlier, narrower "no centre-of-mass field" reading); the
  maneuver-yaw mode's own equations extracted OCR-garbled; no stated redistribution terms; no
  current consumer. Every reason is named in the refusal's own message, not only in this spec.
- **SPCR-R-020.** `sentinel6() -> Result<Macromodel, SpacecraftError>` — `SATMOD` §16.3's own twelve
  body-fixed `FlatSurface`s, EVERY row carrying both a visible and an infrared `OpticalTriple` (§3's
  own first-use finding), two rows renormalised from their own printed (non-unit) components (§3).
  Mass and centre of mass are §16.1's own baseline (1191.831 kg; `(1.5274, -0.0073, 0.0373)` m, §3's
  own baseline-not-epoch reasoning). Always succeeds — one satellite, one table.
- **SPCR-R-021.** `jason2(source: JasonMassSource) -> Result<Macromodel, SpacecraftError>` — `SATMOD`
  §7.3's own eight body-fixed `FlatSurface`s (6 bus + 2 solar array, the array's own FIXED normal, §3),
  every row carrying both a visible and an infrared `OpticalTriple`. Mass and centre of mass are
  §7.1's own baseline (505.9 kg; `(0.9768, 0.0001, 0.0011)` m), selected by `JasonMassSource`
  (`SPCR-R-024`'s own header comment states why this selector exists instead of an epoch lookup).
  Always succeeds for the one legal `source` value.
- **SPCR-R-022.** `jason3(source: JasonMassSource) -> Result<Macromodel, SpacecraftError>` — `SATMOD`
  §12.3's own table, IDENTICAL to `jason2()`'s own §7.3 table cell by cell (§3, `SPCR-A-025`), built
  from its OWN citation (§12.3, not a call into `jason2()`). Mass and centre of mass are §12.1's own
  baseline (509.6 kg; `(1.0023, 0.0000, -0.0021)` m), same `JasonMassSource` selector.
- **SPCR-R-023.** `jason1() -> Result<Macromodel, SpacecraftError>` **refuses** (`SPCR-F-007`)
  unconditionally in this version: `SATMOD` §6.3 states its own optics were tuned and carry an
  overall 0.97 scale factor (§3's own full account) — checked directly, every body-face row found to
  sum to something other than 1, some with negative cells the schema's own physical triple cannot
  hold either way. No current consumer.
- **SPCR-R-024.** `JasonMassSource { Baseline }` — REQUIRED, no default, an EXPLICITLY NAMED selector
  per the manager's own instruction, with exactly one legal value today (§3's own reasoning: the
  offset files' own dense, thousands-of-row operational-log shape is not "Galileo's shape") — a
  future round that adds real epoch-based lookup extends this enum, forcing every call site to
  choose, rather than silently keeping today's baseline-only behaviour.
- **SPCR-R-025.** Every numeric value `SPCR-R-020`–`SPCR-R-022` state is a `Cited<double>` or
  `Cited<Vec3>` (`SPEC-macromodel`'s own `MCRM-R-004`), the same rule every other block in this spec
  states — no exemption for Sentinel-6 or Jason either.

---

## 5. Interfaces, stated language-free

- `SpacecraftError = odl::Diagnostic`, this module's own alias, matching every other module's
  established pattern.
- `gps_block_i(svn: int) -> Result<Macromodel, SpacecraftError>`
- `gps_block_ii_iia(is_iia: bool) -> Result<Macromodel, SpacecraftError>`
- `gps_block_iir() -> Result<Macromodel, SpacecraftError>`
- `gps_block_iir_m() -> Result<Macromodel, SpacecraftError>`
- `gps_block_iif() -> Result<Macromodel, SpacecraftError>` — built from `RS14` Table 5.5, §4
  `SPCR-R-004`.
- `gps_block_iiia() -> Result<Macromodel, SpacecraftError>` — refuses unconditionally, §4
  `SPCR-R-007`.
- `galileo_frame_from_mechanical(mechanical: Vec3) -> Vec3` — §4 `SPCR-R-008`.
- `OpticalLife { BeginningOfLife, EndOfLife }` — §4 `SPCR-R-009`'s own required selector.
- `galileo_iov(gsat: int, epoch: YearMonth, life: OpticalLife) -> Result<Macromodel, SpacecraftError>`
  — §4 `SPCR-R-009`.
- `galileo_foc(gsat: int, epoch: YearMonth) -> Result<Macromodel, SpacecraftError>` — §4
  `SPCR-R-010`. No `life` parameter — `GALSC`'s own FOC table carries no BOL/EOL label at all.
- `YearMonth { year: int, month: int }`, ordered — §3's own stated precision match to `GALSC`'s
  own dated entries.
- `glonass() -> Result<Macromodel, SpacecraftError>` — §4 `SPCR-R-012`, built from `RS14` Table 5.6.
- `glonass_m() -> Result<Macromodel, SpacecraftError>` — §4 `SPCR-R-013`, built from `RS14` Table 5.7.
- `glonass_k() -> Result<Macromodel, SpacecraftError>` — §4 `SPCR-R-014`, built from `RS14` Table 5.8.
  All three parameterless: `RS14` gives one table per block, no per-satellite or per-epoch axis.
- `QzssLife { BeginningOfLife, EndOfLife }` — §4 `SPCR-R-016`'s own required selector.
- `qzss_frame_from_native(native: Vec3) -> Vec3` — §4 `SPCR-R-017`.
- `qzss_1(life: QzssLife) -> Result<Macromodel, SpacecraftError>` — §4 `SPCR-R-016`, built from
  `SPI_QZS1_B` Table 4/Table 1.
- `beidou() -> Result<Macromodel, SpacecraftError>` — refuses unconditionally, §4 `SPCR-R-019`.
- `sentinel6() -> Result<Macromodel, SpacecraftError>` — §4 `SPCR-R-020`, built from `SATMOD` §16.
- `JasonMassSource { Baseline }` — §4 `SPCR-R-024`'s own required selector.
- `jason2(source: JasonMassSource) -> Result<Macromodel, SpacecraftError>` — §4 `SPCR-R-021`, built
  from `SATMOD` §7.
- `jason3(source: JasonMassSource) -> Result<Macromodel, SpacecraftError>` — §4 `SPCR-R-022`, built
  from `SATMOD` §12.
- `jason1() -> Result<Macromodel, SpacecraftError>` — refuses unconditionally, §4 `SPCR-R-023`.

Each function is a pure, parameterless (or SVN-/GSAT-/epoch-/life-/source-parameterised) constructor:
no file is read, no network reached; the cited literature is data this module states directly, the
same shape `ecom::d4b1_order()` names a configuration rather than reading one.

---

## 6. Precision and accuracy

- **SPCR-P-1.** `RS14`'s own values are stated to the precision it prints (three decimal places
  for area, three for each optical coefficient) and are not rounded further here. GPS-IIF's mass is
  **`IGSMETA`'s own 1633 kg** (`SATELLITE/MASS`, SVN63/G063, `gps_block_iif`'s own `SPCR-R-004`),
  not `RS14`'s own figure, as of this round's own mass-source correction (§3). Three mass figures
  sit alongside each other, none silently dropped: `RS14` Table 5.5's own 1555 kg (now the
  cross-check), a U.S. Space Force fact sheet's own 3439 lb = 1559.7 kg (agrees with `RS14` to
  0.3%, found before this round), and `IGSMETA`'s own 1633 kg (agrees with neither to better than
  ~5%). **The two-round-old explanation for the `RS14`/`IGSMETA` gap — launch mass against
  on-orbit/dry mass — is WITHDRAWN**: `SMSD24` (§2, §3) states `SATELLITE/MASS` is documented as
  in-orbit mass, for non-gravitational force modelling, checked directly rather than assumed, and
  found to rule that explanation out. The gap is recorded as genuinely open, not resolved by a
  hypothesis that did not survive being checked. The SAME correction applies to IIR/IIR-M
  (`SPCR-R-003`/`-005`): `IGSMETA`'s own 1080 kg (SVN50) against `RS14`'s own 1100 kg, ~1.8%,
  likewise unexplained rather than assumed. A further aggregate check, GPS-IIF's own panel span
  against a published figure, was sought and not completed — no independently-verified source
  was found (`SPCR-R-004`).
- Every surface's own α + specular + diffuse sums to 1.000 exactly as `RS14`'s own table prints it
  (`SPCR-A-001` checks this at every surface of every built block) — a property of the source's own
  arithmetic, not an independent measurement this tree makes.
- **SPCR-P-2.** `GALSC`'s own values are stated to the precision it prints (three decimal places
  for area, mass and CoM; two for most optical coefficients). Mass and CoM are per-satellite,
  dated to the MONTH `GALSC` states ("as of April 2024" IOV, "as of May 2026" FOC) — this spec does
  not claim day- or second-level validity the source itself does not state (§3's own `YearMonth`).
  Every material's own α + ρ + δ sums to 1.000 exactly as `GALSC`'s own table prints it
  (`SPCR-A-010`), the same property `SPCR-A-001` checks for GPS. `GALSC`'s own +Z-panel
  inconsistency (FOC, ~0.46%, `SPCR-R-010`) is the only area discrepancy found between its own
  summary and detailed tables — every other face and both blocks' own wing totals match exactly
  (`SPCR-A-011`).
- **SPCR-P-3.** `RS14`'s own GLONASS/GLONASS-M/GLONASS-K values are stated to the same precision as
  its GPS tables (three decimal places). The ±X/±Y bus faces of `glonass()`/`glonass_m()` are built
  under RS14's own flat-law (shape=0) special case, UNDERSTATING the true cylindrical contribution
  RS14's own Eq. 4.5 would give (§3's own full account, `SPCR-Q-007`) — a stated approximation, not
  a rounding-level one: the shape fractions RS14 prints (0.494–0.728) are not small.
- **SPCR-P-4.** `SPI_QZS1_B`'s own values are stated to the precision it prints (one decimal place
  for area and mass, one for CoM in mm, four decimal places for most optical coefficients — Table
  4's own printed precision varies row to row, not rounded further here). Mass and CoM are dated to
  a LIFE STAGE (`QzssLife`), not a calendar epoch — `SPI_QZS1_B`'s own Table 1 states BOL/EOL
  directly, with no intermediate schedule, so no interpolation or "as of" pin is offered where the
  source states none. Table 1 is captioned "Prediction as a design," not a measured in-orbit value —
  stated in each mass/CoM citation, the same distinction `SPCR-P-1` already draws for `RS14`'s own
  cross-check figures elsewhere.
- **SPCR-P-5.** `SATMOD`'s own values are stated to the precision it prints (three decimal places for
  area, mass and CoM; four decimal places for optical coefficients). Sentinel-6's own infrared triples
  sum to exactly 1.000 on every row (uniform 0.100/0.800/0.100); Jason-2's/Jason-3's own VISIBLE
  triples sum to exactly 1.000 on every row, but their own INFRARED triples do NOT always — a small
  (<=0.2%), genuine, source-side rounding property (e.g. the -Y row's own 0.104+0.569+0.328=1.001),
  checked directly and recorded here rather than silently absorbed by a loose test tolerance nobody
  explains (`SPCR-A-025`).
- **SPCR-P-6.** Mass and centre of mass for `sentinel6()`/`jason2()`/`jason3()` are each source
  section's own BASELINE value, not the current in-orbit value an epoch lookup would give — the gap
  between baseline and current is small (the offset files' own printed deltas: Sentinel-6 up to ~12 kg
  against a 1192 kg baseline, roughly 1%; Jason-2/-3 similarly small fractions of their own smaller
  baselines) but not zero, `SPCR-Q` (this spec's own open question) names it explicitly rather than
  implying the baseline is current.

---

## 7. Failure behaviour

| id | fires when | carries |
|---|---|---|
| `SPCR-F-001` | `gps_block_i` called with an SVN not among `RS14` Table 5.2's own eight (03, 04, 06, 08, 09, 10, 11) | the offending SVN, and the two mass groups this spec does hold |
| `SPCR-F-002` | **retired, does not fire in this version** — was `gps_block_iif` called at all; withdrawn 2026-09-24 when `SPCR-R-004` was ruled and built from `RS14` Table 5.5. Kept documented, not deleted, for traceability (an earlier commit's own tests referenced it) | — |
| `SPCR-F-003` | `gps_block_iiia` called at all, in this version | the one-search outcome, and where its full record is kept; states explicitly that no baseline consumes this function |
| `SPCR-F-004` | `galileo_iov`/`galileo_foc` called with a `gsat` not among `GALSC`'s own currently-listed satellites for that block | the offending GSAT and the block name |
| `SPCR-F-005` | `galileo_iov`/`galileo_foc` called with an `epoch` before the named satellite's own mass/CoM entry's stated month | the offending epoch, the satellite's own coverage start, and the `odl::atmosphere`-style reasoning |
| `SPCR-F-006` | `beidou` called at all, in this version | all five independent reasons (§3, §4 `SPCR-R-019`), and where the full search record is kept; states explicitly that no baseline consumes this function |
| `SPCR-F-007` | `jason1` called at all, in this version | both reasons (non-energy-conserving optics, checked cell by cell; the 0.97 scale factor this schema has no field for), and that no baseline consumes this function |
| (inherited) `MCRM-F-001` | any citation this module supplies is blank | the schema's own refusal, unchanged — this module supplies none blank, `SPCR-A-002` proves the guard still fires if one were |

---

## 8. Acceptance tests

| id | what is checked | expected value | source | tolerance | discharges |
|---|---|---|---|---|---|
| `SPCR-A-001` | every surface of every built block (I, II/IIA, IIR, IIR-M, IIF): α + specular + diffuse = 1 | exactly 1 | `RS14`'s own arithmetic, cross-checked | 1e-12 | R-001, R-002, R-003, R-004, R-005 |
| `SPCR-A-002` | **the guard shown firing**: a deliberately blank citation on one value of a test-local copy of `gps_block_iir`'s own construction is refused by `MCRM-F-001`, through this module's own call path — proving the inherited refusal actually reaches a caller of THIS module, not only `macromodel`'s own existing suite | the refusal, `MCRM-F-001` | `SPCR-R-006` | — | R-006 |
| `SPCR-A-003` | `gps_block_i`'s own +Z face matches `RS14` Table 5.2 cell by cell, with the δ/ρ mapping (§3, formula-verified) applied, not the naive (unswapped) reading; every built block's own solar panel independently satisfies `specular > diffuse` (glass-panel physical cross-check) | agreement to `RS14`'s own printed precision; specular > diffuse | `RS14` Table 5.2, read directly in the test | exact (transcription) | R-001 |
| `SPCR-A-004` | `gps_block_iif` builds from `RS14` Table 5.5 (every bus face/panel value, including the pure-absorber −Z row, α=1.000); area citation traces to "an unpublished document", optics citation is marked ASSUMED — the two citations checked to actually differ, proving the split is real, not cosmetic; panel `specular > diffuse` (same physical cross-check as `SPCR-A-003`); mass is `IGSMETA`'s own 1633 kg (not `RS14`'s 1555 kg), its own citation checked to both name `IGSMETA` and record `RS14`'s 1555 kg as the cross-check | agreement to `RS14` Table 5.5's own printed precision; citations differ; specular > diffuse; mass = 1633 kg, citation names both sources | `RS14` Table 5.5; `IGSMETA` `SATELLITE/MASS`, read directly in the test | exact (transcription) | R-004 |
| `SPCR-A-005` | `gps_block_i` refuses `SPCR-F-001` on an SVN outside the eight named, and does **not** refuse on each of the eight | the diagnostic; success on all eight | `SPCR-R-001`'s own domain | — | F-001 |
| `SPCR-A-006` | `gps_block_iir_m()`'s own surfaces equal `gps_block_iir()`'s own, field by field, value AND citation — the citation naming the inheritance, not silently identical by coincidence | identical values; citation states the inheritance | `SPCR-R-005` | exact | R-005 |
| `SPCR-A-007` | `gps_block_iiia` refuses, unconditionally, with `SPCR-F-003` | the refusal | `SPCR-R-007` | — | F-003, R-007 |
| `SPCR-A-008` | `gps_block_iir`'s and `gps_block_iir_m`'s own mass is `IGSMETA`'s own 1080 kg (SVN50), not `RS14`'s own 1100 kg — both citations checked to name `IGSMETA`/`SVN50` and to record `RS14`'s 1100 kg as the cross-check, plus a guard that 1080 ≠ 1100 (proving a regression to the old value would fail) | mass = 1080 kg on both; citations name the source and the cross-check | `IGSMETA` `SATELLITE/MASS`, read directly in the test | exact | R-003, R-005 |
| `SPCR-A-009` | `galileo_frame_from_mechanical`, VERIFIED against three real `GALSC` coordinate pairs (its own Mechanical RF / ANTEX RF columns for the identical physical point, two IOV and one FOC) — not trusted from the yaw law's own stated sign convention alone; a guard that an X-only flip (matching the yaw law's own prose read in isolation) would NOT reproduce the real data; an involution check | agreement with `GALSC`'s own printed ANTEX-RF values | `GALSC` §5 ARP/LRR tables, read directly in the test | 1e-9 | R-008 |
| `SPCR-A-010` | every material row `galileo_iov`/`galileo_foc` build has α + specular + diffuse = 1, `GALSC`'s own arithmetic; each built macromodel's own surface count matches the table's own row count (12 for IOV, 13 for FOC) | exactly 1; the stated counts | `GALSC` §6, cross-checked | 1e-12 | R-009, R-010 |
| `SPCR-A-011` | both of `GALSC`'s own wings (IOV: +Y/-Y; FOC: +SA/-SA) are identical in area and optics, checked cell by cell before being summed; the built macromodel's own sun-pointing surfaces carry the summed areas (7.76/3.06 m² IOV, 7.760/3.060 m² FOC) | identical inputs; summed outputs present | `GALSC` §6, read directly in the test | exact | R-009, R-010 |
| `SPCR-A-012` | mass/CoM lookup: the right value at a known GSAT; refused for an unknown GSAT (`SPCR-F-004`); refused for an epoch one month before coverage, shown firing exactly at that boundary; succeeds exactly at the coverage start and well after (open-ended coverage) | the diagnostics; the stated values | `SPCR-R-009`/`-010`'s own domain | 1e-9 | F-004, F-005, R-009, R-010 |
| `SPCR-A-013` | every built IOV/FOC macromodel is fully cited (mass, CoM, every surface's own area and absorptivity); the citation-refusal guard (`MCRM-F-001`) reaches this module's own call path; the centre of mass is a real, nonzero offset (unlike GPS's own (0,0,0) default) | no empty citation; the refusal fires; `\|com\| > 0.1` m | `SPCR-R-011` | — | R-011 |
| `SPCR-A-014` | `galileo_iov`'s own BOL and EOL optics genuinely differ where `GALSC` prints different coefficients (the Optical surface radiator, +X/+Y/-Y) and agree exactly where it prints the same ones (the Germanium-coated Kapton foil, +Z; Material 1; both wings) — every surface's own area and normal unchanged either way, only optics vary | BOL/EOL differ on 3 faces, agree on the rest; areas identical | `GALSC` §6.1, read directly in the test | exact | R-009 |
| `SPCR-A-015` | every surface of `glonass()`/`glonass_m()`/`glonass_k()`: α + specular + diffuse = 1; 7 surfaces each; the +Z bus row matches `RS14` Table 5.6 cell by cell, δ/ρ mapping applied; masses 1415/1415/935 kg | exactly 1; 7 surfaces; agreement to `RS14`'s own printed precision; the stated masses | `RS14` Tables 5.6–5.8, read directly in the test | 1e-9/1e-12 | R-012, R-013, R-014 |
| `SPCR-A-016` | the ±X/±Y bus faces of `glonass()`/`glonass_m()` carry a citation stating RS14's own cylinder-wing "shape" blend and this build's own flat-law approximation; the ±Z faces and panels, and EVERY `glonass_k()` face (no "shape" column in RS14's own Table 5.8), do not — the split is real, checked by substring | 4 caveated / 3 plain, both blocks; `glonass_k()` entirely plain | this file's own §3 finding | — | R-012, R-013, R-014 |
| `SPCR-A-017` | `glonass_k()`'s own dimension citation names RS14's own stated chain end, "Mitrikas (personal communication, 2011)" | citation contains that string | `RS14` Table 5.8's own "Information sources" | — | R-014 |
| `SPCR-A-018` | **the guard shown firing**: a deliberately blank citation on a test-local value, through this module's own `Cited`/`body_direction` call path, is refused by `MCRM-F-001` | the refusal, `MCRM-F-001` | `SPCR-R-015` | — | R-015 |
| `SPCR-A-019` | `glonass()`/`glonass_m()`'s own ±X/±Y optics and `glonass_k()`'s own optics (every face) are marked ASSUMED, `RS14`'s own generic Ziebart (2001) fallback | citation contains "ASSUMED" | `RS14` Tables 5.6–5.8's own "Information sources" | — | R-012, R-013, R-014 |
| `SPCR-A-020` | `qzss_frame_from_native` is an involution and has the stated 180°-about-Z form, checked as pure algebra | exact algebraic match; involution holds | this file's own §3 derivation | 1e-12 | R-017 |
| `SPCR-A-021` | `qzss_1` (BOL and EOL alike): every surface's own absorption+specular+diffuse sums to 1; 9 surfaces (8 body-fixed + 1 combined SAP); the +Y Radiator row matches Table 4 cell by cell; BOL and EOL genuinely differ in mass and CoM, surfaces unchanged either way | exactly 1; 9 surfaces; agreement to Table 4's own printed precision; BOL != EOL mass/CoM | `SPI_QZS1_B` Table 4/Table 1, read directly in the test | 1e-9/1e-12 | R-016 |
| `SPCR-A-022` | the SAP is built sun-pointing, not body-fixed, with the summed +Y/-Y area (45.0 m²); every other material is body-fixed | 1 sun-pointing surface, area 45.0 m²; 8 body-fixed | `SPI_QZS1_B` Table 4's own footnote *2 | 1e-9 | R-016 |
| `SPCR-A-023` | **the guard shown firing**: a deliberately blank citation, through this module's own `Cited`/`body_direction` call path, is refused by `MCRM-F-001` | the refusal, `MCRM-F-001` | `SPCR-R-018` | — | R-018 |
| `SPCR-A-024` | `beidou` refuses unconditionally with `SPCR-F-006`; the refusal's own message names all five reasons (curved surfaces, no populated per-satellite value, the unread maneuver-yaw equations, no redistribution terms, no consumer), checked by substring, not merely asserted | the refusal; all five reasons present | `SPCR-R-019` | — | F-006, R-019 |
| `SPCR-A-025` | `jason2()`/`jason3()`: the source's own stated equality checked cell by cell (area, every optics coefficient, both bands); 8 surfaces each, every body-fixed (solar array included); citations differ between the two (independent citation, not a silent call-through) | agreement to `SATMOD`'s own printed precision; citations differ | `SATMOD` §7.3/§12.3, read directly in the test | 1e-12/2e-3 (infrared) | R-021, R-022 |
| `SPCR-A-026` | `sentinel6()`: every surface's own visible AND infrared triple sums to 1; 12 surfaces, all body-fixed; every normal unit length | exactly 1 (1e-9); 12 surfaces; unit normals | `SATMOD` §16.3, read directly in the test | 1e-9 | R-020 |
| `SPCR-A-027` | `sentinel6()`: infrared optics are uniform (0.100/0.800/0.100) on every row, and genuinely populated (not silently falling back to the visible triple, `BandedOptics::in()`'s own documented fallback) | infrared matches; differs from visible where visible does | `SATMOD` §16.3 | 1e-9 | R-020, R-025 |
| `SPCR-A-028` | `sentinel6()`: the two non-axis-aligned face normals, AS PRINTED, do not renormalise to unit length (checked against the source's own printed components directly); the built surface carries the RENORMALISED direction | printed norm != 1 (1.0002, 0.9560); built normal = printed/norm exactly | `SATMOD` §16.3, cross-checked against the rendered PDF page | 1e-6 | R-020 |
| `SPCR-A-029` | `sentinel6()`: mass and CoM are §16.1's own baseline; every value cited; the citation-refusal guard reaches this module's own call path | the stated values; the refusal, `MCRM-F-001` | `SATMOD` §16.1 | 1e-9 | R-020, R-025 |
| `SPCR-A-030` | `jason2()`: the solar array rows carry a FIXED `(+1,0,0)`/`(-1,0,0)` body-frame normal exactly as §7.3 prints them, not a sun-pointing surface | 0 sun-pointing surfaces; 1 each of +X/-X at area 9.8 | `SATMOD` §7.3, read directly in the test | — | R-021 |
| `SPCR-A-031` | `jason2()`/`jason3()`: mass and CoM are each section's own baseline, genuinely different between the two satellites | 505.9/509.6 kg respectively | `SATMOD` §7.1/§12.1 | 1e-9 | R-021, R-022 |
| `SPCR-A-032` | `jason1()` refuses unconditionally with `SPCR-F-007`; the refusal's own message names both reasons (the 0.97 factor, the non-energy-conserving optics quantified) and states no consumer needs it, checked by substring | the refusal; both reasons present | `SPCR-R-023` | — | F-007, R-023 |

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `SPCR-F-002` | Retired, §7: withdrawn 2026-09-24 when `SPCR-R-004` was ruled and built, and `SPCR-A-004` rewritten to test the real construction instead of this refusal. No code path returns it any more, so no test can discharge it; kept documented, not deleted, for traceability against the earlier commit that did fire it. |
| `SPCR-R-024` | `JasonMassSource`'s own single legal value is a pure type declaration, not an independently testable behaviour — it is exercised indirectly by every `SPCR-A-025`/`-030`/`-031` call, all of which pass `JasonMassSource::Baseline` to `jason2()`/`jason3()` and check the resulting mass/CoM. `OpticalLife`/`QzssLife` avoid this gap by being folded into their OWN constructor's requirement id (`SPCR-R-009`/`-016`) rather than given a separate one — `JasonMassSource` was split out separately in this spec's own numbering, which is why it needs its own excuse here rather than simply inheriting `SPCR-R-021`'s/`-022`'s own discharge the way the others do. |

---

## 9. Provenance obligations

- `PROVENANCE.md` records, as a new numbered section: the `RS14` licence search and the manager's
  own ruling to use it anyway, with reasoning; the AGU embargo-route search and its result; the
  Block IIF absence, its own full first-pass search, and the manager's own later ruling to build it
  from `RS14` Table 5.5 anyway, area and optics cited separately; the δ/ρ notation mapping error —
  a first pass trusted `RS14` §5.1.2's own prose alone, the manager caught that this was verified by
  words and not the force equation, and re-verification against `RHS12`'s own reprinted Eq. 6 (plus
  an independent physical specular-panel check) found the prose-only reading backwards and the
  formula-based one correct, cross-checked against `SPEC-macromodel.md`'s own already-correct
  convention (no L4 spec needed correction) — recorded as the FULL story, including the wrong first
  pass, not only the corrected end state; the IIF/IIR/IIR-M mass-source correction — a first pass
  used `RS14`'s own figures with `IGSMETA` as an aggregate cross-check, explained by an
  unchecked launch-vs-on-orbit hypothesis; the manager asked for the hypothesis to be checked
  against what `IGSMETA`'s own field is actually documented to be; `SMSD24` (fetched this round)
  settled it — in-orbit mass, for non-gravitational force modelling, not launch mass — so `IGSMETA`
  became primary for IIR/IIR-M/IIF and the hypothesis was WITHDRAWN, the resulting ~5%/~1.8% gaps
  recorded as genuinely open, not re-explained; a panel-span check sought and not completed; the
  Block IIIA one-search absence; the IIR-M geometry-sharing inference and its own textual basis in
  `MSGA15`.
- **L5 step 2 (Galileo)**: the `GALSC` rule-4/licence search and result (clean, unlike `RS14`);
  the frame mapping VERIFIED against real coordinate pairs (and the "(+z, anti-nadir)" labelling
  error this caught and corrected in §3 above, a mislabel, not a convention this tree ever actually
  used backwards); the per-satellite dated mass/CoM design ruling and its own stated shape;
  the α/ρ/δ quote and its own sum-to-1 confirmation; the FOC +Z-panel area inconsistency
  (`SPCR-R-010`); the HTML table re-parse that replaced an earlier, alignment-losing flattened-text
  pass.
- **L5 step 3 (GLONASS)**: the manager's own ruling extending `RS14`'s own secondary-source status
  to its GLONASS/GLONASS-M/GLONASS-K tables; the GENUINE schema-fidelity limitation found reading
  `RS14` §4.2 closely — the ±X/±Y bus faces of GLONASS and GLONASS-M are a cylinder/flat "shape"
  blend with a distinct force formula (Eq. 4.5) this schema cannot represent — and the judgment call
  made to build those four faces under the flat-law special case anyway, stated explicitly in each
  affected citation, reported for the manager's own ruling rather than decided silently either way
  (`SPCR-Q-007`); GLONASS-K's own dimension chain ending at "Mitrikas, personal communication,
  2011," recorded the same way GPS-IIF's "an unpublished document" is.
- **L5 step 3 (QZSS)**: the rule-4/licence search and result (clean, "freely available to any user");
  the frame mapping HYPOTHESISED from the source's own stated Sun-hemisphere property and a second,
  independent reading (the yaw-steering mode's own y-axis definition), then CONFIRMED against real
  attitude data (four checks, two satellites, 0.00003°–0.00019°, the tightest real-data agreement
  any frame mapping in this tree has had) rather than trusted from prose alone; the L-ANT Cover's
  own missing area and unsupported cone shape, found reading Table 4 closely, omitted rather than
  approximated; the SAP's own sun-pointing treatment, resolved by Table 4's own footnote *2 rather
  than assumed from its "Location" column; the BOL/EOL mass selector mirroring Galileo's own
  `OpticalLife` exactly, per the manager's own instruction.
- **L5 step 3 (BeiDou, carried)**: the rule-4/licence search against `BD 420025-2019`; the REFINED
  finding that the standard is a file-format specification with no populated per-satellite value
  anywhere, superseding an earlier, narrower "no centre-of-mass field" reading after Appendix A's
  own informative Table A.1 was found to print a centroid-coordinate column alongside mass (an
  apparent normative/informative inconsistency internal to the standard, reported rather than
  silently resolved either way, the SAME class of finding RS14's own alpha/delta/rho notation
  already was); the curved-surface types, the maneuver-yaw mode's own OCR-garbled equations, and the
  redistribution-terms search, each independently sufficient; the refusal's own message naming every
  reason, checked by test.
- **L5 step 4 (Sentinel-6, Jason-2, Jason-3, Jason-1)**: the `SATMOD` rule-4/licence search (the
  paywalled Cerri et al. 2010, the usable CNES note instead, its own placeholder Legal Notice); the
  `RS14` ruling extended a second time, this note's own edition hash-pinned; the STALE-CACHE finding —
  an earlier pass this round cached a Jason-2/-3 table from what turned out to be an outdated read,
  caught by re-reading the current, hash-pinned fetch directly before any code was written, the exact
  before/after numbers recorded; `BandedOptics`'s own infrared field populated for the first time;
  Sentinel-6's own two non-unit printed normals, found and confirmed against the rendered PDF page
  directly, renormalised; Jason's own solar array found to be body-fixed, not Sun-tracking, a
  departure from every earlier constellation's own panel treatment; Jason-1's own refusal reasoning,
  matching the BeiDou/GPS-IIIA "checked directly, not assumed from the word alone" pattern; the
  mass/CoM baseline-vs-epoch-lookup judgment call, differently reasoned for Sentinel-6 (small file,
  no ruling requirement) and Jason (large operational log, the manager's own offered fallback taken).

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `SPCR-Q-001` | **Block IIF — RESOLVED** (manager, 2026-09-24). Ruled: build from `RS14` Table 5.5 directly (it publishes per-surface values itself) rather than the imagery-derivation fallback `../plan/PLAN.md` §3.6 names — dimensions cited to `RS14`'s own stated chain-end ("an unpublished document"), optics marked ASSUMED (`RS14`'s own generic Ziebart (2001) assumption, not a new analogue chosen from outside the source). Built, `SPCR-R-004`; the imagery-derivation route was not needed and remains unused. |
| `SPCR-Q-002` | **Per-satellite mass from `IGSMETA`, PARTIALLY RESOLVED this round.** `gps_block_iir`/`_iir_m`/`_iif` now use `IGSMETA`'s own per-satellite figure for their own single reference SVN (50, 50, 63 respectively) as the primary mass, §3. Still open: a genuine per-SVN table for satellites OTHER than these three references (e.g. a specific non-reference IIR-M SVN L7 might one day need) remains unbuilt — worth doing before L7 needs more than the reference satellites, or acceptable to keep at reference-satellite granularity? |
| `SPCR-Q-003` | **What does `IGSMETA`'s own `SATELLITE/MASS` field mean — RESOLVED** (manager asked this round, quote required before trusting the field for SRP). `IGSMETA`'s own header: `"SATELLITE/MASS  In-orbit satellite mass"`. `SMSD24` §1.1, in full: *"Knowledge of the mass of a GNSS satellite is required to compute the acceleration caused by non-gravitational forces (such as solar radiation pressure, radiation thrust, or Earth radiation pressure). In line with the quality of other model parameters, a 1% accuracy is typically deemed adequate for this purpose. Updates following the start of initial operations are only required after maneuvers and incremental mass changes of more than 1 kg."* Not launch mass, not unstated — documented, specifically, for this library's own purpose. `SMSD24` §4.3's own Table 5 gives block-level in-orbit figures independently (GPS IIR/IIR-M 1080 kg, Hegarty 2017; IIF 1633 kg, a Boeing technical-specifications page), matching this session's own per-SVN reads (SVN50, SVN63) exactly — and states explicitly that Block I/II/IIA's own individually-varying `FLGA92` masses are NOT incorporated into this SINEX block, which is why those three blocks keep `RS14` as primary (§3) rather than switching too. |
| `SPCR-Q-004` | **`GALSC`'s own End-Of-Life optical coefficients for IOV — RESOLVED** (manager, 2026-09-24). Ruled: an explicit `OpticalLife` selector, REQUIRED, no default (`SPCR-R-009`) — both BOL and EOL now built, chosen by the caller, since every IOV satellite is long past early life and a silent default would be the wrong answer for present use. `SPCR-A-014` checks the selector actually reaches the built surfaces. |
| `SPCR-Q-005` | **FOC's own "modified yaw steering law" is not built (`GALY-Q-001`, `SPEC-galileo-attitude.md`) — `galileo_yaw_attitude` refuses instead, near colinearity.** Does any consumer need FOC attitude that close to colinearity (β < 4.1°, ε < 10°) before this is worth building? The condition is rare (a narrow geometric window) and GSC's own text frames it as a smoothing measure, not a large-swing regime the way GPS's own noon/midnight turns are. |
| `SPCR-Q-006` | **A genuine per-SVN mass table for Galileo satellites GSC does not currently list (205, 228–231) or for GSAT numbers retired since this pin.** Not searched this round — `GALSC`'s own table is used as printed, absences not filled in or guessed at. Worth a follow-up search if L7 needs one of these specifically. |
| `SPCR-Q-007` | **The GLONASS/GLONASS-M cylinder-wing shape-blend approximation — RESOLVED (manager, 2026-09-24 review).** Ruled: keep the flat-law approximation as built, bounded with a number rather than left as an unquantified judgment call — §3's own new bullet gives the result: 0.57–0.90% of the whole-satellite SRP force for GLONASS, 0.70–1.10% for GLONASS-M, at the Sun-along-normal and 45° geometries, computed from RS14's own Eq. 4.5 self-consistently (the flat law is that same formula's own `s=0` case). Curved surfaces stay carried together with BeiDou's own, gated on a consumer, since the photon-pressure kernel is itself gated for flat panels only. |
| `SPCR-Q-008` | **Only QZS-1's own macromodel is built.** QZS-2, QZS-3, QZS-4 and QZS-1R's own SPI documents (QZS-1R's own PDF was fetched this session but not read) were not built this round — each would need its own per-satellite SPI read the same way QZS-1's was, `SPI_QZS1_B` itself stating no other satellite's specific mass/CoM/geometry. The attitude LAW is treated as constellation-wide and confirmed against two of these other satellites' real data (`SPEC-qzss-attitude.md` `QZSY-R-004`), but their own macromodels remain unbuilt. Worth doing if L7 needs more than QZS-1. |
| `SPCR-Q-009` | **The L-ANT Cover's own omission (§3) leaves QZS-1's own +Z-face geometry incomplete** — a real physical surface (a truncated cone, both faces of which see sunlight at different times) is simply absent from the built macromodel, not merely approximated. `SPI_QZS1_B` itself names a "box-wing-hat model" (Ikari et al. 2014) built specifically to handle this shape more accurately — not pursued, since this schema could not hold its own output regardless. Worth a schema extension (a conical or general axisymmetric surface type) if a consumer needs QZS-1's own L-ANT-cover contribution specifically — the SAME class of extension BeiDou's own curved surfaces and GLONASS's own cylinder-wing faces would also benefit from, a recurring gap across three of this round's four constellations. |
| `SPCR-Q-010` | **BeiDou's own refusal reasoning was REFINED this round, past what the manager's own ruling stated — flagged for the manager's own awareness, not a request to reverse the refusal.** The ruling's own shorthand named "no centre of mass field found" as one of BeiDou's four reasons; reading `BD 420025-2019` closely for this build found the fuller picture is that the standard is a FILE-FORMAT specification with no populated per-satellite VALUE of any field anywhere (mass, centroid or optics alike), and that its own informative Appendix A actually DOES print a centroid-coordinate FIELD (just never a real number) alongside its own normative text's silence on centroid — a genuine nuance the first pass's own rule-4 search did not surface. The refusal itself is unaffected (if anything, more strongly justified: even a schema-compatible BeiDou build would have no real numbers to read from this source), so no code change follows from this — recorded so the manager's own record of the reasoning is accurate, not merely so the conclusion is. |
