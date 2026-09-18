#!/usr/bin/env python3
"""constraint8.py — keep D7's migration one line.

Plan §5 constraint 8.  The owner chose C++20 with a vendored `tl::expected` over
moving to C++23, on the ground that C++20 → C++23 later is two CMake lines and
deleting a shim, where the reverse would mean hunting every C++23 feature that
had crept in over months.

That migration is near-free **only while the tree stays inside the part of the
API where `tl::expected` and `std::expected` are interchangeable**: construction,
checking, unwrapping.  The monadic operations are where the two diverge.  So the
constraint is:

  1. Every signature names `odl::Result`, never the underlying type.
  2. No `and_then`, `or_else`, `transform` or `transform_error`, ever.

A constraint that depends on everyone remembering it is a constraint that
expires quietly, and this one expires in a way nobody notices until the day the
migration is attempted and turns out not to be one line after all.  So it is
checked, and CI runs the check.

Usage:  constraint8.py [--root DIR]
Exit:   0 clean   1 violation
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

OK, VIOLATION = 0, 1

# The one file allowed to name the underlying type: the alias itself.
ALIAS_HEADER = Path("modules/core/include/odl/core/result.hpp")

UNDERLYING = re.compile(r"\btl::(expected|unexpected)\b")
# Method-call syntax, so free functions such as std::ranges::transform and
# std::transform do not match -- those are unrelated and perfectly fine.
MONADIC = re.compile(r"\.\s*(and_then|or_else|transform_error|transform)\s*\(")

SCAN_SUFFIXES = {".hpp", ".cpp", ".h", ".cc", ".ipp"}


def sources(root: Path) -> list[Path]:
    out = []
    for base in ("modules", "tests"):
        d = root / base
        if not d.is_dir():
            continue
        out += [p for p in sorted(d.rglob("*")) if p.suffix in SCAN_SUFFIXES]
    return out


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="constraint8.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    args = ap.parse_args(argv)
    root = args.root.resolve()

    violations: list[str] = []
    scanned = 0

    for path in sources(root):
        scanned += 1
        rel = path.relative_to(root)
        text = path.read_text(encoding="utf-8", errors="replace")
        for n, line in enumerate(text.splitlines(), 1):
            stripped = line.lstrip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue          # prose about the rule is not a breach of it
            if rel != ALIAS_HEADER and UNDERLYING.search(line):
                violations.append(
                    f"{rel}:{n}: names the underlying type\n"
                    f"    {line.strip()}\n"
                    f"    Constraint 8.1: every signature names odl::Result. Only\n"
                    f"    {ALIAS_HEADER} may name tl::expected.")
            m = MONADIC.search(line)
            if m:
                violations.append(
                    f"{rel}:{n}: monadic operation '{m.group(1)}'\n"
                    f"    {line.strip()}\n"
                    f"    Constraint 8.2: construction, checking and unwrapping are where\n"
                    f"    tl::expected and std::expected are interchangeable; the monadic\n"
                    f"    operations are where they diverge. Using one forfeits the one-line\n"
                    f"    migration D7 was chosen for. Write it with an if and an early return.")

    for v in violations:
        print(f"VIOLATION  {v}\n", file=sys.stderr)

    if violations:
        print(f"{len(violations)} violation(s) of plan §5 constraint 8 in {scanned} files",
              file=sys.stderr)
        return VIOLATION
    print(f"ok       {scanned} files, no constraint-8 violation "
          f"(no monadic use, underlying type named only in the alias header)")
    return OK


if __name__ == "__main__":
    sys.exit(main())
