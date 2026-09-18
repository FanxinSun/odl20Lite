#!/usr/bin/env python3
"""speccheck.py — the specification coverage checker.

Plan L0 step 6.  Every requirement and refusal in an adopted specification is
discharged by an acceptance test or individually excused, and this checks it.

THE DENOMINATOR IS THE WHOLE POINT.  A hand audit of the L1 specifications
counted 121 requirements and refusals; a naive count of the same files gave 128,
because the specifications cross-cite each other's identifiers by design and the
naive count swept in the references.  Seven out of 128 is small enough to read as
a rounding disagreement, and a script would have asserted whichever it computed
as fact.  So:

  * The denominator is OWN-PREFIX identifiers only: the prefix is read from each
    file's own `Spec ID` front-matter field, never guessed from the filename.
  * A file with no declared Spec ID is REFUSED, not guessed at.
  * BOTH counts are printed on every run, labelled, with the difference named.
    The checker cannot make the 128-vs-121 mistake quietly because it is
    structurally incapable of printing only one number.

Usage:  speccheck.py [--spec-dir DIR] [--quiet]
Exit:   0 complete   1 gaps found   3 a spec could not be parsed
"""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter
from pathlib import Path

OK, GAPS, UNPARSEABLE = 0, 1, 3

ID_RE = re.compile(r"\b([A-Z][A-Z0-9]{1,15})-([RSFAQP])-(\d+[a-z]?)\b")
SPEC_ID_RE = re.compile(r"^\|\s*\*\*Spec ID\*\*\s*\|\s*`([A-Z][A-Z0-9]{1,15})`\s*\|", re.M)
COVERAGE_RE = re.compile(r"\*\*Coverage\.\*\*")
RANGE_RE = re.compile(r"\b([RF])-(\d+)…([RF])-(\d+)\b")


def table_defs(text: str, prefix: str) -> list[str]:
    """Identifiers defined as the first cell of a table row."""
    return re.findall(rf"^\|\s*`({prefix}-[RSFAQP]-\d+[a-z]?)`\s*\|", text, re.M)


def bullet_defs(text: str, prefix: str) -> list[str]:
    """Identifiers defined as a bolded bullet: - **TIME-R-001.**"""
    return re.findall(rf"^\s*-\s+\*\*({prefix}-[RSFAQP]-\d+[a-z]?)\.\*\*", text, re.M)


def split_coverage(text: str) -> tuple[str, str]:
    """Split out the §8 Coverage table, which EXCUSES identifiers, from the rest
    of the document, which DEFINES them.

    The Coverage table's first column is an identifier in a table row, exactly
    like a definition, so without this split every excused item would be counted
    a second time as a definition — a denominator error of precisely the kind
    this tool exists to prevent.

    The region ends at the next `## ` heading, not at end of file.  It ran to EOF
    in the first draft, which swallowed §10's open-questions table: its `-Q-`
    rows stopped counting as definitions and reported as dangling references,
    and its incidental mentions of requirements inflated the excused count past
    the number of requirements that exist.  Both were visible only because the
    checker prints its components rather than a verdict."""
    m = COVERAGE_RE.search(text)
    if not m:
        return text, ""
    rest = text[m.start():]
    nxt = re.search(r"^## ", rest[1:], re.M)
    end = m.start() + 1 + nxt.start() if nxt else len(text)
    return text[: m.start()] + text[end:], text[m.start(): end]


def check_spec(path: Path, quiet: bool) -> tuple[bool, dict]:
    text = path.read_text(encoding="utf-8")

    m = SPEC_ID_RE.search(text)
    if not m:
        print(f"REFUSED  {path.name}: no `Spec ID` field in the front matter.\n"
              f"  The denominator is own-prefix identifiers, so the prefix must be declared,\n"
              f"  not inferred from the filename. Add a front-matter row:\n"
              f"      | **Spec ID** | `TIME` |", file=sys.stderr)
        raise SystemExit(UNPARSEABLE)
    prefix = m.group(1)

    body, coverage = split_coverage(text)

    defined = set(table_defs(body, prefix)) | set(bullet_defs(body, prefix))
    dup = [i for i, n in Counter(table_defs(body, prefix) + bullet_defs(body, prefix)).items() if n > 1]

    reqs = {i for i in defined if i.split("-")[1] in ("R", "F")}

    # Acceptance rows discharge requirements through their final column.
    discharged: set[str] = set()
    for row in re.findall(rf"^\|\s*`{prefix}-A-\d+`.*$", body, re.M):
        col = row.rstrip().rstrip("|").rsplit("|", 1)[-1]
        # The suffix letter matters: an amendment that inserts R-021a between
        # R-021 and R-022 keeps every later identifier stable, which is worth
        # more than tidy numbering — but only if the discharge parser sees it.
        for letter, num in re.findall(r"\b([RF])-(\d+[a-z]?)\b", col):
            discharged.add(f"{prefix}-{letter}-{num}")
        for l1, a, _l2, b in RANGE_RE.findall(col):
            for n in range(int(a), int(b) + 1):
                cand = f"{prefix}-{l1}-{n:03d}"
                if cand in defined:
                    discharged.add(cand)

    excused = {f"{prefix}-{l}-{n}" for l, n in re.findall(rf"`{prefix}-([RFS])-(\d+)", coverage)}

    uncovered = sorted(reqs - discharged - excused)

    # Every identifier mentioned anywhere, so dangling references are caught.
    all_ids = {f"{p}-{k}-{n}" for p, k, n in ID_RE.findall(text)}
    own_ids = {i for i in all_ids if i.startswith(prefix + "-")}
    foreign = sorted(all_ids - own_ids)
    dangling = sorted(own_ids - defined)

    stat = {
        "path": path, "prefix": prefix,
        "own": len(own_ids), "all": len(all_ids), "foreign": foreign,
        "defined": len(defined), "reqs": len(reqs),
        "tested": len(reqs & discharged), "excused": len(reqs & excused),
        "uncovered": uncovered, "dangling": dangling, "duplicates": dup,
    }

    ok = not (uncovered or dangling or dup)

    if not quiet:
        print(f"\n{path.name}  [Spec ID: {prefix}]")
        print(f"  identifiers, OWN-PREFIX  {stat['own']:>4}   <- the denominator")
        print(f"  identifiers, ALL         {stat['all']:>4}"
              + (f"   ({len(foreign)} foreign, NOT counted: {', '.join(foreign)})" if foreign
                 else "   (no foreign identifiers referenced)"))
        print(f"  requirements + refusals  {stat['reqs']:>4}")
        print(f"    discharged by a test   {stat['tested']:>4}")
        print(f"    excused in §8 Coverage {stat['excused']:>4}")
        print(f"    UNCOVERED              {len(uncovered):>4}"
              + (f"   {', '.join(uncovered)}" if uncovered else ""))
        if dangling:
            print(f"  DANGLING references      {len(dangling):>4}   {', '.join(dangling)}")
        if dup:
            print(f"  DUPLICATE definitions    {len(dup):>4}   {', '.join(dup)}")
    return ok, stat


def main(argv: list[str] | None = None) -> int:
    root = Path(__file__).resolve().parent.parent
    ap = argparse.ArgumentParser(prog="speccheck.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--spec-dir", type=Path, default=root / "spec")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)

    specs = sorted(p for p in args.spec_dir.glob("SPEC-*.md") if p.name != "SPEC-template.md")
    if not specs:
        print(f"speccheck.py: no SPEC-*.md in {args.spec_dir}", file=sys.stderr)
        return UNPARSEABLE

    results = [check_spec(p, args.quiet) for p in specs]
    stats = [s for _, s in results]

    total_reqs = sum(s["reqs"] for s in stats)
    total_own = sum(s["own"] for s in stats)
    total_all = sum(s["all"] for s in stats)
    total_tested = sum(s["tested"] for s in stats)
    total_excused = sum(s["excused"] for s in stats)
    total_uncovered = sum(len(s["uncovered"]) for s in stats)

    print(f"\n{'=' * 70}")
    print(f"{len(specs)} specifications")
    print(f"  requirements and refusals, OWN-PREFIX denominator : {total_reqs}")
    print(f"    discharged by an acceptance test                : {total_tested}")
    print(f"    excused, with a reason, in §8 Coverage          : {total_excused}")
    print(f"    uncovered                                       : {total_uncovered}")
    print(f"  identifiers referenced, own-prefix / all          : {total_own} / {total_all}")
    if total_own != total_all:
        print(f"  the difference of {total_all - total_own} is cross-references between specs, which")
        print(f"  are references and not definitions.  Counting them would inflate the")
        print(f"  denominator; that is the 121-against-128 error, printed rather than made.")

    if all(ok for ok, _ in results):
        print("\nok       every requirement and refusal is tested or excused")
        return OK
    print("\nFAILED   see above", file=sys.stderr)
    return GAPS


if __name__ == "__main__":
    sys.exit(main())
