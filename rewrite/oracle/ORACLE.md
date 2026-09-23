# The oracle — what the predecessor says, frozen

**Captured 2026-09-18 from predecessor commit `a9782ab`.** Plan rule R2, made operational.

This directory is the predecessor's entire role in this project. It is a program that is **run
and never read**, and the intent is that it has now been run to exhaustion: every number the
rewrite is measured against is in `cases.tsv`, so nobody should ever need to touch it again. An
oracle that has already answered every question cannot tempt anyone into opening it.

| file | what it is |
|---|---|
| `cases.tsv` | 27 frozen cases: id, quantity, value, unit, and the command or derivation |
| `environment.txt` | predecessor commit, compiler, host, and a SHA-256 for every input file |
| `capture.sh` | the runner that produced both, so the capture is reproducible rather than asserted |

---

## 1. Who may use this, and how

**Running the predecessor is permitted to anyone, including a session that has never seen its
source.** Observing what a program outputs is not derived from its expression — that is plan
rule R4, and it is why comparing against it is legitimate at all.

**Reading its source is forbidden** to anyone writing specifications or implementation (R1, R2).

In practice nobody should need to do either, because the capture is already done. If a new case
is genuinely needed, add it to `capture.sh`, run it, and append — do not go looking inside.

**One asymmetry worth stating, because it decided who built this.** R1 had to be executed by a
session that had never read the predecessor; that is what made the specifications clean. R2 is
its mirror: it requires *operating* the predecessor, which the clean session must not do. The
two rules therefore partition the work by who has read what, and neither session could have done
the other's job.

## 2. What is here, and what deliberately is not

`cases.tsv` holds extracted scalar values with the command that produced each. It holds **no raw
program output**. Logs carry configuration echoes, internal file names and diagnostic text; none
of that is a measurement, and copying it would move expression rather than observation.

The same line is why the sweep-derived cases (`B-*`, `E-*`) cite a source file by SHA-256 rather
than reproducing it: plan R6 forbids any file crossing from the predecessor's `analysis/` or
`res/`, while R4 permits logged observations. **A number is a fact about what a program did; a
file is an artefact somebody made.** Numbers cross; files do not.

## 3. Every number is conditional on the inputs

`environment.txt` hashes nine input files and every SP3 arc. This matters more than it looks:
plan rule R11 exists because the IERS revises Earth-orientation data retroactively at an
unchanged URL, so "the same input" is not a stable idea unless it is hashed. The oracle values
below were produced against `res/eopc04` at `1c121252…`, and against a run of that predecessor
which reports itself dirty only because of untracked user documents in its working tree — no
tracked source differs from `a9782ab`.

## 4. THE INTERPRETATION CAVEAT — read before comparing anything

The predecessor computes with **IAU-76/1980** precession-nutation and consumes the **IAU 1980**
variants of the IERS products. The rewrite uses **IAU 2006/2000A** and the IAU 2000A variants.

**Certain disagreements are therefore required, of a predictable size, and agreement would be the
failure.** The specification captures this as `FRAME-A-009`: assert a disagreement of a predicted
size. Concretely, the accumulated IAU-76 precession rate error is ≈ 3 mas/yr, which over the
~26 years since J2000 is ≈ 0.064 arcsec, or **2.17 m at 7000 km** — essentially the whole of case
`T-01`. A rewrite that reproduced `T-01` to the millimetre would have silently reimplemented the
older model, and no "matches the oracle" test can catch that.

Consequently, and this is the standing rule from `SPEC-template.md`: **oracle comparison ranks
last among acceptance-value sources and is never a gate on its own.** Its job is to catch gross
error — a sign, an axis, a factor of two — not to certify correctness. Published worked examples
(Vallado's ITRS↔TEME case; the IERS reference-routine test values) are the gates.

## 5. A trap this capture uncovered: the same result, two statistics, 1.5× apart

The flagship result is that a fit told nothing about hardware recovers the GPS block structure
from estimated radiation-pressure coefficients alone. The plan recorded its strength as
"9.6–13.9σ". Recomputing it from the raw sweep gave **11.3–20.8σ** — the same data, no error on
either side.

The difference is the **denominator**:

| definition | formula | IIF vs IIR-A | IIF vs IIIA |
|---|---|---|---|
| **pooled SD** — an effect size: how far apart are the block means relative to the spread *within* blocks | \|m₁−m₂\| / √(((n₁−1)s₁² + (n₂−1)s₂²)/(n₁+n₂−2)) | **14.1σ** | **9.8σ** |
| **standard error of the means** — a t-statistic: how confident are we the means differ at all | \|m₁−m₂\| / √(s₁²/n₁ + s₂²/n₂) | **20.8σ** | **11.3σ** |

Both are correct; they answer different questions. The historically recorded figure is the
**pooled-SD** one, and `cases.tsv` carries both with their formulas so the rewrite's acceptance
test can name which it means.

This is the second time in two days that an unstated denominator produced a defensible-looking
wrong number — the first was own-prefix versus all-prefix identifiers in the specification
coverage check, 121 against 128. **A statistic without its definition is not a measurement**, and
both instances were small enough to read as rounding disagreements rather than as errors.

## 6. What the cases cover

- **`F-*` frames** — ECEF→ECI components at a frozen epoch, round-trip closure, a present-day
  epoch. Ballpark and negative-control use only; see §4. Note that closure depends on how many
  digits of the epoch are supplied: `1.29e-7 km` was recorded at `…37458333333333` and `8.55e-8 km`
  at `…37458333`. Both are far inside the 1e-6 km target; the sensitivity is ≈ 0.1 mm and is
  itself worth knowing.
- **`T-*` TEME** — the SGP4 frame-conversion check against a JPL Horizons table. Bounded above by
  the ~3 m definitional floor of TEME itself, and the comparison ephemeris is *not* independent:
  Horizons' ephemeris for that object forward of a TLE epoch **is that TLE**.
- **`G-*` GNSS** — three seven-parameter fit residuals against IGS final orbits. The strongest
  cases here, because IGS orbits are independent truth rather than predecessor output.
  *Recorded 2026-09-24:* the fits are **cannonball** fits — `validate_sp3.sh` asserts that the
  run used a template's single `area` and `mass`, and the seventh parameter is the SRP scale, the
  same `A·C_R/m` convention the `B-*` block means below state. No surface model and no attitude
  law entered them. So these residuals constrain a cannonball fit; a box-wing fit reaching them
  is a different and stronger claim, and a comparison of like with like uses a cannonball.
- **`S-*` LightSail-2** — laser-ranging fit residual and recovered effective A·C_R/m.
- **`O-*` ACS3 optical** — angular residual from amateur astrometry.
- **`D-*` atmosphere** — the NRLMSISE-00 density envelope over one 6-hour LEO arc. Appended
  2026-09-18 after the initial freeze, against verified-unchanged inputs, because the redrafted
  plan's drag gate needed a frozen number rather than a remembered one; `capture.sh` gained the
  block, so the script remains the generator.
- **`B-*` block recovery** and **`E-*` ECOM** — derived from recorded sweeps, too expensive to
  re-run; source files cited by hash, not copied.

Block means are stated in the **corrected** convention, A·C_R/m — the recorded sweep column is
scale × A/m, which understates it by C_R = 1.1444 because the model's reflectivity coefficient
sits inside the estimated quantity. The block *separations* are ratios and are unaffected.

## 7. Re-running this

`oracle/capture.sh` regenerates `cases.tsv` and `environment.txt` in about ten minutes. It needs
the predecessor built (`make rebuild` in its tree) and its IGS, ILRS and optical data present;
it fetches nothing itself. The `B-*` and `E-*` rows are appended separately from the recorded
sweeps and are not regenerated by the script.

If a re-run disagrees with the frozen values, that is a finding about the predecessor's inputs —
almost certainly the Earth-orientation data under R11 — and not a licence to overwrite the frozen
set. Freeze the new values alongside, with their own environment hashes, and record why.
