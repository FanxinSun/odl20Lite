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
SEARCHED = [p for p in (list((ROOT / "modules").rglob("*.hpp"))
                        + list((ROOT / "modules").rglob("*.cpp")))
            if "build" not in p.parts and "tests" not in p.parts]


def main() -> int:
    failures = []
    for name, pattern in CONSTANTS.items():
        where = sorted(str(p.relative_to(ROOT)) for p in SEARCHED
                       if re.search(pattern, p.read_text(encoding="utf-8")))
        elsewhere = [w for w in where if w != HOME]
        if where != [HOME]:
            failures.append(f"{name}: expected only {HOME}, also found in {elsewhere}")
        print(f"ok       {name:<8} appears only in {HOME}"
              if not elsewhere else f"FAILED   {name}: also in {', '.join(elsewhere)}")

    print(f"\n{len(SEARCHED)} module sources searched for the four constants of TN36-7 (21)")
    if failures:
        print("\n".join(failures), file=sys.stderr)
        print("\nThe secular pole is defined once, in gravity, and consumed by the pole tide "
              "(GRAV-R-029, PERT-R-006). A second copy is how the two drift apart.",
              file=sys.stderr)
        return 1
    print("the secular pole has exactly one definition")
    return 0


if __name__ == "__main__":
    sys.exit(main())
