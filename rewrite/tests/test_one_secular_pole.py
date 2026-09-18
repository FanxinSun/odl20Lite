#!/usr/bin/env python3
"""test_one_secular_pole.py — SPEC-perturbations PERT-A-009, the half of it that
is a property of the SOURCE rather than of a running program.

GRAV-R-029 and the manager's binding condition on GRAV-Q-005: the secular pole
is defined ONCE, in `gravity`, and L2 step 3's pole tide CONSUMES that definition
rather than restating it.  *Two definitions is how the static field ends up
secular and the pole tide ends up mean.*

The runtime half — that perturbing gravity's definition moves the pole tide — is
in the C++ tests.  This is the other half: the four constants of TN36-7 (21)
appear in exactly one file.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
HOME = "modules/gravity/include/odl/gravity/secular_pole.hpp"
# TN36-7 (21): xs = 55.0 + 1.677 (t - 2000), ys = 320.5 + 3.460 (t - 2000), mas.
CONSTANTS = {"55.0": r"\b55\.0\b", "1.677": r"\b1\.677\b", "320.5": r"\b320\.5\b",
             "3.460": r"\b3\.460\b"}
# PRODUCTION SOURCES ONLY.  A test that asserts the four constants is the
# opposite of a second definition — GRAV-A-028 does exactly that, on purpose —
# and `relativity_tests.cpp` happens to use 55.0 as an orbital inclination.
# The rule is about code that COMPUTES the pole, not about code that checks it.
#
# THIS GUARD HAS NOW BEEN NARROWED TWICE AND IS BEING REPLACED INSTEAD OF
# NARROWED A THIRD TIME.  The history matters more than either instance:
#
#   1. It matched a bare `55.0` anywhere.  `relativity_tests.cpp` used 55.0 as an
#      inclination, so the test directory was excluded.
#   2. It matched a bare `55.0` in production.  `msis_thermosphere.hpp` carries
#      it as NRLMSISE-00's ZN2 mesospheric spline node — 55 km.  So it was
#      narrowed to require TWO of the four constants in one file.
#
# Each narrowing was locally right and the trend is not: A GUARD THAT ANSWERS
# EVERY FALSE POSITIVE BY LOWERING ITS OWN SENSITIVITY CONVERGES ON A GUARD THAT
# FIRES AT NOTHING.  55.0 and 320.5 are ordinary numbers and a big enough tree
# will eventually put two of them in one file by accident too.
#
# So the DISCRIMINATOR changes rather than the threshold.  TN36-7 (21) is two
# expressions —  xs = 55.0 + 1.677 (t - 2000),  ys = 320.5 + 3.460 (t - 2000) —
# and a copy of either puts its offset and its rate NEXT TO EACH OTHER.  A
# coincidence scatters them: the spline node and the inclination are nowhere near
# anything else on this list.  Adjacency is the property that actually separates
# the two, it costs no more to test, and it does not get weaker as the tree fills
# with numbers.
WINDOW = 2          # lines either side; a wrapped expression stays inside this
MIN_TOGETHER = 2    # two of the four adjacent is an equation, not an accident

SEARCHED = [p for p in (list((ROOT / "modules").rglob("*.hpp"))
                        + list((ROOT / "modules").rglob("*.cpp")))
            if "build" not in p.parts and "tests" not in p.parts]


def adjacent_hits(text: str) -> list[tuple[int, list[str]]]:
    """Windows of the file carrying two or more of the four constants."""
    lines = text.split("\n")
    per_line = [[n for n, pat in CONSTANTS.items() if re.search(pat, ln)] for ln in lines]
    out = []
    for i in range(len(lines)):
        lo, hi = max(0, i - WINDOW), min(len(lines), i + WINDOW + 1)
        near: list[str] = []
        for j in range(lo, hi):
            for n in per_line[j]:
                if n not in near:
                    near.append(n)
        if len(near) >= MIN_TOGETHER and (not out or out[-1][1] != near):
            out.append((i + 1, near))
    return out


def main() -> int:
    carries: dict[str, list[str]] = {}
    adjacent: dict[str, list[tuple[int, list[str]]]] = {}
    for p in SEARCHED:
        text = p.read_text(encoding="utf-8")
        found = [n for n, pat in CONSTANTS.items() if re.search(pat, text)]
        if found:
            rel = str(p.relative_to(ROOT))
            carries[rel] = found
            hits = adjacent_hits(text)
            if hits:
                adjacent[rel] = hits

    # the components, printed rather than a verdict
    for name in CONSTANTS:
        where = sorted(f for f, got in carries.items() if name in got)
        others = [f for f in where if f != HOME]
        note = "" if not others else f"   (also, scattered, in {', '.join(others)})"
        print(f"ok       {name:<8} in {HOME}{note}"
              if HOME in where else f"MISSING  {name} is not in {HOME}")

    if carries.get(HOME, []) != list(CONSTANTS):
        print(f"\n{HOME} does not carry all four constants", file=sys.stderr)
        return 1
    if HOME not in adjacent:
        print(f"\n{HOME} does not put any pair of them within {WINDOW} lines, so this test "
              "is no longer looking at the definition it was written for", file=sys.stderr)
        return 1

    copies = {f: h for f, h in adjacent.items() if f != HOME}
    scattered = len(carries) - 1
    print(f"\n{len(SEARCHED)} module sources searched. {scattered} carry one or more of the "
          f"four constants incidentally and scattered; {len(copies)} put {MIN_TOGETHER} or "
          f"more within {WINDOW} lines, which is what a restatement of TN36-7 (21) looks like.")
    if copies:
        for f, hits in sorted(copies.items()):
            for line, got in hits:
                print(f"FAILED   {f}:{line} has {', '.join(got)} within {WINDOW} lines",
                      file=sys.stderr)
        print("\nThe secular pole is defined once, in gravity, and consumed by the pole tide "
              "(GRAV-R-029, PERT-R-006). A second copy is how the two drift apart.",
              file=sys.stderr)
        return 1
    print("the secular pole has exactly one definition")
    return 0


if __name__ == "__main__":
    sys.exit(main())
