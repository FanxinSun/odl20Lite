#!/usr/bin/env python3
"""unitcheck.py — the register of every factor of a thousand in production code.

SPEC-dynamics DYN-R-040, DYN-A-010.  Plan §3.4 step 1 asks for a gate that fails
on a km<->m scaling written outside `core/units.hpp`.

THE GATE AS THE PLAN WORDED IT WOULD FIRE FOURTEEN TIMES AND CATCH NOTHING, and
that was measured before this tool was written rather than after it went green.
Enumerating every 1e3-family literal in production sources found FIFTEEN, in
seven files, of which exactly ONE is the crossing -- and it is the definition in
`core/units.hpp` that the gate must permit.  The other fourteen are g/cm^3 ->
kg/m^3, a YYDDD radix, two "no longitude" sentinels, eight milliarcsecond and
millisecond conversions in `eop`, a mas -> rad constant in `gravity`, and a
row-count threshold in `tides`.  Fourteen false positives to one true positive,
before L4 adds a dozen forces with their own constants.

SO THIS IS A REGISTER, NOT A SEARCH.  Every such literal is either in
`core/units.hpp` or carries a marker on its own line saying what it is.  The gate
prints the whole register and fails on one that carries neither.

WHY NOT NARROW THE SEARCH INSTEAD.  Excluding `eop`, or requiring the literal to
sit near a position-like identifier, would work today.  This tree has watched
that approach fail three times: a licence DENYLIST that passed CeCILL because it
contained no forbidden substring; `test_one_secular_pole.py`, which matched a
bare `55.0`, was narrowed to exclude tests, matched a bare `55.0` again in
production, and was narrowed again.  A SEARCH CONVERGES ON FLAGGING NOTHING; A
REGISTER CONVERGES ON ACCOUNTING FOR EVERYTHING.  This is the licence ALLOWLIST's
shape: adding an entry is a deliberate act with a reason attached.

TWO MARKERS, NOT ONE, and the second came out of re-deriving the fourteen rather
than trusting the enumeration that produced them:

    // UNIT-CROSSING: <from> -> <to>          a genuine unit conversion
    // NOT-A-UNIT-CROSSING: <what it is>      a factor of 1000 that converts nothing

The marker goes ON THE LINE, or on the line IMMEDIATELY above it -- one line of
lookback and no more.  Same-line alone would force a 145-character line on the
one site where two literals share a long condition; a window of several lines
would let an annotation drift onto a literal it was never written for, which is
the attribution failure this register exists to avoid.

Four of the fourteen convert nothing at all -- a date radix, two sentinels and a
row count -- and labelling those as conversions would have made the register say
something false.  A reader's use of this register is to scan it and see that no
km<->m crossing hides among the entries; one wrong label turns "fourteen
unexamined literals" into "fourteen literals someone said were fine", which is
worse than no register.

LITERALS ARE MATCHED BY VALUE, NOT BY SPELLING.  `1e3`, `1.0E+3`, `1000.`,
`0.001` and `1.0e-03` are the same number differently written, and a spelling
list is a denylist wearing a different hat.
"""
from __future__ import annotations
import argparse, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CROSSING_HOME = "modules/core/include/odl/core/units.hpp"
MARKER = re.compile(r"//.*?\b(NOT-A-UNIT-CROSSING|UNIT-CROSSING)\s*:\s*(.+?)\s*$")
NUMBER = re.compile(r"(?<![\w.])(\d+\.?\d*(?:[eE][+-]?\d+)?)(?![\w.])")


def is_thousand(tok: str) -> bool:
    """Exactly 1000 or 1/1000, however it is spelt."""
    try:
        v = float(tok)
    except ValueError:
        return False
    return v == 1000.0 or v == 0.001


def production_sources() -> list[Path]:
    return sorted(p for p in (list((ROOT / "modules").rglob("*.hpp"))
                              + list((ROOT / "modules").rglob("*.cpp")))
                  if "build" not in p.parts and "tests" not in p.parts)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()

    srcs = production_sources()
    home, crossings, declared, unaccounted, misplaced = [], [], [], [], []

    for p in srcs:
        rel = str(p.relative_to(ROOT))
        lines = p.read_text(encoding="utf-8", errors="replace").split("\n")
        for lineno, line in enumerate(lines, 1):
            prev = lines[lineno - 2] if lineno >= 2 else ""
            stripped = line.lstrip()
            if stripped.startswith(("//", "*", "/*")):
                continue
            code = line.split("//")[0]
            found = [m.group(1) for m in NUMBER.finditer(code) if is_thousand(m.group(1))]
            if not found:
                continue
            mark = MARKER.search(line) or (MARKER.search(prev) if prev.lstrip().startswith("//") else None)
            for lit in found:
                row = (rel, lineno, lit, line.strip())
                if rel == CROSSING_HOME:
                    home.append(row)
                elif mark and mark.group(1) == "UNIT-CROSSING":
                    # AN ANNOTATION IS NOT A PERMIT.  Injecting a km<->m scaling and
                    # labelling it honestly would otherwise satisfy the gate while being
                    # exactly what the gate exists to prevent.  No conversion outside
                    # core/units.hpp may name kilometres: if kilometres are involved it IS
                    # the crossing, and the crossing has one site.
                    (misplaced if re.search(r"\bkm\b|kilomet", mark.group(2), re.I)
                     else crossings).append(row + (mark.group(2),))
                elif mark:
                    declared.append(row + (mark.group(2),))
                else:
                    unaccounted.append(row)

    if not a.quiet:
        print("THE REGISTER — every literal whose value is exactly 1000 or 0.001,")
        print("in production sources (tests excluded).\n")
        print(f"  the crossing itself, in {CROSSING_HOME}:")
        for rel, ln, lit, txt in home:
            print(f"    {ln:5d}  {lit:<8s} {txt[:80]}")
        print(f"\n  unit conversions, {len(crossings)}, each naming what it converts:")
        for rel, ln, lit, txt, what in crossings:
            print(f"    {rel}:{ln}  {lit:<8s} {what}")
        print(f"\n  not conversions at all, {len(declared)}:")
        for rel, ln, lit, txt, what in declared:
            print(f"    {rel}:{ln}  {lit:<8s} {what}")

    # THE DENOMINATOR.  The last number is the gate; the others make it legible.
    print(f"\n  production sources searched     {len(srcs):5d}")
    n = len(home) + len(crossings) + len(declared) + len(unaccounted) + len(misplaced)
    print(f"  literals of value 1000 or 1/1000 {n:5d}")
    print(f"    in {CROSSING_HOME:<44s} {len(home):5d}")
    print(f"    annotated UNIT-CROSSING                        {len(crossings):5d}")
    print(f"    annotated NOT-A-UNIT-CROSSING                  {len(declared):5d}")
    print(f"    ACCOUNTED FOR BY NEITHER                       {len(unaccounted):5d}")
    print(f"    naming km OUTSIDE core/units.hpp               {len(misplaced):5d}")

    if misplaced:
        print("\nA KM<->M CROSSING OUTSIDE core/units.hpp:", file=sys.stderr)
        for rel, ln, lit, txt, what in misplaced:
            print(f"  {rel}:{ln}  annotated '{what}'", file=sys.stderr)
        print(
            "\n  Annotating it does not make it legal. SPEC-frames FRAME-R-062 and\n"
            "  SPEC-dynamics DYN-R-010 give the km<->m crossing ONE site, and this is not it:\n"
            "  call metres_from_km / km_from_metres, or state_accel_km_s2_from_m_s2 and\n"
            "  field_position_m_from_state_km if what you are crossing is a state quantity.\n"
            "  One conversion site becomes six, and five of them are somebody's afternoon.",
            file=sys.stderr)
        return 1

    if unaccounted:
        print("\nUNACCOUNTED FACTOR OF A THOUSAND:", file=sys.stderr)
        for rel, ln, lit, txt in unaccounted:
            print(f"  {rel}:{ln}  [{lit}]  {txt[:90]}", file=sys.stderr)
        print(
            "\n  Every factor of a thousand in production code is either the km<->m crossing\n"
            "  in core/units.hpp, or says what it is. Add ONE of:\n"
            "      // UNIT-CROSSING: <from> -> <to>\n"
            "      // NOT-A-UNIT-CROSSING: <what it is>\n"
            "  on that line or the one directly above it. If it IS a km<->m scaling, it does not belong here at all:\n"
            "  call core/units.hpp, which is what SPEC-frames FRAME-R-062 and SPEC-dynamics\n"
            "  DYN-R-010 require and what this gate exists to keep true.", file=sys.stderr)
        return 1

    print("\nok       every factor of a thousand in production code is accounted for")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
