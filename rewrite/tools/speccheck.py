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

SCOPE, stated because the first version of this message overstated it.  This
tool reads the SPECIFICATIONS' internal traceability: is every requirement and
refusal discharged by an acceptance ROW, or individually excused?  It does not
look at the test suite and cannot tell whether those rows are implemented.  An
adopted-but-unbuilt specification passes.  Saying "tested" would have been the
same class of error as an unstated denominator: a true statement about a smaller
thing, read as a statement about a larger one.

Usage:  speccheck.py [--spec-dir DIR] [--cpp-root DIR] [--quiet]
Exit:   0 complete   1 gaps found   3 a spec could not be parsed

DUPLICATE TEST_CASE CLAIMS (added 2026-09-24, L5 step 4's own review round).
A spec file's own internal duplicate rows were always caught (`DUPLICATE
definitions`, below) — but the defect that prompted this addition was NOT
that: two DIFFERENT `TEST_CASE`s, in two DIFFERENT `.cpp` files, both named
themselves after the SAME id (`SRPA-A-011`), one a long-standing test this
tree already had, the other a brand new one that assumed the number was
free without checking. Nothing in this tool, or in Catch2 itself (each
compiles into its own module's own test binary, so the same string name in
two binaries collides with nothing at build or run time), would have
caught it. This scans every `.cpp` file's own `TEST_CASE("<ID> ...")`
invocations, tree-wide, and refuses if the SAME id is claimed by more than
one — independent of which spec file, if any, actually defines that id.

DUPLICATE DEFINITIONS ACROSS SPEC FILES (added 2026-09-24, the same review
round's own third question: does the SPEC side of this have the same gap the
TEST_CASE side just closed?). `check_spec()` already catches a duplicate row
WITHIN one file (`DUPLICATE definitions`, below), but until now that was the
whole check — each file's own `defined` set is computed from ITS OWN declared
prefix alone (`table_defs`/`bullet_defs` only ever match `{that file's own
prefix}-...`), and nothing aggregated across files. Two literal id strings can
only collide across files if both files declare the SAME `Spec ID` prefix in
their own front matter — a different kind of copy-paste than the TEST_CASE
one, but the same shape: each file looks completely clean in isolation, and
nothing tree-wide was checking the seam between them. This aggregates every
spec's own `defined` set across the whole `--spec-dir` run and refuses if the
SAME id is defined in more than one file."""

from __future__ import annotations

import argparse
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path

OK, GAPS, UNPARSEABLE = 0, 1, 3

ID_RE = re.compile(r"\b([A-Z][A-Z0-9]{1,15})-([RSFAQP])-(\d+[a-z]?)\b")
SPEC_ID_RE = re.compile(r"^\|\s*\*\*Spec ID\*\*\s*\|\s*`([A-Z][A-Z0-9]{1,15})`\s*\|", re.M)
COVERAGE_RE = re.compile(r"\*\*Coverage\.\*\*")
RANGE_RE = re.compile(r"\b([RF])-(\d+)…([RF])-(\d+)\b")
TEST_CASE_RE = re.compile(
    r'TEST_CASE\s*\(\s*"([A-Z][A-Z0-9]{1,15}-[RSFAQP]-\d+[a-z]?)')


def test_case_claims(cpp_root: Path) -> dict[str, list[str]]:
    """Every id claimed as a `TEST_CASE`'s own leading identifier, tree-wide,
    mapped to where each claim was found ("path:line"). A `.cpp` file that
    cannot be read as UTF-8 is skipped, not fatal — this check's own false
    negative there is far cheaper than making an unrelated encoding issue
    block gate 7."""
    claims: dict[str, list[str]] = defaultdict(list)
    for path in sorted(cpp_root.rglob("*.cpp")):
        try:
            text = path.read_text(encoding="utf-8")
        except (UnicodeDecodeError, OSError):
            continue
        for m in TEST_CASE_RE.finditer(text):
            line = text.count("\n", 0, m.start()) + 1
            claims[m.group(1)].append(f"{path}:{line}")
    return claims


def check_duplicate_test_case_claims(cpp_root: Path, quiet: bool) -> bool:
    claims = test_case_claims(cpp_root)
    dupes = {i: locs for i, locs in claims.items() if len(locs) > 1}
    if not quiet:
        print(f"\nTEST_CASE claims, tree-wide (`--cpp-root {cpp_root}`)")
        print(f"  distinct ids claimed     {len(claims):>4}")
        print(f"  DUPLICATE claims         {len(dupes):>4}")
    if dupes:
        for i, locs in sorted(dupes.items()):
            print(f"  DUPLICATE  {i}  claimed by {len(locs)} TEST_CASEs:", file=sys.stderr)
            for loc in locs:
                print(f"      {loc}", file=sys.stderr)
    return not dupes


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

    # EXCUSED IS THE FIRST CELL OF A COVERAGE ROW, NOT ANY MENTION IN ONE.
    # The first version matched every identifier anywhere in the Coverage region,
    # so an excuse that read "discharged by PERT-A-016" excused whatever it
    # named in passing.  That inflated the excused count without changing the
    # verdict, which is why it survived: UNCOVERED stayed 0 and nobody adds up
    # the components of a passing gate.  A row's first cell may name several
    # identifiers — "R-023, R-024" is one excuse for two — so the cell is parsed
    # rather than the row.
    # A row marked "(partial" — optionally qualified, "(partial: third bullet
    # only)" — excuses PART of a requirement whose other parts a
    # test does cover.  It counts as tested, not excused, and is reported
    # separately: a requirement with three clauses of which one is structural is
    # a real thing, and the alternative — leaving it to look like a
    # contradiction, or dropping the explanation — is worse than a marker.
    excused: set[str] = set()
    partial: set[str] = set()
    for row in re.findall(r"^\|(.*?)\|", coverage, re.M):
        target = partial if "(partial" in row else excused
        for l, n in re.findall(rf"`{prefix}-([RFS])-(\d+[a-z]?)`", row):
            target.add(f"{prefix}-{l}-{n}")

    uncovered = sorted(reqs - discharged - excused)
    # An identifier that is BOTH excused and discharged is a contradiction: the
    # Coverage table's column heading is "why no test", so a row for something a
    # test does discharge is stale.  It is reported, never netted off.
    contradictory = sorted((reqs & discharged) & excused)
    partial_ok = sorted(reqs & discharged & partial)
    # A "(partial)" marker on something no test discharges is not a partial
    # excuse, it is an excuse with a misleading label.
    contradictory += sorted((reqs & partial) - discharged)

    # Every identifier mentioned anywhere, so dangling references are caught.
    all_ids = {f"{p}-{k}-{n}" for p, k, n in ID_RE.findall(text)}
    own_ids = {i for i in all_ids if i.startswith(prefix + "-")}
    foreign = sorted(all_ids - own_ids)
    dangling = sorted(own_ids - defined)

    stat = {
        "path": path, "prefix": prefix,
        "own": len(own_ids), "all": len(all_ids), "foreign": foreign,
        "defined": len(defined), "defined_ids": defined, "reqs": len(reqs),
        "tested": len(reqs & discharged), "excused": len(reqs & excused),
        "uncovered": uncovered, "dangling": dangling, "duplicates": dup,
        "contradictory": contradictory, "partial": partial_ok,
    }
    # The three printed components must PARTITION the denominator.  They are laid
    # out as though they do, so they are made to, and the check is here rather
    # than in a reader's head.
    assert (stat["tested"] + stat["excused"] - len(contradictory) + len(uncovered)
            == stat["reqs"]), (
        f"{path}: {stat['tested']} tested + {stat['excused']} excused "
        f"- {len(contradictory)} both + {len(uncovered)} uncovered != {stat['reqs']}")

    ok = not (uncovered or dangling or dup or contradictory)

    if not quiet:
        print(f"\n{path.name}  [Spec ID: {prefix}]")
        print(f"  identifiers, OWN-PREFIX  {stat['own']:>4}   <- the denominator")
        print(f"  identifiers, ALL         {stat['all']:>4}"
              + (f"   ({len(foreign)} foreign, NOT counted: {', '.join(foreign)})" if foreign
                 else "   (no foreign identifiers referenced)"))
        print(f"  requirements + refusals  {stat['reqs']:>4}")
        print(f"    discharged by a test   {stat['tested']:>4}")
        print(f"    excused in §8 Coverage {stat['excused']:>4}")
        if stat["partial"]:
            print(f"    partially excused      {len(stat['partial']):>4}"
                  f"   {', '.join(stat['partial'])}"
                  "   <- counted as tested; one clause is structural")
        if stat["contradictory"]:
            print(f"    BOTH tested and excused{len(stat['contradictory']):>4}"
                  f"   {', '.join(stat['contradictory'])}"
                  "   <- the Coverage row is stale; its column says \"why no test\"")
        print(f"    UNCOVERED              {len(uncovered):>4}"
              + (f"   {', '.join(uncovered)}" if uncovered else ""))
        if dangling:
            print(f"  DANGLING references      {len(dangling):>4}   {', '.join(dangling)}")
        if dup:
            print(f"  DUPLICATE definitions    {len(dup):>4}   {', '.join(dup)}")
    return ok, stat


def check_cross_file_duplicate_definitions(stats: list[dict], quiet: bool) -> bool:
    """The SAME identifier defined (as a table row or a bulleted requirement)
    in two DIFFERENT spec files. Can only happen if both files declare the
    same `Spec ID` prefix — see the module docstring. Aggregates every spec's
    own `defined_ids` set across the whole run, the same "collect every claim,
    then look for one id with more than one location" shape
    `check_duplicate_test_case_claims` already uses for TEST_CASEs."""
    locations: dict[str, list[Path]] = defaultdict(list)
    for s in stats:
        for i in s["defined_ids"]:
            locations[i].append(s["path"])
    dupes = {i: paths for i, paths in locations.items() if len(paths) > 1}
    if not quiet:
        print(f"\nCross-file identifier definitions ({len(stats)} specs read as one namespace)")
        print(f"  distinct ids defined      {len(locations):>4}")
        print(f"  DUPLICATE definitions     {len(dupes):>4}")
    if dupes:
        for i, paths in sorted(dupes.items()):
            print(f"  DUPLICATE  {i}  defined in {len(paths)} files:", file=sys.stderr)
            for p in paths:
                print(f"      {p}", file=sys.stderr)
    return not dupes


def main(argv: list[str] | None = None) -> int:
    root = Path(__file__).resolve().parent.parent
    ap = argparse.ArgumentParser(prog="speccheck.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--spec-dir", type=Path, default=root / "spec")
    ap.add_argument("--cpp-root", type=Path, default=root)
    ap.add_argument("--skip-test-case-check", action="store_true",
                    help="spec-only run (the synthetic-duplicate test's own positive case "
                         "needs this, so a --spec-dir pointed at a temp dir is not also "
                         "scanned tree-wide for unrelated TEST_CASE duplicates)")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)

    specs = sorted(p for p in args.spec_dir.glob("SPEC-*.md") if p.name != "SPEC-template.md")
    if not specs:
        print(f"speccheck.py: no SPEC-*.md in {args.spec_dir}", file=sys.stderr)
        return UNPARSEABLE

    results = [check_spec(p, args.quiet) for p in specs]
    stats = [s for _, s in results]
    cross_file_ok = check_cross_file_duplicate_definitions(stats, args.quiet)
    test_case_ok = True if args.skip_test_case_check else check_duplicate_test_case_claims(
        args.cpp_root, args.quiet)

    total_reqs = sum(s["reqs"] for s in stats)
    total_own = sum(s["own"] for s in stats)
    total_all = sum(s["all"] for s in stats)
    total_tested = sum(s["tested"] for s in stats)
    total_excused = sum(s["excused"] for s in stats)
    total_uncovered = sum(len(s["uncovered"]) for s in stats)

    print(f"\n{'=' * 70}")
    print(f"{len(specs)} specifications")
    print(f"  requirements and refusals, OWN-PREFIX denominator : {total_reqs}")
    print(f"    discharged by an acceptance ROW                 : {total_tested}")
    print(f"    excused, with a reason, in §8 Coverage          : {total_excused}")
    print(f"    uncovered                                       : {total_uncovered}")
    print(f"  identifiers referenced, own-prefix / all          : {total_own} / {total_all}")
    if total_own != total_all:
        print(f"  the difference of {total_all - total_own} is cross-references between specs, which")
        print(f"  are references and not definitions.  Counting them would inflate the")
        print(f"  denominator; that is the 121-against-128 error, printed rather than made.")

    if all(ok for ok, _ in results) and cross_file_ok and test_case_ok:
        print("\nok       every requirement and refusal is discharged by an acceptance ROW")
        print("         or individually excused, in every specification.")
        print("ok       no id is defined in more than one specification file.")
        if not args.skip_test_case_check:
            print("ok       no id is claimed by more than one TEST_CASE, tree-wide.")
        print()
        print("         WHAT THIS DOES NOT CHECK: that those rows are implemented. This reads")
        print("         the specifications' own traceability, not the test suite, so a spec")
        print("         that is written and not yet built passes here — SPEC-ephemerides.md")
        print("         does today. A green result is 'the spec is internally complete', never")
        print("         'the module is tested'.")
        return OK
    print("\nFAILED   see above", file=sys.stderr)
    return GAPS


if __name__ == "__main__":
    sys.exit(main())
