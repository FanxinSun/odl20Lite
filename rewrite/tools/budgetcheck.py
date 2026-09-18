#!/usr/bin/env python3
"""budgetcheck.py — evaluate the arithmetic in every specification budget row.

WHY THIS EXISTS.  On 2026-09-18 four of the twenty-three budget rows in this
tree were wrong — by factors of 1000, 1000, 1000 and 10^6 — and every one of them
had been read and adopted.  One was in a specification the manager had reviewed.
The rows are the only class of number in this project with no test behind them:
`tests/toolchain_smoke.cpp` had the same arithmetic right the whole time, because
code gets tested and prose does not.

Writing the multiplication out makes the error VISIBLE to a careful reader.
"Visible to a careful reader" is exactly the standard that failed four times in
one document.  This makes it CHECKABLE instead, which is the same move as
printing components rather than a verdict.

HOW IT AVOIDS BEING THE NEXT DISGUISE.  A checker that silently skipped rows it
could not parse would be worse than none: it would report success over the rows
it understood and say nothing about the rest.  So every budget row is classified,
and the third class is a failure:

  checked        contains `… = **result**`; the arithmetic was evaluated
  no-arithmetic  contains no such expression anywhere in the row
  UNPARSEABLE    contains one and it did not parse  ->  FAILURE

Row shape is not assumed.  The tables differ between specifications —
SPEC-frames has four columns where SPEC-time has five — so this scans the whole
row for expressions rather than trusting a column index.  Discovering that before
writing the parser is why it does not trust one.

Usage:  budgetcheck.py [--spec-dir DIR] [--quiet]
Exit:   0 all rows check   1 a row is wrong   2 a row would not parse
"""

from __future__ import annotations

import argparse
import math
import re
import sys
from pathlib import Path

OK, WRONG, UNPARSEABLE = 0, 1, 2

ARCSEC = math.pi / (180.0 * 3600.0)

# symbol -> (SI scale, dimension as (length, time, angle))
UNITS: dict[str, tuple[float, tuple[int, int, int]]] = {
    "m":   (1.0, (1, 0, 0)),      "km": (1e3, (1, 0, 0)),
    "mm":  (1e-3, (1, 0, 0)),     "um": (1e-6, (1, 0, 0)),
    "nm":  (1e-9, (1, 0, 0)),     "pm": (1e-12, (1, 0, 0)),
    "fm":  (1e-15, (1, 0, 0)),
    "AU":  (1.495978707e11, (1, 0, 0)),
    "s":   (1.0, (0, 1, 0)),      "ms": (1e-3, (0, 1, 0)),
    "us":  (1e-6, (0, 1, 0)),     "ns": (1e-9, (0, 1, 0)),
    "ps":  (1e-12, (0, 1, 0)),    "fs": (1e-15, (0, 1, 0)),
    "rad": (1.0, (0, 0, 1)),
    "as":  (ARCSEC, (0, 0, 1)),   "mas": (ARCSEC * 1e-3, (0, 0, 1)),
    "uas": (ARCSEC * 1e-6, (0, 0, 1)),
    "":    (1.0, (0, 0, 0)),      "1": (1.0, (0, 0, 0)),
}

SUPERSCRIPT = str.maketrans("⁻⁰¹²³⁴⁵⁶⁷⁸⁹", "-0123456789")


def normalise(text: str) -> str:
    """Unicode the specs are written in, into something parseable."""
    t = text
    t = t.replace("−", "-").replace("×", "*")
    t = t.replace("µ", "u").replace("μ", "u")          # both micro signs
    t = t.replace("–", "~").replace("—", " ")          # en dash = range; em dash = prose
    t = t.replace(" ", " ").replace(" ", " ").replace(" ", " ")
    # 10⁻¹³ -> 10^-13, s⁻¹ -> s^-1
    t = re.sub(r"([A-Za-z0-9])([⁻⁰¹²³⁴⁵⁶⁷⁸⁹]+)",
               lambda m: m.group(1) + "^" + m.group(2).translate(SUPERSCRIPT), t)
    # digit grouping: "1.495 978 707" -> "1.495978707"
    t = re.sub(r"(?<=\d) (?=\d)", "", t)
    # Scientific notation is written with the SAME multiplication sign as a
    # genuine factor — "1.495978707 * 10^11 m/AU" is one quantity, not two — so
    # it is folded into exponent form BEFORE the product is split on '*'.
    # Missing this made the parser read "10^-6 km" as the number 10 with a unit
    # of "^-6 km", which is the kind of quiet mis-parse this tool exists to
    # prevent, so it failed loudly on three rows rather than producing a wrong
    # comparison.
    t = re.sub(r"(\d+(?:\.\d+)?)\s*\*\s*10\^(-?\d+)", r"\1e\2", t)
    t = re.sub(r"(?<![\d.eE])10\^(-?\d+)", r"1e\1", t)
    return t


class Quantity:
    __slots__ = ("value", "dim")

    def __init__(self, value: float, dim: tuple[int, int, int]):
        self.value, self.dim = value, dim

    def __mul__(self, o: "Quantity") -> "Quantity":
        return Quantity(self.value * o.value, tuple(a + b for a, b in zip(self.dim, o.dim)))


class ParseError(Exception):
    pass


def parse_unit(u: str) -> Quantity:
    """`km s^-1`, `mm/uas`, `m/AU`, `mm`, ``."""
    u = u.strip()
    if not u:
        return Quantity(1.0, (0, 0, 0))
    num, _, den = u.partition("/")
    q = Quantity(1.0, (0, 0, 0))
    for part, sign in ((num, 1), (den, -1)):
        for tok in part.split():
            sym, _, exp = tok.partition("^")
            e = int(exp) if exp else 1
            if sym not in UNITS:
                raise ParseError(f"unknown unit {sym!r} in {u!r}")
            scale, dim = UNITS[sym]
            q = q * Quantity(scale ** (e * sign), tuple(d * e * sign for d in dim))
    return q


NUM = r"[-+]?\d+(?:\.\d+)?(?:\s*\*\s*10\^-?\d+|[eE][-+]?\d+)?"


def parse_number(s: str) -> float:
    s = s.strip().replace(" ", "")
    m = re.fullmatch(r"([-+]?\d+(?:\.\d+)?)\*10\^(-?\d+)", s)
    if m:
        return float(m.group(1)) * 10.0 ** int(m.group(2))
    m = re.fullmatch(r"10\^(-?\d+)", s)
    if m:
        return 10.0 ** int(m.group(1))
    return float(s)


def parse_term(t: str) -> tuple[list[float], Quantity]:
    """`10 uas`, `30~100 uas`, `1.11*10^-16 s`, `0.034 mm/uas`, `10^-13 AU`."""
    t = t.strip().lstrip("<≤≈~ ").strip()
    m = re.match(rf"^({NUM})(?:~({NUM}))?\s*(.*)$", t)
    if not m:
        raise ParseError(f"cannot read a quantity from {t!r}")
    values = [parse_number(m.group(1))]
    if m.group(2):
        values.append(parse_number(m.group(2)))
    return values, parse_unit(m.group(3))


def significant_tolerance(text: str, value: float) -> float:
    """Half an ulp of the last SIGNIFICANT digit of the stated value.

    The rows are rounded for reading, so the check must accept that rounding and
    nothing looser.  Counting decimal places instead of significant figures —
    which this did in its first draft — makes the tolerance absolute, so a row
    stating "8 x 10^-16 m" got a tolerance of 0.5 and an error of a THOUSAND
    passed.  That is the tool acquiring the defect it was built to catch, and it
    was caught by replaying the four historical errors against it: three failed
    as they should and the fourth did not.
    """
    t = text.replace(" ", "")
    m = re.search(r"(\d+(?:\.\d+)?)", t)
    mantissa = m.group(1) if m else "1"
    digits = mantissa.replace(".", "").lstrip("0") or "0"
    sig = max(len(digits), 1)
    if value == 0.0:
        return 0.5
    exponent = math.floor(math.log10(abs(value)))
    return 0.5 * 10.0 ** (exponent - (sig - 1)) * 1.001


EXPR = re.compile(r"([^|=]+?)\s*=\s*\*\*([^*]+)\*\*")


# A budget cell may open with prose before its arithmetic, and since
# SPEC-template.md §7 was amended on 2026-09-18 it often MUST: an acceleration
# budget that quotes a position consequence has to name the integration time and
# the spectral character of the error, and that is prose.
#
# What may be dropped is restricted to a prefix containing NO DIGIT.  Once a
# digit appears there is no way to tell prose from a factor, and dropping a
# factor silently is exactly the failure this tool exists to prevent — so a
# prefix with a digit in it is left where it is and the row fails loudly as
# UNPARSEABLE.  The prefix that is dropped is printed, because a tool that
# discards part of its input without saying so is the shape of defect this
# whole file is about.
PROSE = re.compile(r"^(?P<prose>[^\d]*?[A-Za-z][^\d]*?)(?P<rest>[-+]?\s*\d.*)$", re.S)


def strip_prose_prefix(lhs: str) -> tuple[str, str]:
    m = PROSE.match(lhs)
    if not m:
        return "", lhs
    return m.group("prose").strip(), m.group("rest").strip()


def check_row(rid: str, row: str, quiet: bool) -> tuple[str, list[str]]:
    problems: list[str] = []
    text = normalise(row)
    found = EXPR.findall(text)
    if not found:
        return "no-arithmetic", problems

    for lhs, rhs in found:
        lhs = lhs.strip().lstrip(";, ").strip()
        prose, lhs = strip_prose_prefix(lhs)
        if prose and not quiet:
            print(f"note     {rid:<14} prose prefix ignored: {prose!r}")
        try:
            factors = [parse_term(p) for p in lhs.split("*") if p.strip()]
            # A range on the left pairs with the range on the right, elementwise.
            n = max(len(v) for v, _ in factors)
            lefts = []
            for i in range(n):
                q = Quantity(1.0, (0, 0, 0))
                for vals, unit in factors:
                    q = q * Quantity(vals[i if len(vals) > 1 else 0], (0, 0, 0)) * unit
                lefts.append(q)
            rvals, runit = parse_term(rhs)
        except (ParseError, ValueError, IndexError) as exc:
            problems.append(f"{rid}: UNPARSEABLE {lhs!r} = {rhs!r}: {exc}")
            continue

        if len(rvals) != len(lefts):
            problems.append(f"{rid}: {len(lefts)} value(s) on the left, {len(rvals)} on the right")
            continue
        for left, rv in zip(lefts, rvals):
            tol = significant_tolerance(rhs, rv)
            right = Quantity(rv, (0, 0, 0)) * runit
            if left.dim != right.dim:
                problems.append(f"{rid}: dimensions differ — left {left.dim}, right {right.dim}"
                                f"  ({lhs} = {rhs})")
                continue
            stated_si = right.value
            # Compare in the units the row states, so the tolerance means what
            # the printed digits mean.
            unit_scale = runit.value
            got_in_units = left.value / unit_scale
            if abs(got_in_units - rv) > tol:
                problems.append(
                    f"{rid}: ARITHMETIC WRONG\n"
                    f"      stated   {rhs.strip()}\n"
                    f"      computed {got_in_units:.6g} (same units)\n"
                    f"      from     {lhs}\n"
                    f"      out by a factor of {got_in_units / rv:.4g}"
                    if rv else f"{rid}: stated zero")
            elif not quiet:
                print(f"ok       {rid:<14} {lhs} = {rv:g} ({got_in_units:.6g} computed)")
    return ("checked" if not problems else "problem"), problems


def main(argv: list[str] | None = None) -> int:
    root = Path(__file__).resolve().parent.parent
    ap = argparse.ArgumentParser(prog="budgetcheck.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--spec-dir", type=Path, default=root / "spec")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args(argv)

    rows = 0
    counts = {"checked": 0, "no-arithmetic": 0}
    all_problems: list[str] = []
    row_re = re.compile(r"^\|\s*`([A-Z][A-Z0-9]*-P-[0-9a-z]+)`\s*\|")

    for path in sorted(a.spec_dir.glob("SPEC-*.md")):
        for line in path.read_text(encoding="utf-8").splitlines():
            m = row_re.match(line)
            if not m:
                continue
            rows += 1
            kind, problems = check_row(m.group(1), line, a.quiet)
            all_problems += problems
            if kind in counts:
                counts[kind] += 1

    print(f"\n{rows} budget rows: {counts['checked']} with arithmetic, checked; "
          f"{counts['no-arithmetic']} with none.")
    if rows != counts["checked"] + counts["no-arithmetic"]:
        print(f"  {rows - counts['checked'] - counts['no-arithmetic']} row(s) neither — "
              f"see the failures below.")

    if all_problems:
        print(file=sys.stderr)
        for p in all_problems:
            print(f"FAILED   {p}", file=sys.stderr)
        print(f"\n{len(all_problems)} problem(s). A row that looks like arithmetic and does not "
              f"parse is a FAILURE, never a skip:\n"
              f"  a checker that silently skipped them would report success over the rows it\n"
              f"  understood and say nothing about the rest, which is the defect this tool\n"
              f"  exists to end rather than to join.", file=sys.stderr)
        return UNPARSEABLE if any("UNPARSEABLE" in p for p in all_problems) else WRONG

    print("ok       every budget row's arithmetic evaluates to what it states")
    return OK


if __name__ == "__main__":
    sys.exit(main())
