#!/usr/bin/env python3
"""test_speccheck_cross_file_duplicate.py — does speccheck.py catch a
duplicate ID defined across two DIFFERENT spec files, not just within one
file and not just in TEST_CASE names?

L5 step 4's third review round asked this directly: `check_spec()` already
caught a duplicate row WITHIN one file, and the round before this one taught
`speccheck.py` to catch a duplicate TEST_CASE name across two `.cpp` files
tree-wide — but neither of those aggregates a `defined` set ACROSS spec
files, so an id defined twice, in two different `.md` documents, had nothing
watching the seam between them. This replays that shape as a synthetic pair
of spec files, the same "inject the historical error" discipline
`test_budgetcheck.py` and `test_speccheck_duplicate.py` already use for two
other checkers.
"""

import re
import subprocess
import sys
import tempfile
from pathlib import Path

TOOL = Path(__file__).resolve().parent.parent / "tools" / "speccheck.py"
OK, GAPS, UNPARSEABLE = 0, 1, 3

failures = []


def run(spec_files: dict[str, str]) -> subprocess.CompletedProcess:
    with tempfile.TemporaryDirectory() as td:
        spec_dir = Path(td) / "spec"
        spec_dir.mkdir()
        for name, content in spec_files.items():
            (spec_dir / name).write_text(content, encoding="utf-8")
        # --skip-test-case-check: this is entirely about the cross-file SPEC
        # duplicate check, not the tree-wide TEST_CASE one (already covered by
        # test_speccheck_duplicate.py) -- without it, the default --cpp-root
        # would scan this real repository's own .cpp files for an unrelated
        # reason every time this test runs.
        return subprocess.run(
            [sys.executable, str(TOOL), "--spec-dir", str(spec_dir),
             "--skip-test-case-check", "--quiet"],
            capture_output=True, text=True)


def spec(prefix: str, rows: str) -> str:
    """A minimal, otherwise-valid spec file. Every -R-/-F- id mentioned in
    `rows` is automatically given a Coverage excuse, so the ONLY thing that
    can fail these synthetic files is the cross-file duplicate check itself
    -- not an incidental uncovered-requirement gap this test is not about."""
    ids = dict.fromkeys(re.findall(rf"{prefix}-[RF]-\d+[a-z]?", rows))
    coverage = "".join(
        f"| `{i}` | synthetic, excused so this test is only about cross-file duplication |\n"
        for i in ids)
    return (
        "| | |\n|---|---|\n"
        f"| **Spec ID** | `{prefix}` |\n\n"
        "## 5. Required behaviour\n\n" + rows + "\n\n"
        "## 8. Coverage\n\n**Coverage.**\n\n| id | reason |\n|---|---|\n" + coverage)


def check(name, got, want, extra=""):
    if got == want:
        print(f"ok       {name}")
    else:
        print(f"FAILED   {name}: expected {want}, got {got}\n{extra}", file=sys.stderr)
        failures.append(name)


def main() -> int:
    # --- THE HISTORICAL SHAPE: two different FILES, same PREFIX, same id ----
    # A literal id can only collide across files if both declare the same
    # Spec ID prefix (table_defs/bullet_defs only ever match a file's own
    # declared prefix) -- so that is exactly what the injected pair does.
    res = run({
        "SPEC-a.md": spec("DUPX", "- **DUPX-R-001.** first file's own row.\n"),
        "SPEC-b.md": spec("DUPX", "- **DUPX-R-001.** second file, same id, unrelated text.\n"),
    })
    check("CATCHES the same id defined in two different spec files",
          res.returncode, GAPS, res.stdout + res.stderr)
    combined = res.stdout + res.stderr
    check("  ... and names the id", "DUPX-R-001" in combined, True, combined)
    check("  ... and names both files", "SPEC-a.md" in combined and "SPEC-b.md" in combined,
          True, combined)

    # --- the collision survives a table-row vs. bulleted-row form mismatch --
    # defined_ids is the UNION of table_defs and bullet_defs, so the check
    # must not be blind to two files using different definition FORMS for the
    # same literal id string.
    res = run({
        "SPEC-c.md": spec("DUPY", "| `DUPY-R-001` | table-row form |\n"),
        "SPEC-d.md": spec("DUPY", "- **DUPY-R-001.** bulleted form, same id.\n"),
    })
    check("CATCHES the same id across a table-row and a bulleted definition",
          res.returncode, GAPS, res.stdout + res.stderr)

    # --- distinct, genuinely unrelated specs must pass -----------------------
    # This is the ordinary, legitimate shape this tree actually has: many spec
    # files, each with its own unique prefix. Must not be flagged.
    res = run({
        "SPEC-e.md": spec("ONEX", "- **ONEX-R-001.** first spec's own requirement.\n"),
        "SPEC-f.md": spec("TWOX", "- **TWOX-R-001.** second spec's own requirement, same NUMBER.\n"),
    })
    check("does not flag the same NUMBER under two different prefixes",
          res.returncode, OK, res.stdout + res.stderr)

    # --- a lettered-suffix split across files is NOT a duplicate ------------
    # The same legitimate-amendment mechanism this tree already relies on
    # within one file (EPH-A-001 / EPH-A-001b) must not be flagged across
    # files either: the literal id strings genuinely differ.
    res = run({
        "SPEC-g.md": spec("DUPZ", "- **DUPZ-R-001.** the original.\n"),
        "SPEC-h.md": spec("DUPZ", "- **DUPZ-R-001b.** an amendment, lettered, genuinely distinct.\n"),
    })
    check("a lettered-suffix split across files is NOT flagged as a duplicate",
          res.returncode, OK, res.stdout + res.stderr)

    # --- THE FAILURE MODE THIS CHECK MUST NOT HAVE ---------------------------
    # A spec directory with a genuine cross-file duplicate must not report
    # success on the "no id is defined in more than one specification file"
    # line specifically -- not just a nonzero exit code from some other gate.
    res = run({
        "SPEC-i.md": spec("DUPW", "- **DUPW-R-001.** first file.\n"),
        "SPEC-j.md": spec("DUPW", "- **DUPW-R-001.** second file.\n"),
    })
    check("does not report success on the cross-file line over an unresolved duplicate",
          "ok       no id is defined in more than one specification file" not in res.stdout,
          True, res.stdout)

    if failures:
        print(f"\n{len(failures)} check(s) failed", file=sys.stderr)
        return 1
    print("\nall cross-file duplicate-definition checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
