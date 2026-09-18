#!/usr/bin/env python3
"""test_configure_is_current.py — a stale configure tests a subset and reports 100%.

The manager's first check of L2 step 3 reported "100% tests passed, 165 of 165"
from a build directory that had never been configured with `tides`, `relativity`
or `thirdbody` in the tree.  A fresh configure gives 194.  That is the unstated
denominator again — a count of passed tests with no statement of how many there
should be — and it is the one that would have certified a step on a suite that
did not contain it.

WHY THIS IS A TEST AND NOT A LINE IN ci.sh.  The first attempt put the check in
`tools/ci.sh` just before ctest.  It could never fire: ci.sh CONFIGURES at gate 3
and then tests at gate 5, so by the time the check ran the build system was
always newer than the CMakeLists.  Dead code that reads like protection.  The
failure mode is somebody running ctest — or a single test binary — against an old
build directory, and the only place that can be caught is inside the suite.
"""

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    build = Path(sys.argv[1]) if len(sys.argv) > 1 else None
    if build is None or not build.exists():
        print(f"no build directory given or it does not exist: {build}", file=sys.stderr)
        return 1
    generated = build / "build.ninja"
    if not generated.exists():
        generated = build / "Makefile"
    if not generated.exists():
        print(f"neither build.ninja nor Makefile in {build}", file=sys.stderr)
        return 1

    stamp = generated.stat().st_mtime
    lists = sorted(p for p in ROOT.rglob("CMakeLists.txt")
                   if "build" not in p.parts and "_deps" not in p.parts)
    newer = [p for p in lists if p.stat().st_mtime > stamp]

    print(f"{len(lists)} CMakeLists.txt files, checked against {generated.name}")
    if newer:
        print("STALE CONFIGURE — this suite may be a subset of the tree.", file=sys.stderr)
        for p in newer:
            print(f"  newer than the build system: {p.relative_to(ROOT)}", file=sys.stderr)
        print("  A build directory configured before a module existed tests every OTHER\n"
              "  module and reports 100%. Re-run cmake.", file=sys.stderr)
        return 1
    print("ok       the configure is current; the suite is the whole tree")
    return 0


if __name__ == "__main__":
    sys.exit(main())
