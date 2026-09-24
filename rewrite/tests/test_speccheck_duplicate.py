#!/usr/bin/env python3
"""test_speccheck_duplicate.py — does the duplicate-TEST_CASE checker catch
the defect that prompted it?

L5 step 4's own review round: two DIFFERENT TEST_CASEs, in two DIFFERENT
.cpp files, both claimed "SRPA-A-011" -- one a long-standing test, the other
a brand new one that assumed the number was free without checking. Nothing
in speccheck.py, or in Catch2 itself, caught it. This replays that shape as
a synthetic pair of files, the same "inject the historical error" discipline
`test_budgetcheck.py` already uses for a different checker.
"""

import subprocess
import sys
import tempfile
from pathlib import Path

TOOL = Path(__file__).resolve().parent.parent / "tools" / "speccheck.py"
OK, GAPS, UNPARSEABLE = 0, 1, 3

failures = []


def run(cpp_files: dict[str, str]) -> subprocess.CompletedProcess:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        for name, content in cpp_files.items():
            (root / name).write_text(content, encoding="utf-8")
        # An empty, valid spec directory: this test is entirely about the
        # cpp-side duplicate check, not spec traceability, so a minimal spec
        # keeps the spec-side gate trivially satisfied rather than testing
        # nothing by accident.
        spec_dir = root / "spec"
        spec_dir.mkdir()
        (spec_dir / "SPEC-synthetic.md").write_text(
            "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n", encoding="utf-8")
        return subprocess.run(
            [sys.executable, str(TOOL), "--spec-dir", str(spec_dir), "--cpp-root", str(root),
             "--quiet"],
            capture_output=True, text=True)


def check(name, got, want, extra=""):
    if got == want:
        print(f"ok       {name}")
    else:
        print(f"FAILED   {name}: expected {want}, got {got}\n{extra}", file=sys.stderr)
        failures.append(name)


def test_case(id_str: str, label: str) -> str:
    return f'TEST_CASE("{id_str}  {label}", "[synthetic]") {{ CHECK(true); }}\n'


def main() -> int:
    # --- THE HISTORICAL SHAPE: two different files, same id, unrelated tests --
    res = run({
        "a_tests.cpp": test_case("SRPA-A-011", "pure absorber at oblique incidence"),
        "b_tests.cpp": test_case("SRPA-A-011", "the general formula against 20 published vectors"),
    })
    check("CATCHES two different files claiming the same id", res.returncode, GAPS,
          res.stdout + res.stderr)
    check("  ... and names the id", "SRPA-A-011" in res.stdout + res.stderr, True,
          res.stdout + res.stderr)
    combined = res.stdout + res.stderr
    check("  ... and names both locations", "a_tests.cpp" in combined and "b_tests.cpp" in combined,
          True, combined)

    # --- the SAME defect, within ONE file (two TEST_CASEs, one file) --------
    res = run({
        "one_file_tests.cpp": (
            test_case("MCRM-A-099", "first claim") + test_case("MCRM-A-099", "second claim")),
    })
    check("CATCHES two TEST_CASEs in the SAME file claiming the same id", res.returncode, GAPS,
          res.stdout + res.stderr)

    # --- the corrected form must pass ---------------------------------------
    res = run({
        "a_tests.cpp": test_case("SRPA-A-014", "pure absorber at oblique incidence"),
        "b_tests.cpp": test_case("SRPA-A-015", "the general formula against 20 published vectors"),
    })
    check("accepts distinct ids", res.returncode, OK, res.stdout + res.stderr)

    # --- THE FAILURE MODE THIS TOOL MUST NOT HAVE ---------------------------
    # A tree with a genuine duplicate must not report success.
    res = run({
        "x_tests.cpp": test_case("EPH-A-001", "claim one") + test_case("EPH-A-001", "claim two"),
    })
    check("does not report success over an unresolved duplicate",
          "ok       no id is claimed by more than one TEST_CASE" not in res.stdout, True,
          res.stdout)

    # --- a legitimate split (lettered suffix) is NOT a duplicate ------------
    # This tree's own resolution for a genuinely related pair (EPH-A-001's
    # own short-kernel gate and its full-sweep companion, PHPR-A-003's own
    # success and refusal cases) is a lettered suffix -- a mechanically
    # distinct id, not an exception the checker has to reason about.
    res = run({
        "y_tests.cpp": test_case("EPH-A-001", "the gate") + test_case("EPH-A-001b", "the full sweep"),
    })
    check("a lettered-suffix split is NOT flagged as a duplicate", res.returncode, OK,
          res.stdout + res.stderr)

    if failures:
        print(f"\n{len(failures)} check(s) failed", file=sys.stderr)
        return 1
    print("\nall duplicate-TEST_CASE checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
