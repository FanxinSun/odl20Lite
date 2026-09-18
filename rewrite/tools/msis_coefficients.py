#!/usr/bin/env python3
"""msis_coefficients.py — extract NRLMSISE-00's fitted coefficients as generated source.

SPEC-atmosphere ATMO-R-001/ATMO-R-002.  The model IS ~1500 fitted coefficients
plus the code combining them, and both exist only in the pinned FORTRAN (plan §4
rule 6).  The coefficients are therefore EXTRACTED from the pinned bytes, never
transcribed: there are 3 300 of them and hand-copying would be its own defect
source, exactly as PERT-R-014 reasoned about chapter 6's tide tables.

Extraction is NOT part of the build.  It runs against the manifest cache and its
output is committed, so a build reproduces without re-parsing anything.

THE STORAGE IS FORTRAN COMMON ALIASING, which is the one subtle thing here.
BLOCK DATA GTD7BK declares COMMON/PARM7/ as sixty-four 50-element arrays
PT1..PAA2 (3 200 doubles).  Every subroutine declares the SAME common block as
PT(150), PD(150,9), PS(150), PDL(25,2), PTL(100,4), PMA(100,10), SAM(100) —
150 + 1350 + 150 + 50 + 400 + 1000 + 100 = 3 200.  So the DATA statements fill a
flat block that the code reads through a completely different set of names, and
an extractor that took the DATA names at face value would produce arrays the
model never indexes.  The concatenation order is the DECLARATION order, and the
two-dimensional views are COLUMN-MAJOR.
"""
from __future__ import annotations
import argparse, hashlib, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FOR_ID = "nrlmsise00-fortran"

# COMMON/PARM7/ as BLOCK DATA declares it, in order.  3 200 doubles.
PARM7 = ([f"PT{i}" for i in (1,2,3)] + [f"P{c}{i}" for c in "ABCDEFGHIJ" for i in (1,2,3)]
         + ["PK1"] + [f"P{c}{i}" for c in "LMNOPQRSUVWXYZ" for i in (1,2)] + ["PAA1","PAA2"])
# how the subroutines view that same block
VIEWS = [("pt", 150, None), ("pd", 150, 9), ("ps", 150, None), ("pdl", 25, 2),
         ("ptl", 100, 4), ("pma", 100, 10), ("sam", 100, None)]
FLAT = [("PTM", 10, None), ("PDM", 10, 8), ("PAVGM", 10, None)]

NUM = re.compile(r"[-+]?\d*\.?\d+(?:[EeDd][-+]?\d+)?")

def read_data(text: str, name: str) -> list[float]:
    m = re.search(rf"^      DATA {name}/", text, re.M)
    if not m:
        sys.exit(f"no DATA statement for {name}")
    out, i = [], m.end()
    body = text[i:]
    end = body.index("/")
    for tok in NUM.findall(body[:end]):
        out.append(float(tok.replace("D", "E").replace("d", "e")))
    return out

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="modules/atmosphere/src/msis_coefficients.hpp")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    doc = json.loads((ROOT / "manifest" / "manifest.json").read_text())
    e = next(x for x in doc["entries"] if x["id"] == FOR_ID)
    src = ROOT / doc["cache"] / e["id"] / e["filename"]
    sha = hashlib.sha256(src.read_bytes()).hexdigest()
    if sha != e["sha256"]:
        sys.exit(f"{FOR_ID}: cached bytes are not the pinned bytes")
    text = src.read_text(errors="replace")

    flat: list[float] = []
    for nm in PARM7:
        v = read_data(text, nm)
        if len(v) != 50:
            sys.exit(f"{nm}: expected 50 values, parsed {len(v)}")
        flat += v
    if len(flat) != 3200:
        sys.exit(f"COMMON/PARM7/ came to {len(flat)}, not 3200")

    blocks, off = {}, 0
    for nm, n, m in VIEWS:
        take = n * (m or 1)
        blocks[nm] = (flat[off:off + take], n, m); off += take
    assert off == 3200
    for nm, n, m in FLAT:
        v = read_data(text, nm); take = n * (m or 1)
        if len(v) != take: sys.exit(f"{nm}: expected {take}, parsed {len(v)}")
        blocks[nm.lower()] = (v, n, m)

    text_out = emit(sha, blocks, len(PARM7))
    out = ROOT / a.out
    if a.check:
        if not out.exists() or out.read_text() != text_out:
            print("REGENERATED COEFFICIENTS DIFFER from the committed file", file=sys.stderr)
            return 1
        print(f"ok  {a.out} reproduces from {FOR_ID}")
        return 0
    out.write_text(text_out)
    total = sum(len(v) for v, _, _ in blocks.values())
    print(f"wrote {a.out}: {total} coefficients "
          f"({len(PARM7)} DATA arrays -> 3200 flat -> {len(VIEWS)} views, plus {len(FLAT)} tables)")
    return 0

def emit(sha: str, blocks: dict, ndata: int) -> str:
    L = []; w = L.append
    w("#pragma once")
    w("// msis_coefficients.hpp — GENERATED.  Do not edit.")
    w("//")
    w("// Produced by tools/msis_coefficients.py from the hash-pinned NRL reference")
    w(f"// implementation NRLMSISE-00.FOR, sha256 {sha}")
    w("//")
    w("// NRLMSISE-00 has no published closed form: the model IS these coefficients")
    w("// together with the code that combines them, which is why plan §4 rule 6 calls")
    w("// the reference normative rather than an oracle.  They are EXTRACTED rather than")
    w("// transcribed because there are 3 300 of them.")
    w("//")
    w(f"// BLOCK DATA GTD7BK fills COMMON/PARM7/ through {ndata} fifty-element DATA arrays")
    w("// PT1..PAA2.  Every subroutine reads that same storage through DIFFERENT names —")
    w("// pt(150), pd(150,9), ps(150), pdl(25,2), ptl(100,4), pma(100,10), sam(100),")
    w("// which is 3200 again.  The views below are that aliasing made explicit; the")
    w("// two-dimensional ones are COLUMN-MAJOR as FORTRAN stores them, so pd[j][i] here")
    w("// is PD(i+1, j+1) there.")
    w("")
    w("#include <array>")
    w("")
    w("namespace odl::atmosphere::coeff {")
    w("")
    for nm, n, m in [(k, v[1], v[2]) for k, v in blocks.items()]:
        vals, _, _ = blocks[nm]
        if m is None:
            w(f"inline constexpr std::array<double, {n}> k{nm.upper()} = {{")
            w(fmt(vals)); w("};"); w("")
        else:
            w(f"inline constexpr std::array<std::array<double, {n}>, {m}> k{nm.upper()} = {{{{")
            for j in range(m):
                col = vals[j*n:(j+1)*n]
                w("    {")
                w(fmt(col, indent=8))
                w("    },")
            w("}};"); w("")
    w("}  // namespace odl::atmosphere::coeff")
    return "\n".join(L) + "\n"

def fmt(vals: list[float], indent: int = 4) -> str:
    pad, out, row = " " * indent, [], []
    for v in vals:
        row.append(f"{v: .6e}")
        if len(row) == 5:
            out.append(pad + ", ".join(row) + ","); row = []
    if row: out.append(pad + ", ".join(row) + ",")
    return "\n".join(out)

if __name__ == "__main__":
    raise SystemExit(main())
