#!/usr/bin/env python3
"""rk_coefficients.py — RKF7(8)'s Butcher tableau, transcribed and then PROVED.

SPEC-integrators.  Plan §4 rule 6's converse: where an independent specification
exists -- and it need not be a document -- the artifact's legibility stops
mattering.  Ask what the thing must satisfy before asking how cleanly it prints.

THE SOURCE PRINTS EVERYTHING AND OCR DESTROYS ALL OF IT.  Fehlberg, NASA TR
R-287 (1968), Table X, is a scan.  `pdftotext` renders its beta block as

    83_ = 841 = B_I = 8sl = B71 = Be1 = B_l = 81ol = _m = _12_ = 0

and mangles even the contents page -- "TABLE IN." for III, "8O" for 80, the I/1
and O/0 confusions that matter most for digits.  THE PAGE IMAGES ARE PERFECTLY
LEGIBLE at 300 dpi, and the coefficients there are EXACT RATIONALS.

So the numbers below were read by eye from the page image.  That would be
unacceptable on its own, and it does not have to be acceptable on its own,
because a Runge-Kutta tableau has an independent specification: THE ORDER
CONDITIONS, which are exact algebraic identities over the rationals.  A misread
digit fails them.  The question is not whether a 1968 scan can be read reliably;
it is whether what was read is checkable, and it is, completely.

WHAT THIS TOOL PROVES, in exact rational arithmetic and never in floating point:

  * row-sum consistency, sum_j beta_ij = alpha_i, on all 13 rows;
  * c satisfies all 85 order conditions through order 7;
  * chat satisfies all 200 through order 8;
  * c VIOLATES exactly 40 of the 115 order-8 conditions -- which is what makes
    the pair an estimator rather than two names for one method.

That last number is an INDEPENDENT CROSS-CHECK on the transcription that does not
touch the coefficient digits at all.  There are 115 rooted trees of order 8, which
is exactly the range of Fehlberg's error coefficients T_v (v = 1...115), and his
Section XV says in prose: "our formula RK7(8) contains only 40 non-zero error
coefficients T_v".  Forty is what this tool counts.  Prose in one part of the
report and mathematics applied to a table in another agree, and neither is the
scan's rendering of the digits.
"""
from __future__ import annotations
import argparse, sys
from fractions import Fraction as F
from functools import lru_cache
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE_ID = "fehlberg-tr-r-287"
SOURCE_SHA = "5553a2a3eb53785a461762cc2b29428015f1b32c3ad0a5cb57f85a421256a0c8"

# TABLE X, RK 7(8), report page 65 (PDF page 72), read at 300 dpi.
ALPHA = [F(0),F(2,27),F(1,9),F(1,6),F(5,12),F(1,2),F(5,6),F(1,6),F(2,3),F(1,3),F(1),F(0),F(1)]
BETA = [
    [],
    [F(2,27)],
    [F(1,36),F(1,12)],
    [F(1,24),F(0),F(1,8)],
    [F(5,12),F(0),F(-25,16),F(25,16)],
    [F(1,20),F(0),F(0),F(1,4),F(1,5)],
    [F(-25,108),F(0),F(0),F(125,108),F(-65,27),F(125,54)],
    [F(31,300),F(0),F(0),F(0),F(61,225),F(-2,9),F(13,900)],
    [F(2),F(0),F(0),F(-53,6),F(704,45),F(-107,9),F(67,90),F(3)],
    [F(-91,108),F(0),F(0),F(23,108),F(-976,135),F(311,54),F(-19,60),F(17,6),F(-1,12)],
    [F(2383,4100),F(0),F(0),F(-341,164),F(4496,1025),F(-301,82),F(2133,4100),F(45,82),
     F(45,164),F(18,41)],
    [F(3,205),F(0),F(0),F(0),F(0),F(-6,41),F(-3,205),F(-3,41),F(3,41),F(6,41),F(0)],
    [F(-1777,4100),F(0),F(0),F(-341,164),F(4496,1025),F(-289,82),F(2193,4100),F(51,82),
     F(33,164),F(12,41),F(0),F(1)],
]
C    = [F(41,840),F(0),F(0),F(0),F(0),F(34,105),F(9,35),F(9,35),F(9,280),F(9,280),
        F(41,840),F(0),F(0)]
CHAT = [F(0),F(0),F(0),F(0),F(0),F(34,105),F(9,35),F(9,35),F(9,280),F(9,280),
        F(0),F(41,840),F(41,840)]
S = len(ALPHA)

# Fehlberg's own count, printed in Section XV (p.66) and used here as a check on
# the table on p.65 -- two different pages, one of them prose.
FEHLBERG_NONZERO_T = 40


def beta(i: int, j: int) -> F:
    return BETA[i][j] if j < len(BETA[i]) else F(0)


@lru_cache(maxsize=None)
def trees(n: int):
    """Rooted trees of order n, canonicalised. One order condition per tree."""
    if n == 1:
        return ((),)
    out = set()
    def parts(total: int, mx: int):
        if total == 0:
            yield (); return
        for k in range(min(total, mx), 0, -1):
            for t in trees(k):
                for rest in parts(total - k, k):
                    yield tuple(sorted((t,) + rest, key=repr))
    for p in parts(n - 1, n - 1):
        out.add(p)
    return tuple(sorted(out, key=repr))


@lru_cache(maxsize=None)
def order(t) -> int:
    return 1 + sum(order(s) for s in t)


@lru_cache(maxsize=None)
def gamma(t) -> F:
    g = F(order(t))
    for s in t:
        g *= gamma(s)
    return g


@lru_cache(maxsize=None)
def phi(t):
    if not t:
        return tuple([F(1)] * S)
    sub = [phi(s) for s in t]
    out = []
    for i in range(S):
        p = F(1)
        for v in sub:
            p *= sum(beta(i, j) * v[j] for j in range(i))
        out.append(p)
    return tuple(out)


def violations(weights, p: int) -> list:
    bad = []
    for n in range(1, p + 1):
        for t in trees(n):
            lhs = sum(w * v for w, v in zip(weights, phi(t)))
            if lhs != F(1) / gamma(t):
                bad.append((n, t))
    return bad


def verify(verbose: bool = True) -> dict:
    rows_ok = sum(1 for i in range(S) if sum(BETA[i]) == ALPHA[i])
    if rows_ok != S:
        sys.exit("row-sum consistency fails: the transcription is wrong")
    v7 = violations(C, 7)
    v8 = violations(CHAT, 8)
    v_c8 = [x for x in violations(C, 8) if x[0] == 8]
    n7 = sum(len(trees(n)) for n in range(1, 8))
    n8 = sum(len(trees(n)) for n in range(1, 9))
    if v7:  sys.exit(f"c violates {len(v7)} order conditions through order 7")
    if v8:  sys.exit(f"chat violates {len(v8)} order conditions through order 8")
    if len(v_c8) != FEHLBERG_NONZERO_T:
        sys.exit(f"c violates {len(v_c8)} order-8 conditions; Fehlberg's Section XV prints "
                 f"{FEHLBERG_NONZERO_T} non-zero error coefficients. These must agree.")
    r = {"rows": S, "c_trees": n7, "chat_trees": n8, "order8_trees": len(trees(8)),
         "c_order8_violations": len(v_c8)}
    if verbose:
        print(f"  row-sum consistency          {rows_ok}/{S} rows, exactly")
        print(f"  c    through order 7         {n7} trees, 0 violated")
        print(f"  chat through order 8         {n8} trees, 0 violated")
        print(f"  c    at order 8              {len(v_c8)} of {len(trees(8))} violated")
        print(f"  Fehlberg's printed count     {FEHLBERG_NONZERO_T} non-zero T_v  -> AGREES")
    return r


def emit(stats: dict) -> str:
    L = []; w = L.append
    w("#pragma once")
    w("// rkf78_coefficients.hpp — GENERATED.  Do not edit.")
    w("//")
    w("// Produced by tools/rk_coefficients.py from Fehlberg, NASA TR R-287 (1968),")
    w(f"// Table X (report p.65), sha256 {SOURCE_SHA}")
    w("//")
    w("// READ FROM THE PAGE IMAGE, NOT THE TEXT LAYER.  The report is a 1968 scan and")
    w("// pdftotext returns noise for its mathematics; the images at 300 dpi are exact")
    w("// and give the coefficients as RATIONALS.  See the manifest entry's verify_note.")
    w("//")
    w("// AND THE TRANSCRIPTION IS PROVED, NOT TRUSTED.  In exact rational arithmetic:")
    w(f"//   row-sum consistency   {stats['rows']}/{stats['rows']} rows, sum_j beta_ij = alpha_i")
    w(f"//   c    through order 7  {stats['c_trees']} order conditions, 0 violated")
    w(f"//   chat through order 8  {stats['chat_trees']} order conditions, 0 violated")
    w(f"//   c    at order 8       {stats['c_order8_violations']} of {stats['order8_trees']} violated")
    w("//")
    w("// That last line is an INDEPENDENT CROSS-CHECK that never touches the digits:")
    w(f"// there are {stats['order8_trees']} rooted trees of order 8, exactly the range of")
    w("// Fehlberg's error coefficients T_v, and his Section XV says in PROSE that RK7(8)")
    w(f"// has \"only {FEHLBERG_NONZERO_T} non-zero error coefficients T_v\".  Prose on one page and")
    w("// mathematics applied to a table on another agree.")
    w("//")
    w("// THE ESTIMATOR IS IDENTICALLY ZERO ON A QUADRATURE PROBLEM.  alpha_0 = alpha_11 = 0")
    w("// and alpha_10 = alpha_12 = 1, and the truncation term (134) is")
    w("//     TE = (41/840)(f0 + f10 - f11 - f12) h,")
    w("// so for any right-hand side depending on x alone the four evaluations cancel in")
    w("// pairs and the estimate is not small but ZERO.  See SPEC-integrators; the")
    w("// controller is blind there, not merely optimistic.")
    w("")
    w("#include <array>")
    w("")
    w("namespace odl::integrators::rkf78 {")
    w("")
    w(f"inline constexpr int kStages = {S};")
    w("")
    w("inline constexpr std::array<double, kStages> kAlpha = {")
    w("    " + ", ".join(f"{float(a):.17g}" for a in ALPHA))
    w("};")
    w("")
    w("/// Row-major, lower triangular; entries above the diagonal are zero.")
    w(f"inline constexpr std::array<std::array<double, kStages>, kStages> kBeta = {{{{")
    for i in range(S):
        row = [beta(i, j) for j in range(S)]
        w("    {" + ", ".join(f"{float(x):.17g}" for x in row) + "},")
    w("}};")
    w("")
    w("/// The propagating 7th-order weights.")
    w("inline constexpr std::array<double, kStages> kC = {")
    w("    " + ", ".join(f"{float(x):.17g}" for x in C))
    w("};")
    w("")
    w("/// The 8th-order companion, used only for the error estimate.")
    w("inline constexpr std::array<double, kStages> kCHat = {")
    w("    " + ", ".join(f"{float(x):.17g}" for x in CHAT))
    w("};")
    w("")
    w("/// (134): TE = kErrorWeight * (f0 + f10 - f11 - f12) * h")
    w(f"inline constexpr double kErrorWeight = {float(F(41,840)):.17g};")
    w(f"inline constexpr int kOrder = 7;          ///< the order that PROPAGATES")
    w(f"inline constexpr int kEstimatorOrder = 8;")
    w(f"inline constexpr int kOrder8Violations = {stats['c_order8_violations']};")
    w("")
    w("}  // namespace odl::integrators::rkf78")
    return "\n".join(L) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="modules/integrators/include/odl/integrators/rkf78_coefficients.hpp")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    stats = verify(verbose=not a.check)
    text = emit(stats)
    out = ROOT / a.out
    if a.check:
        if not out.exists() or out.read_text() != text:
            print("REGENERATED COEFFICIENTS DIFFER from the committed file", file=sys.stderr)
            return 1
        print(f"ok  {a.out} reproduces, and its order conditions hold exactly")
        return 0
    out.write_text(text)
    print(f"wrote {a.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
