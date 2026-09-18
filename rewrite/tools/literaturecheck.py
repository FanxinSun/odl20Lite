#!/usr/bin/env python3
"""literaturecheck.py — nothing that builds may reach the literature.

Plan §5 constraint 3.  A `literature` manifest entry is exempt from the
permissive-licence gate because it is a PROVENANCE RECORD, NOT A DEPENDENCY:
it is pinned by hash so that "this was derived from that" is checkable by a
future reader who fetches the same hash, and nothing derived from it is a copy
of it.

THE EXEMPTION IS EARNED BY A CHECKED PROPERTY AND NEVER BY THE LABEL, which is
what this tool is.  A carve-out that cannot be shown to fire is a carve-out that
will be widened, so the three conditions are mechanical:

  1. a literature entry is fetched OUTSIDE the build cache -- `data/literature`,
     not `data/cache` (tools/fetch.py's entry_path);
  2. NO BUILD INPUT REFERENCES THAT PATH, which is this gate, and it is
     demonstrated by injecting one (plan §4 rule 5);
  3. the repository holds the URL and the hash and never the bytes, so this
     project redistributes nothing -- `.gitignore` carries `data/literature/`.

Condition 2 is the one that could rot silently.  A source file that opened one
of these PDFs, or a CMake target that copied one, would turn a citation into a
dependency without anybody deciding to -- and the licence question this tree
just ruled on would be back, answered wrongly by default.
"""
from __future__ import annotations
import argparse, json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Everything the build reads or is driven by. Specifications and PROVENANCE are
# NOT here on purpose: a citation is exactly where these belong.
BUILD_GLOBS = ("CMakeLists.txt", "*.cmake", "*.cpp", "*.hpp", "*.h", "*.c")
BUILD_DIRS = ("modules", "tests", "cmake")


def build_inputs() -> list[Path]:
    out = [ROOT / "CMakeLists.txt"]
    for d in BUILD_DIRS:
        base = ROOT / d
        if not base.exists():
            continue
        for g in BUILD_GLOBS:
            out += [p for p in base.rglob(g) if "build" not in p.parts]
    return sorted(p for p in out if p.exists())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()

    doc = json.loads((ROOT / "manifest" / "manifest.json").read_text())
    lit = [e for e in doc["entries"] if e.get("kind") == "literature"]
    lit_root = doc.get("literature", "data/literature")

    # what a build input must not mention: the root, or any entry's id or filename
    needles = {lit_root}
    for e in lit:
        needles.add(e["id"])
        needles.add(e["filename"])
    patterns = {n: re.compile(re.escape(n)) for n in needles if n}

    inputs = build_inputs()
    hits = []
    for p in inputs:
        text = p.read_text(encoding="utf-8", errors="replace")
        for n, pat in patterns.items():
            for i, line in enumerate(text.split("\n"), 1):
                if pat.search(line):
                    hits.append((str(p.relative_to(ROOT)), i, n, line.strip()[:88]))

    if not a.quiet:
        print("LITERATURE ENTRIES — pinned for provenance, exempt from the licence gate,")
        print("and unreachable from anything that builds.\n")
        for e in lit:
            print(f"  {e['id']}")
            print(f"    fetched to  {lit_root}/{e['id']}/{e['filename']}")
            print(f"    terms       {'established' if e.get('licence') else 'NOT established — see the entry'}")
    print(f"\n  literature entries              {len(lit):5d}")
    print(f"  build inputs searched           {len(inputs):5d}")
    print(f"  build inputs REACHING one       {len(hits):5d}")

    if hits:
        print("\nA BUILD INPUT REACHES A LITERATURE ENTRY:", file=sys.stderr)
        for f, i, n, line in hits:
            print(f"  {f}:{i}  mentions {n!r}\n      {line}", file=sys.stderr)
        print(
            "\n  That turns a citation into a dependency, and the licence exemption plan §5\n"
            "  constraint 3 grants does not cover it: the exemption holds because this tree\n"
            "  READS these and does not redistribute or build against them. If the build needs\n"
            "  it, it is not literature -- pin it as data, with a licence that passes the gate.",
            file=sys.stderr)
        return 1

    print("\nok       no build input can reach a literature entry")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
