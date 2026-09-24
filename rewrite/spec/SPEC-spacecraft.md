# SPEC-spacecraft — the macromodel library, as cited data

| | |
|---|---|
| **Spec ID** | `SPCR` |
| **Status** | **draft** 2026-09-24, for review |
| **Version** | 1.1 — δ/ρ mapping corrected by formula (§3), IIF built (`SPCR-R-004`), IIIA searched and refused (`SPCR-R-007`) |
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
| `IGSMETA` | Steigenberger, P., Montenbruck, O. (maintainers); IGS | *IGS Satellite Metadata (SINEX)* | continuously updated; this pin 2026-09-24 | `https://files.igs.org/pub/station/general/igs_satellite_metadata.snx` | **primary**, open with attribution (IGS's own open data policy, re-checked this session for the redistribution bar, not only retrievability) | which physical block each frozen baseline SVN is (already used, `PROVENANCE.md` §30.1); a future per-satellite mass/power refinement, not consumed by this version |

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
- **Mass is per-block, not per-SVN**, in this version: `RS14`'s own tables give one mass for
  Block I (which itself varies by SVN, §4), one for II, one for IIA, one for IIR. A future,
  per-satellite refinement from `IGSMETA`'s own mass field is named in §10, not built here.

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
  own six bus faces and solar panel, mass 1100 kg. Cited `"RS14 Table 5.4, averaged from FLGA96"`
  per value.
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

  Mass 1555 kg, `RS14` Table 5.5's own caption, cross-checked in aggregate two ways (§6
  `SPCR-P-1`): against a U.S. Space Force fact sheet's own 3439 lb = 1559.7 kg (agrees to 0.3%,
  recorded before this round) and against `IGSMETA`'s own `SATELLITE/MASS` field for SVN63 (1633
  kg, agrees to ~5%, the gap consistent with SINEX's own field being launch mass against `RS14`'s
  own on-orbit/dry figure — not a contradiction, added this round). A second aggregate check — a
  published panel-span figure, independently verified — was sought and **not completed**: no
  directly-fetched, citable source was found (a 43.1 ft / 13.11 m figure appeared only in a search
  engine's own synthesized summary, never independently confirmed at a primary source, and is not
  used; a specific lead, "USA-66," was found to be a different, unrelated satellite — USA-NNN and
  NAVSTAR-NNN are separate numbering systems — and discarded before use, not carried into any
  citation). Recorded here as an incomplete check, not silently dropped or filled with an unverified
  number.
- **SPCR-R-005.** `gps_block_iir_m() -> Result<Macromodel, SpacecraftError>` returns
  **`gps_block_iir()`'s own geometry**, mass unchanged at 1100 kg (no IIR-M-specific mass is
  published in any source this spec holds; stated as assumed, not measured) — basis: `MSGA15`
  states IIR-A/IIR-B/IIR-M are distinguished specifically by "different phase center locations"
  (the antenna, `MSGA15` Table 3's own separate IIR-A / IIR-B-M rows), grouping all three under one
  body-frame figure (`MSGA15` Fig. 4's own caption, "IIR/IIR-M") — an inference that the
  BUS/PANEL geometry this schema holds is shared, stated as an inference from what IS enumerated as
  different, not a direct "IIR-M's bus equals IIR's" sentence in the source. Every value this
  function returns carries a citation stating exactly this — `"= gps_block_iir(), per MSGA15's own
  IIR/IIR-M grouping and its own phase-center-only distinction; not independently measured for
  IIR-M"` — so a reader sees the inference, not a bare number.
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
  for area, three for each optical coefficient) and are not rounded further here. Its own
  cross-checkable figure, GPS-IIF's mass, 1555 kg (`RS14` Table 5.5, `gps_block_iif`'s own
  `SPCR-R-004`), is now the value this spec DOES use, and carries two independent aggregate
  cross-checks: a U.S. Space Force fact sheet's own 3439 lb = **1559.7 kg** (within 0.3%, found
  before this round) and `IGSMETA`'s own `SATELLITE/MASS` field for SVN63/G063 = **1633 kg**
  (within ~5%, added this round) — the two disagree with EACH OTHER by more than either does with
  `RS14`, consistent with the fact sheet describing an on-orbit/dry-mass-like figure and `IGSMETA`'s
  own field describing launch mass (which includes expendables `RS14`'s own figure does not); both
  are recorded rather than one silently preferred. A third aggregate check, GPS-IIF's own panel
  span against a published figure, was sought and not completed — no independently-verified source
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
| `SPCR-A-004` | `gps_block_iif` builds from `RS14` Table 5.5 (mass, and every bus face/panel value, including the pure-absorber −Z row, α=1.000); area citation traces to "an unpublished document", optics citation is marked ASSUMED — the two citations checked to actually differ, proving the split is real, not cosmetic; panel `specular > diffuse` (same physical cross-check as `SPCR-A-003`) | agreement to `RS14` Table 5.5's own printed precision; citations differ; specular > diffuse | `RS14` Table 5.5, read directly in the test | exact (transcription) | R-004 |
| `SPCR-A-005` | `gps_block_i` refuses `SPCR-F-001` on an SVN outside the eight named, and does **not** refuse on each of the eight | the diagnostic; success on all eight | `SPCR-R-001`'s own domain | — | F-001 |
| `SPCR-A-006` | `gps_block_iir_m()`'s own surfaces equal `gps_block_iir()`'s own, field by field, value AND citation — the citation naming the inheritance, not silently identical by coincidence | identical values; citation states the inheritance | `SPCR-R-005` | exact | R-005 |
| `SPCR-A-007` | `gps_block_iiia` refuses, unconditionally, with `SPCR-F-003` | the refusal | `SPCR-R-007` | — | F-003, R-007 |

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
  pass, not only the corrected end state; the IIF aggregate cross-checks, mass agreeing two ways
  (a fact sheet to 0.3%, `IGSMETA` to ~5%, the gap explained) and a panel-span check sought and not
  completed; the Block IIIA one-search absence; the IIR-M geometry-sharing inference and its own
  textual basis in `MSGA15`.

---

## 10. Open questions for the manager

| id | question |
|---|---|
| `SPCR-Q-001` | **Block IIF — RESOLVED** (manager, 2026-09-24). Ruled: build from `RS14` Table 5.5 directly (it publishes per-surface values itself) rather than the imagery-derivation fallback `../plan/PLAN.md` §3.6 names — dimensions cited to `RS14`'s own stated chain-end ("an unpublished document"), optics marked ASSUMED (`RS14`'s own generic Ziebart (2001) assumption, not a new analogue chosen from outside the source). Built, `SPCR-R-004`; the imagery-derivation route was not needed and remains unused. |
| `SPCR-Q-002` | **Per-satellite mass from `IGSMETA`.** §3 notes this as a future refinement, not built in this version — worth doing before L7 reads this library, or acceptable at block-level granularity for the box-wing fit's own purposes? (This version DID add one `IGSMETA` read, `SPCR-R-004`'s own IIF mass cross-check — a plausibility check, not the general per-satellite refinement this question is about, which remains open.) |
