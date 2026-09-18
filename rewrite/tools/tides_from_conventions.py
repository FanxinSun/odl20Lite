#!/usr/bin/env python3
"""tides_from_conventions.py — the tidal EOP coefficients, out of the document.

SPEC-eop.md EOP-R-007.  The IERS Conventions publish these models both as
printed tables and as Fortran routines.  The Fortran carries NO LICENCE AT ALL,
so there is nothing for plan §5 constraint 3 to be satisfied by; the tables are
the content of a standard, published so that they can be implemented.  This tree
therefore implements from the tables.

Hand transcription of ~1400 numbers would be its own defect source, so the tables
are extracted from the hash-pinned PDFs by this script and the result is COMMITTED
as generated source.  The extraction is not part of the build: pdftotext output
varies with the poppler version, and a build that re-ran it would not be
reproducible.  What the build checks instead is EOP-R-008 — that the coefficients
reproduce the routines' own published test cases, which is a check on the numbers
rather than on the pipeline that produced them.

Row shape, common to all six tables and the reason a single parser handles them:
each row ends with

    ... <6 integer multipliers> <Doodson> <period/days> <c1> <c2> <c3> <c4>

so the fields are taken FROM THE END.  That skips the optional tide-name column
(Q1, 2Q1, χ1 …) and Table 5.1's extra leading degree column without having to
model either.

Usage:  tides_from_conventions.py --ch5 icc5.pdf --ch8 icc8.pdf --out <header>
"""

from __future__ import annotations

import argparse
import hashlib
import re
import subprocess
import sys
from pathlib import Path

MINUS = "−"          # the PDF uses U+2212, not ASCII hyphen
NUMERIC = re.compile(r"^[+-]?(\d+\.?\d*|\.\d+)$")


def pdftotext(pdf: Path) -> list[str]:
    r = subprocess.run(["pdftotext", "-layout", str(pdf), "-"],
                       capture_output=True, text=True, check=True)
    return r.stdout.replace(MINUS, "-").splitlines()


def sha256(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def table_rows(lines: list[str], start_marker: str, stop_markers: list[str]) -> list[list[float]]:
    """Every row between the table caption and the next caption, taken from the end."""
    try:
        i = next(n for n, l in enumerate(lines) if l.strip().startswith(start_marker))
    except StopIteration:
        raise SystemExit(f"table {start_marker!r} not found")
    rows: list[list[float]] = []
    for line in lines[i + 1:]:
        s = line.strip()
        if any(s.startswith(m) for m in stop_markers):
            break
        toks = [t for t in s.split() if NUMERIC.match(t)]
        if len(toks) < 12:
            continue
        tail = toks[-12:]
        try:
            args = [int(float(x)) for x in tail[0:6]]
            doodson = tail[6]
            period = float(tail[7])
            coeffs = [float(x) for x in tail[8:12]]
        except ValueError:
            continue
        if "." not in doodson or period <= 0.0:
            continue                       # a stray line, not a constituent
        rows.append([float(a) for a in args] + [period] + coeffs)
    return rows


# --------------------------------------------------------------------------- #
# Chapter 6, Tables 6.5a/b/c — the solid Earth tide frequency corrections.
#
# THESE DO NOT SHARE A SHAPE, WITH EACH OTHER OR WITH CHAPTERS 5 AND 8, and the
# differences are not cosmetic:
#
#   6.5a  Name deg/hr Doodson  <6 Doodson> <5 Delaunay>  dkR dkI  ip op
#   6.5b  Name Doodson deg/hr  <6 Doodson> <5 Delaunay>  dkR ip   dkI op   <- interleaved
#   6.5c  Name Doodson deg/hr  <6 Doodson> <5 Delaunay>  dkR ip             <- real part only
#
# and 6.5a's caption says "The entries for dkR and dkI are in units of 10^-5"
# where 6.5b's and 6.5c's are absolute.  A parser that assumed one shape would
# read 6.5b's in-phase amplitude as an imaginary Love number and be wrong by
# five orders of magnitude without failing.  So each table carries its own
# explicit tail, and the values are emitted AS PRINTED with the scale named
# separately, so that the generated header can be diffed against the PDF.
#
# The Doodson number ("125,755") carries a comma and is not a NUMERIC token, so
# it drops out of the token stream in all three.  That is relied on, and the
# expected token count per table is asserted rather than inferred.
CH6_TABLES = {
    "kSolidTideDiurnal": {
        "start": "Table 6.5a:", "stop": ["Table 6.5b:"],
        "tail": ("dk_real", "dk_imag", "amp_ip", "amp_op"),
        "dk_scale": 1e-5, "band": 1,
        "what": "Table 6.5a - diurnal (m = 1) corrections for the frequency dependence of k21",
    },
    "kSolidTideZonal": {
        "start": "Table 6.5b:", "stop": ["Table 6.5c:"],
        "tail": ("dk_real", "amp_ip", "dk_imag", "amp_op"),
        "dk_scale": 1.0, "band": 0,
        "what": "Table 6.5b - zonal (m = 0) corrections for the frequency dependence of k20",
    },
    "kSolidTideSemidiurnal": {
        "start": "Table 6.5c:", "stop": ["6.2.2"],
        "tail": ("dk_real", "amp_ip"),
        "dk_scale": 1.0, "band": 2,
        "what": "Table 6.5c - semidiurnal (m = 2) corrections for k22; the real part only",
    },
}


def ch6_rows(lines: list[str], spec: dict) -> list[dict]:
    want = 1 + 6 + 5 + len(spec["tail"])          # deg/hr, Doodson multipliers, Delaunay, tail
    try:
        i = next(n for n, l in enumerate(lines) if l.strip().startswith(spec["start"]))
    except StopIteration:
        raise SystemExit(f"table {spec['start']!r} not found")
    rows: list[dict] = []
    for line in lines[i + 1:]:
        s = line.strip()
        if any(s.startswith(m) for m in spec["stop"]):
            break
        toks = [x for x in s.split() if NUMERIC.match(x)]
        if len(toks) != want:
            continue                              # a caption, a header, a page number
        v = [float(x) for x in toks]
        if not (0.0 < v[0] < 40.0):
            continue                              # deg/hr for every tide in these bands
        if any(abs(x) > 20 or x != int(x) for x in v[1:12]):
            continue                              # the multipliers are small integers
        row = {"deg_per_hour": v[0],
               "doodson": [int(x) for x in v[1:7]],
               "delaunay": [int(x) for x in v[7:12]]}
        row.update(dict(zip(spec["tail"], v[12:])))
        row.setdefault("dk_imag", 0.0)
        row.setdefault("amp_op", 0.0)
        rows.append(row)
    return rows


# The amplitudes are printed to one decimal in units of 1e-12, so a half-ulp of
# 0.05 propagates into the relation as 0.05*(|dkI| + |dkR|).  The first version
# of this check compared a bare ratio and reported "worst 1.000 relative",
# which is what you get when a printed 0.0 meets a non-zero partner: an
# artefact of the table's own precision, read as a disagreement.
AMPLITUDE_HALF_ULP = 0.05


def ch6_consistency(rows: list[dict], name: str) -> str:
    """Amp(ip) * dkI == Amp(op) * dkR, because both amplitudes are the same
    A_m H_f times their own part of dk.  It checks that the columns were read in
    the right order and needs no H_f.

    A row where the printed precision swamps both products constrains nothing —
    it is satisfied by any column order — so those are COUNTED SEPARATELY rather
    than folded into a pass."""
    worst, worst_row = 0.0, None
    constraining = applicable = 0
    for r in rows:
        if r["dk_imag"] == 0.0 and r["amp_op"] == 0.0:
            continue                               # the table has no such column
        applicable += 1
        residual = r["amp_ip"] * r["dk_imag"] - r["amp_op"] * r["dk_real"]
        bound = AMPLITUDE_HALF_ULP * (abs(r["dk_imag"]) + abs(r["dk_real"]))
        terms = max(abs(r["amp_ip"] * r["dk_imag"]), abs(r["amp_op"] * r["dk_real"]))
        if terms <= bound:
            continue                               # constrains nothing
        constraining += 1
        d = abs(residual) / bound if bound else 0.0
        if d > worst:
            worst, worst_row = d, r
    if applicable == 0:
        return f"{name}: the table has no imaginary column; the relation does not apply"
    return (f"{name}: {constraining} of {applicable} rows constrain it "
            f"({applicable - constraining} lost to the printed precision), "
            f"worst residual {worst:.2f} of its rounding bound"
            + (f" at {worst_row['deg_per_hour']} deg/hr" if worst_row else ""))


def emit_ch6(rows: list[dict], name: str, spec: dict) -> str:
    out = [f"// {spec['what']}",
           f"//   {len(rows)} constituents; dk as printed (scale {spec['dk_scale']:g}), "
           f"amplitudes as printed (scale 1e-12)",
           f"inline constexpr SolidTideTerm {name}[] = {{"]
    for r in rows:
        d = ", ".join(f"{x:3d}" for x in r["doodson"])
        l = ", ".join(f"{x:3d}" for x in r["delaunay"])
        out.append(f"    {{{r['deg_per_hour']:11.5f}, {{{d}}}, {{{l}}}, "
                   f"{r['dk_real']:12.5f}, {r['dk_imag']:12.5f}, "
                   f"{r['amp_ip']:8.1f}, {r['amp_op']:8.1f}}},")
    out.append("};")
    return "\n".join(out)


def emit(rows: list[list[float]], name: str, unit: str, what: str) -> str:
    out = [f"// {what}  ({len(rows)} constituents, {unit})",
           f"inline constexpr TideTerm {name}[] = {{"]
    for r in rows:
        args = ", ".join(f"{int(a):3d}" for a in r[0:6])
        c = ", ".join(f"{x:9.4f}" for x in r[7:11])
        out.append(f"    {{{{{args}}}, {r[6]:12.7f}, {c}}},")
    out.append("};")
    return "\n".join(out)


def write_ch6(pdf: Path, out: Path) -> None:
    lines = pdftotext(pdf)
    tables = {name: ch6_rows(lines, spec) for name, spec in CH6_TABLES.items()}
    for name, rows in tables.items():
        print(f"extracted {name}: {len(rows)} constituents", file=sys.stderr)
        print("  " + ch6_consistency(rows, name), file=sys.stderr)
    # The counts are asserted, not reported: a shape change upstream that
    # silently dropped half a table would otherwise pass as "extracted 35".
    # Counted from the PDF, not guessed: 48 + 21 + 2 = 71 constituents in all.
    expect = {"kSolidTideDiurnal": 48, "kSolidTideZonal": 21, "kSolidTideSemidiurnal": 2}
    for name, n in expect.items():
        if len(tables[name]) != n:
            raise SystemExit(
                f"{name}: extracted {len(tables[name])} constituents, expected {n}.\n"
                f"  The table's shape in the PDF has changed, or pdftotext laid it out\n"
                f"  differently. Re-read Tables 6.5a/b/c before touching this number:\n"
                f"  the three have three different column orders and 6.5a's dk is in\n"
                f"  units of 1e-5 where the other two are absolute.")

    text = f"""#pragma once
// solid_tide_tables.hpp — GENERATED.  Do not edit.
//
// Produced by tools/tides_from_conventions.py --ch6 from the hash-pinned IERS
// Conventions (2010) chapter 6:  sha256 {sha256(pdf)}
//
// SPEC-perturbations PERT-R-014: Step 2's frequency-dependent corrections are
// implemented from the tables PRINTED IN THE CONVENTIONS.  Several hundred
// numbers is its own defect source by hand, and the IERS Fortran carries no
// licence at all, so neither route is open.  Extraction is NOT part of the
// build: pdftotext's layout varies with the poppler version and a build that
// re-ran it would not be reproducible.
//
// VALUES ARE AS PRINTED so that this file can be diffed against the PDF.  The
// scales are named below and applied once, in the module.
//
// The tail columns differ between the three tables and 6.5a's Love-number
// corrections are in units of 1e-5 where 6.5b's and 6.5c's are absolute; see
// CH6_TABLES in the generator.

#include <array>

namespace odl::tides::tables {{

struct SolidTideTerm {{
    double deg_per_hour;
    std::array<int, 6> doodson;    // tau, s, h, p, N', ps
    std::array<int, 5> delaunay;   // l, l', F, D, Omega
    double dk_real;                // AS PRINTED
    double dk_imag;                // AS PRINTED; zero where the table has no such column
    double amp_ip;                 // AS PRINTED, units of 1e-12
    double amp_op;                 // AS PRINTED, units of 1e-12; zero where absent
}};

/// TN36-6 Table 6.5a prints dk in units of 1e-5.  Tables 6.5b and 6.5c print it
/// absolute.  One number per table, applied once, named here.
inline constexpr double kDkScaleDiurnal = {CH6_TABLES['kSolidTideDiurnal']['dk_scale']:g};
inline constexpr double kDkScaleZonal = {CH6_TABLES['kSolidTideZonal']['dk_scale']:g};
inline constexpr double kDkScaleSemidiurnal = {CH6_TABLES['kSolidTideSemidiurnal']['dk_scale']:g};
/// Every amplitude column in all three tables is in units of 1e-12.
inline constexpr double kAmplitudeScale = 1e-12;

{emit_ch6(tables["kSolidTideDiurnal"], "kSolidTideDiurnal", CH6_TABLES["kSolidTideDiurnal"])}

{emit_ch6(tables["kSolidTideZonal"], "kSolidTideZonal", CH6_TABLES["kSolidTideZonal"])}

{emit_ch6(tables["kSolidTideSemidiurnal"], "kSolidTideSemidiurnal", CH6_TABLES["kSolidTideSemidiurnal"])}

}}  // namespace odl::tides::tables
"""
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text, encoding="utf-8")
    print(f"wrote {out} ({sum(len(v) for v in tables.values())} constituents)", file=sys.stderr)


def main() -> int:
    ap = argparse.ArgumentParser(prog="tides_from_conventions.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ch5", type=Path, required=True)
    ap.add_argument("--ch8", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--ch6", type=Path, default=None,
                    help="IERS Conventions chapter 6, for Tables 6.5a/b/c")
    ap.add_argument("--out-ch6", type=Path, default=None,
                    help="where to write the solid Earth tide tables")
    a = ap.parse_args()

    if bool(a.ch6) != bool(a.out_ch6):
        raise SystemExit("--ch6 and --out-ch6 go together")
    if a.ch6:
        write_ch6(a.ch6, a.out_ch6)

    c5, c8 = pdftotext(a.ch5), pdftotext(a.ch8)

    tables = {
        "kPoleOceanDiurnal": table_rows(c8, "Table 8.2a:", ["Table 8.2b:"]),
        "kPoleOceanSemidiurnal": table_rows(c8, "Table 8.2b:", ["Table 8.3a:"]),
        "kUt1OceanDiurnal": table_rows(c8, "Table 8.3a:", ["Table 8.3b:"]),
        "kUt1OceanSemidiurnal": table_rows(c8, "Table 8.3b:", ["Table 8.4", "8.3.2", "Table 8.5"]),
        "kPoleLibration": table_rows(c5, "Table 5.1a:", ["Table 5.1b:", "5.5.2"]),
        "kUt1Libration": table_rows(c5, "Table 5.1b:", ["5.5.4", "Table 5.2"]),
    }

    counts = {k: len(v) for k, v in tables.items()}
    print("extracted:", counts, file=sys.stderr)

    header = f"""#pragma once
// tide_tables.hpp — GENERATED.  Do not edit.
//
// Produced by tools/tides_from_conventions.py from the hash-pinned IERS
// Conventions (2010) chapters in the manifest:
//
//   chapter 5  sha256 {sha256(a.ch5)}
//   chapter 8  sha256 {sha256(a.ch8)}
//
// SPEC-eop.md EOP-R-007: the models are implemented from the tables PRINTED IN
// THE CONVENTIONS, not from the IERS Fortran, which carries no licence.
// EOP-R-008: every table here is verified against that routine's own published
// test case, and a transcription that fails its test case is a build failure.
//
// The six multipliers are of (gamma, l, l', F, D, Omega), where gamma = GMST + pi.

#include <array>

namespace odl::eop::tides {{

struct TideTerm {{
    std::array<int, 6> arg;
    double period_days;   // carried as DATA, not a comment: Table 5.1a is filtered
                          // to its near-diurnal rows and the filter needs it
    double sin1, cos1, sin2, cos2;
}};

{emit(tables["kPoleOceanDiurnal"], "kPoleOceanDiurnal", "microarcseconds",
      "Table 8.2a - diurnal ocean-tide variations in pole coordinates (xp sin, xp cos, yp sin, yp cos)")}

{emit(tables["kPoleOceanSemidiurnal"], "kPoleOceanSemidiurnal", "microarcseconds",
      "Table 8.2b - semidiurnal ocean-tide variations in pole coordinates")}

{emit(tables["kUt1OceanDiurnal"], "kUt1OceanDiurnal", "microseconds / microseconds per day",
      "Table 8.3a - diurnal ocean-tide variations in UT1 and LOD (UT1 sin, UT1 cos, LOD sin, LOD cos)")}

{emit(tables["kUt1OceanSemidiurnal"], "kUt1OceanSemidiurnal", "microseconds / microseconds per day",
      "Table 8.3b - semidiurnal ocean-tide variations in UT1 and LOD")}

{emit(tables["kPoleLibration"], "kPoleLibration", "microarcseconds",
      "Table 5.1a - libration in pole coordinates. ONLY the near-diurnal rows are applied: TN36 "
      "5.5.1.1 says the long-period terms are already in the observed series")}

{emit(tables["kUt1Libration"], "kUt1Libration", "microseconds / microseconds per day",
      "Table 5.1b - semidiurnal libration in UT1 and LOD")}

}}  // namespace odl::eop::tides
"""
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(header, encoding="utf-8")
    print(f"wrote {a.out} ({sum(counts.values())} constituents)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
