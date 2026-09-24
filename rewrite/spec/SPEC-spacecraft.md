# SPEC-spacecraft — the macromodel library, as cited data

| | |
|---|---|
| **Spec ID** | `SPCR` |
| **Status** | **draft** 2026-09-24, for review |
| **Version** | 1.2 — δ/ρ mapping corrected by formula (§3), IIF built (`SPCR-R-004`), IIIA searched and refused (`SPCR-R-007`); mass source corrected to `IGSMETA` for IIR/IIR-M/IIF, launch-vs-on-orbit hypothesis withdrawn (§3, `SMSD24`) |
| **Date** | 2026-09-24 |
| **Layer** | L5 `spacecraft` (`../plan/PLAN.md` §3.6), step 1 (GPS) |
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

**Named GPS satellite-block constructors**, each returning a `macromodel::Macromodel` built from
**published values, each with its own citation** — this layer's own exit gate (`../plan/PLAN.md`
§3.6, amended 2026-09-24): "every value resolves to a citation, and the library refuses to build
if any does not." This spec covers L5 step 1, GPS, across its own six blocks: I, II, IIA, IIR,
IIR-M, and IIF — plus one further block, IIIA, searched once and refused (`SPCR-R-007`) rather
than built, no citable per-surface source having been found for it.

**Not in scope.** The schema itself (`SPEC-macromodel.md`, L4 step 2) — this spec populates it,
never extends it; a value the schema cannot hold is a finding reported to the manager, not a
silent schema change (`../plan/PLAN.md` §3.6's own instruction). Estimating any parameter from
observations (`SPEC-dynamics`, L7) — this library states values, it does not fit them. Galileo,
GLONASS, BeiDou, QZSS and altimetry satellites (`SPEC-spacecraft` grows to cover L5 steps 2–4 as
they are built; this version covers step 1 only). The force law itself (`srp_analytic`,
`photon_force`) — this spec's own output is consumed by that module, unchanged.

---

## 2. Normative sources

| key | author / issuer | title | version / year | locator | obtained | role |
|---|---|---|---|---|---|---|
| `FLGA92` | Fliegel, H. F., Gallini, T. E., Swift, E. R. | *Global Positioning System radiation force model for geodetic applications* | J. Geophysical Research 97(B1): 559–568, 1992 | DOI (paywalled) | **not obtained** — see §2.1 | normative for Block I, II, IIA; not read directly, reached only through `RS14`'s own derived table |
| `FLGA96` | Fliegel, H. F., Gallini, T. E. | *Solar force modeling of block IIR Global Positioning System satellites* | J. Spacecraft and Rockets 33(6): 863–866, 1996 | DOI `10.2514/3.26851` (paywalled) | **not obtained** — see §2.1 | normative for Block IIR; not read directly, reached only through `RS14`'s own derived table |
| `RS14` | Rodríguez-Solano, C. J. | *(doctoral dissertation)*, TU München | 2014 | `data/literature/rodriguez-solano-2014-dissertation/719708.pdf`, retrieved at L4 step 2 (`PROVENANCE.md` §26.1) | **secondary** — retrievable, no login; redistribution terms not stated by the source, see §2.2 | the actual source of every Block I/II/IIA/IIR numeric value this spec states — a derivation from `FLGA92`/`FLGA96`, not a transcription of either |
| `MSGA15` | Montenbruck, O., Schmid, R., Mercier, F., Steigenberger, P. *et al.* | *GNSS satellite geometry and attitude models* | Adv. Space Res. 56: 1015–1029, 2015 | `https://elib.dlr.de/97732/1/ASR_151015_GNSS_SatGeomAtt.pdf` | **primary**, already used at L4 step 6 | the IGS body-frame convention (§3), and IIR-M's own basis for sharing IIR's geometry (§4, `SPCR-R-005`) |
| `IGSMETA` | Steigenberger, P., Montenbruck, O. (maintainers); IGS | *IGS Satellite Metadata (SINEX)* | continuously updated; this pin 2026-09-24 | `https://files.igs.org/pub/station/general/igs_satellite_metadata.snx` | **primary**, open with attribution (IGS's own open data policy, re-checked this session for the redistribution bar, not only retrievability) | which physical block each frozen baseline SVN is (already used, `PROVENANCE.md` §30.1); **as of this version, the primary mass source for `gps_block_iir`/`_iir_m`/`_iif`** (§3, §4 `SPCR-R-003`/`-004`/`-005`) — its own `SATELLITE/MASS` field, per-satellite, SVN50/SVN63 |
| `SMSD24` | Steigenberger, P., Montenbruck, O. | *IGS Satellite Metadata File Description*, v1.10 | 30 September 2024, DOI `10.57677/metadata-sinex` | `https://files.igs.org/pub/resource/working_groups/multi_gnss/Metadata_SINEX_1.10.pdf`, fetched directly 2026-09-24 (same `files.igs.org` domain as `IGSMETA`; redistribution not separately re-verified beyond that) | **primary**, obtained | states what `IGSMETA`'s own `SATELLITE/MASS` field IS — "in-orbit satellite mass," required "to compute the acceleration caused by non-gravitational forces... at ~1% accuracy" (§1.1) — settling `SPCR-Q-003` and its own Table 5 (§4.3), the block-level figures §3 below cites |

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

---

## 3. Definitions and conventions

- **The body frame is the IGS convention, +x towards the Sun, for every GPS block** (`MSGA15`
  Fig. 3/4; ruled at L4 step 6 from `MSGA15` and CODE's own G05 attitude — `SPEC-thrust-yaw.md`,
  and what `odl::attitude` emits in every regime). `RS14` §5.1.1 defines its own XYZ frame
  identically in substance, independently stated, not copied from `MSGA15`: "X normal to the
  surface of the satellite which is always illuminated by the Sun" (+x, Sun-facing), "Z opposite
  to the radial direction" (+z, anti-nadir — the schema's own `body_fixed_normal` for each face is
  stated in this same frame, §4). The two sources' own frame conventions agree; this spec's own
  values are stated in it directly, no rotation applied.
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

Each function is a pure, parameterless (or SVN-parameterised) constructor: no file is read, no
network reached; the cited literature is data this module states directly, the same shape
`ecom::d4b1_order()` names a configuration rather than reading one.

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

---

## 7. Failure behaviour

| id | fires when | carries |
|---|---|---|
| `SPCR-F-001` | `gps_block_i` called with an SVN not among `RS14` Table 5.2's own eight (03, 04, 06, 08, 09, 10, 11) | the offending SVN, and the two mass groups this spec does hold |
| `SPCR-F-002` | **retired, does not fire in this version** — was `gps_block_iif` called at all; withdrawn 2026-09-24 when `SPCR-R-004` was ruled and built from `RS14` Table 5.5. Kept documented, not deleted, for traceability (an earlier commit's own tests referenced it) | — |
| `SPCR-F-003` | `gps_block_iiia` called at all, in this version | the one-search outcome, and where its full record is kept; states explicitly that no baseline consumes this function |
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

**Coverage.** Every requirement and refusal above is discharged by a row, except:

| id | why no test |
|---|---|
| `SPCR-F-002` | Retired, §7: withdrawn 2026-09-24 when `SPCR-R-004` was ruled and built, and `SPCR-A-004` rewritten to test the real construction instead of this refusal. No code path returns it any more, so no test can discharge it; kept documented, not deleted, for traceability against the earlier commit that did fire it. |

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

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `SPCR-Q-001` | **Block IIF — RESOLVED** (manager, 2026-09-24). Ruled: build from `RS14` Table 5.5 directly (it publishes per-surface values itself) rather than the imagery-derivation fallback `../plan/PLAN.md` §3.6 names — dimensions cited to `RS14`'s own stated chain-end ("an unpublished document"), optics marked ASSUMED (`RS14`'s own generic Ziebart (2001) assumption, not a new analogue chosen from outside the source). Built, `SPCR-R-004`; the imagery-derivation route was not needed and remains unused. |
| `SPCR-Q-002` | **Per-satellite mass from `IGSMETA`, PARTIALLY RESOLVED this round.** `gps_block_iir`/`_iir_m`/`_iif` now use `IGSMETA`'s own per-satellite figure for their own single reference SVN (50, 50, 63 respectively) as the primary mass, §3. Still open: a genuine per-SVN table for satellites OTHER than these three references (e.g. a specific non-reference IIR-M SVN L7 might one day need) remains unbuilt — worth doing before L7 needs more than the reference satellites, or acceptable to keep at reference-satellite granularity? |
| `SPCR-Q-003` | **What does `IGSMETA`'s own `SATELLITE/MASS` field mean — RESOLVED** (manager asked this round, quote required before trusting the field for SRP). `IGSMETA`'s own header: `"SATELLITE/MASS  In-orbit satellite mass"`. `SMSD24` §1.1, in full: *"Knowledge of the mass of a GNSS satellite is required to compute the acceleration caused by non-gravitational forces (such as solar radiation pressure, radiation thrust, or Earth radiation pressure). In line with the quality of other model parameters, a 1% accuracy is typically deemed adequate for this purpose. Updates following the start of initial operations are only required after maneuvers and incremental mass changes of more than 1 kg."* Not launch mass, not unstated — documented, specifically, for this library's own purpose. `SMSD24` §4.3's own Table 5 gives block-level in-orbit figures independently (GPS IIR/IIR-M 1080 kg, Hegarty 2017; IIF 1633 kg, a Boeing technical-specifications page), matching this session's own per-SVN reads (SVN50, SVN63) exactly — and states explicitly that Block I/II/IIA's own individually-varying `FLGA92` masses are NOT incorporated into this SINEX block, which is why those three blocks keep `RS14` as primary (§3) rather than switching too. |
