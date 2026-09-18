#!/usr/bin/env python3
"""test_budgetcheck.py — does the budget checker catch the errors it was built for?

The four wrong rows of 2026-09-18 are replayed here as synthetic specifications.
A checker that cannot catch the defect that prompted it is decoration, and the
negative cases matter more than the positive one: the failure mode this tool must
not have is reporting success over rows it did not understand.
"""

import subprocess
import sys
import tempfile
from pathlib import Path

TOOL = Path(__file__).resolve().parent.parent / "tools" / "budgetcheck.py"
OK, WRONG, UNPARSEABLE = 0, 1, 2

failures = []


def run(rows: str) -> subprocess.CompletedProcess:
    with tempfile.TemporaryDirectory() as td:
        spec = Path(td) / "SPEC-synthetic.md"
        spec.write_text("| id | quantity | budget | consequence | basis |\n"
                        "|---|---|---|---|---|\n" + rows, encoding="utf-8")
        return subprocess.run([sys.executable, str(TOOL), "--spec-dir", td, "--quiet"],
                              capture_output=True, text=True)


def check(name, got, want, extra=""):
    if got == want:
        print(f"ok       {name}")
    else:
        print(f"FAILED   {name}: expected {want}, got {got}\n{extra}", file=sys.stderr)
        failures.append(name)


def row(rid, consequence):
    return f"| `{rid}` | a quantity | a budget | {consequence} | a basis |\n"


def main() -> int:
    # --- the four real errors of 2026-09-18 -------------------------------
    historical = [
        ("TIME-P-1 as written: 1 ns given as 7.5 pm",
         row("TIME-P-1", "1 ns × 7.5 km s⁻¹ = **7.5 pm** at LEO")),
        ("TIME-P-1's achieved figure: 8 × 10⁻¹⁶ m",
         row("TIME-P-1", "1.11 × 10⁻¹⁶ s × 7.5 km s⁻¹ = **8 × 10⁻¹⁶ m**")),
        ("TIME-P-2 as written: 1 ns given as 7.5 nm",
         row("TIME-P-2", "1 ns × 7.5 km s⁻¹ = **7.5 nm** at LEO")),
        ("EPH-P-1 as written: 10⁻¹³ AU given as 15 µm",
         row("EPH-P-1", "10⁻¹³ AU × 1.495 978 707 × 10¹¹ m/AU = **15 µm**")),
    ]
    for name, r in historical:
        res = run(r)
        check(f"CATCHES {name}", res.returncode, WRONG, res.stdout + res.stderr)
        check(f"  ... and says so", "ARITHMETIC WRONG" in res.stderr, True, res.stderr)

    # --- the corrected forms must pass ------------------------------------
    for name, r in [
        ("7.5 µm", row("TIME-P-1", "1 ns × 7.5 km s⁻¹ = **7.5 µm** at LEO")),
        ("15 mm", row("EPH-P-1", "10⁻¹³ AU × 1.495 978 707 × 10¹¹ m/AU = **15 mm**")),
        ("a range", row("EOP-P-5", "5–20 µs × 0.51 mm/µs = **2.6–10.2 mm**")),
    ]:
        res = run(r)
        check(f"accepts the corrected {name}", res.returncode, OK, res.stdout + res.stderr)

    # --- THE FAILURE MODE THIS TOOL MUST NOT HAVE -------------------------
    # A row that looks like arithmetic and does not parse must FAIL, never be
    # skipped with a cheerful summary over the rows that did parse.
    for name, r in [
        ("an unknown unit", row("X-P-1", "1 furlong × 7.5 km s⁻¹ = **1 mm**")),
        ("mismatched dimensions", row("X-P-2", "1 ns × 7.5 km s⁻¹ = **3 rad**")),
        ("a factor with no units", row("X-P-3", "30 µas × 0.034 = **1.0 mm**")),
        ("nothing numeric on the left", row("X-P-4", "the model's own realisation = **1 mm**")),
    ]:
        res = run(r)
        check(f"REFUSES {name} rather than skipping it", res.returncode in (WRONG, UNPARSEABLE),
              True, res.stdout + res.stderr)
        check(f"  ... and does not report success", "ok       every budget row" not in res.stdout,
              True, res.stdout)

    # --- a row with no arithmetic is legitimately not checked --------------
    res = run(row("X-P-5", "—") + row("X-P-6", "exact"))
    check("a row with no arithmetic passes", res.returncode, OK, res.stdout + res.stderr)
    check("  ... and is counted as such", "2 with none" in res.stdout, True, res.stdout)

    if failures:
        print(f"\n{len(failures)} check(s) failed", file=sys.stderr)
        return 1
    print("\nall budget-checker checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
