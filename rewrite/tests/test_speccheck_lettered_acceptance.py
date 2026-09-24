#!/usr/bin/env python3
"""test_speccheck_lettered_acceptance.py — does speccheck.py's own discharge
computation read a LETTERED acceptance-test row's own discharge column?

L6 step 1's own review: SPEC-io-formats.md's first full §8 table used
individually-numbered lettered-suffix rows (`IOFM-A-004b`, matching the same
amendment-lettering convention `EPH-A-001b`/`PHPR-A-003b` already use
elsewhere in this tree) to discharge four refusals -- and every one of them
showed UNCOVERED despite a real, correctly-written row existing for each.
The acceptance-row-finding regex required a bare `-A-nnn` id with no
lettered suffix, so the row was never even found, let alone its own
discharge column read. This replays that shape, the same "inject the
historical error" discipline `test_budgetcheck.py`/`test_speccheck_duplicate.py`
already use for two other defects in this same tool.
"""

import subprocess
import sys
import tempfile
from pathlib import Path

TOOL = Path(__file__).resolve().parent.parent / "tools" / "speccheck.py"
OK, GAPS, UNPARSEABLE = 0, 1, 3

failures = []


def run(spec_text: str, quiet: bool = True) -> subprocess.CompletedProcess:
    with tempfile.TemporaryDirectory() as td:
        spec_dir = Path(td) / "spec"
        spec_dir.mkdir()
        (spec_dir / "SPEC-synthetic.md").write_text(spec_text, encoding="utf-8")
        args = [sys.executable, str(TOOL), "--spec-dir", str(spec_dir), "--skip-test-case-check"]
        if quiet:
            args.append("--quiet")
        return subprocess.run(args, capture_output=True, text=True)


def check(name, got, want, extra=""):
    if got == want:
        print(f"ok       {name}")
    else:
        print(f"FAILED   {name}: expected {want}, got {got}\n{extra}", file=sys.stderr)
        failures.append(name)


def spec_with_acceptance_id(acceptance_id: str) -> str:
    return (
        "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n"
        "## 5. Required behaviour\n\n"
        "- **SYN-R-001.** A synthetic requirement.\n\n"
        "## 8. Acceptance tests\n\n"
        "| id | what is checked | expected value | source | tolerance | discharges |\n"
        "|---|---|---|---|---|---|\n"
        f"| `{acceptance_id}` | checks SYN-R-001 | a value | a source | exact | R-001 |\n\n"
        "**Coverage.**\n\n| id | why no test |\n|---|---|\n"
    )


def main() -> int:
    # --- THE HISTORICAL SHAPE: a lettered acceptance row must discharge ----
    res = run(spec_with_acceptance_id("SYN-A-001b"))
    check("a lettered acceptance row (SYN-A-001b) discharges its requirement",
          res.returncode, OK, res.stdout + res.stderr)

    # --- the unlettered case must still work (no regression) ---------------
    res = run(spec_with_acceptance_id("SYN-A-001"))
    check("an unlettered acceptance row (SYN-A-001) still discharges its requirement",
          res.returncode, OK, res.stdout + res.stderr)

    # --- THE FAILURE MODE THIS FIX MUST NOT HAVE INTRODUCED -----------------
    # A requirement genuinely undischarged (the acceptance row names a
    # DIFFERENT requirement) must still be reported UNCOVERED -- the fix
    # widens which acceptance ROWS are found, not which requirements they
    # are deemed to discharge.
    spec_text = (
        "| | |\n|---|---|\n| **Spec ID** | `SYN` |\n\n"
        "## 5. Required behaviour\n\n"
        "- **SYN-R-001.** A synthetic requirement.\n"
        "- **SYN-R-002.** A second synthetic requirement, never discharged.\n\n"
        "## 8. Acceptance tests\n\n"
        "| id | what is checked | expected value | source | tolerance | discharges |\n"
        "|---|---|---|---|---|---|\n"
        "| `SYN-A-001b` | checks SYN-R-001 | a value | a source | exact | R-001 |\n\n"
        "**Coverage.**\n\n| id | why no test |\n|---|---|\n"
    )
    res = run(spec_text)
    check("a genuinely undischarged requirement (SYN-R-002) still fails the gate",
          res.returncode, GAPS, res.stdout + res.stderr)
    res_verbose = run(spec_text, quiet=False)
    check("  ... and is named UNCOVERED, not silently passed",
          "SYN-R-002" in (res_verbose.stdout + res_verbose.stderr), True,
          res_verbose.stdout + res_verbose.stderr)

    if failures:
        print(f"\n{len(failures)} check(s) failed", file=sys.stderr)
        return 1
    print("\nall lettered-acceptance-row discharge checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
