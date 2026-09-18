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


def emit(rows: list[list[float]], name: str, unit: str, what: str) -> str:
    out = [f"// {what}  ({len(rows)} constituents, {unit})",
           f"inline constexpr TideTerm {name}[] = {{"]
    for r in rows:
        args = ", ".join(f"{int(a):3d}" for a in r[0:6])
        c = ", ".join(f"{x:9.4f}" for x in r[7:11])
        out.append(f"    {{{{{args}}}, {r[6]:12.7f}, {c}}},")
    out.append("};")
    return "\n".join(out)


def main() -> int:
    ap = argparse.ArgumentParser(prog="tides_from_conventions.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ch5", type=Path, required=True)
    ap.add_argument("--ch8", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    a = ap.parse_args()

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
