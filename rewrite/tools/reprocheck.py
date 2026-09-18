#!/usr/bin/env python3
"""reprocheck.py — prove the build is reproducible, rather than assert it.

Plan L0 step 7.  The flags in cmake/OdlReproducible.cmake are a claim; this is
the evidence.  It builds the tree twice into directories of DIFFERENT NAME
LENGTHS and compares the hash of every artefact.

The differing lengths matter.  The classic leak is an absolute path landing in
debug info or an assert string, and if the two build directories have the same
length a leaked path changes the bytes but not their count, which several
comparison methods will still catch but which a padded or truncated field will
not.  Different lengths make a leak change the size as well as the content.

Usage:  reprocheck.py [--build-dir DIR] [--keep]
Exit:   0 reproducible   1 artefacts differ   3 a build failed
"""

from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

OK, DIFFERS, BUILD_FAILED = 0, 1, 3

# A fixed instant, so that anything reading a clock is pinned rather than free.
# 2026-09-18T00:00:00Z — the date L0 was built.
SOURCE_DATE_EPOCH = "1789603200"

ARTEFACT_SUFFIXES = (".a", ".so", ".o")


def sha256(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as fh:
        for block in iter(lambda: fh.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def build(root: Path, out: Path) -> None:
    env = dict(os.environ, SOURCE_DATE_EPOCH=SOURCE_DATE_EPOCH, LC_ALL="C", TZ="UTC")
    for cmd in (
        ["cmake", "-S", str(root), "-B", str(out), "-G", "Ninja",
         "-DCMAKE_BUILD_TYPE=RelWithDebInfo"],
        ["cmake", "--build", str(out)],
    ):
        r = subprocess.run(cmd, env=env, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"reprocheck: build failed: {' '.join(cmd)}\n{r.stdout}\n{r.stderr}",
                  file=sys.stderr)
            raise SystemExit(BUILD_FAILED)


def artefacts(out: Path) -> dict[str, Path]:
    found = {}
    for p in sorted(out.rglob("*")):
        if not p.is_file():
            continue
        rel = p.relative_to(out)
        if p.suffix in ARTEFACT_SUFFIXES or (
                p.stat().st_mode & 0o111 and p.suffix == "" and "CMakeFiles" not in rel.parts):
            found[str(rel)] = p
    return found


def main(argv: list[str] | None = None) -> int:
    root = Path(__file__).resolve().parent.parent
    ap = argparse.ArgumentParser(prog="reprocheck.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build-dir", type=Path, default=None,
                    help="ignored; present so ci.sh can pass one uniformly")
    ap.add_argument("--keep", action="store_true", help="do not delete the two build trees")
    args = ap.parse_args(argv)

    a = root / "build-repro-a"
    b = root / "build-repro-bbbbbbbb"       # deliberately a different length
    assert len(str(a)) != len(str(b))

    for d in (a, b):
        shutil.rmtree(d, ignore_errors=True)

    print(f"building twice (SOURCE_DATE_EPOCH={SOURCE_DATE_EPOCH})")
    print(f"  A  {a}")
    print(f"  B  {b}   <- {len(str(b)) - len(str(a))} characters longer")
    build(root, a)
    build(root, b)

    fa, fb = artefacts(a), artefacts(b)
    only_a, only_b = sorted(set(fa) - set(fb)), sorted(set(fb) - set(fa))
    common = sorted(set(fa) & set(fb))

    differing = []
    for rel in common:
        ha, hb = sha256(fa[rel]), sha256(fb[rel])
        if ha != hb:
            differing.append((rel, ha, hb, fa[rel].stat().st_size, fb[rel].stat().st_size))

    print(f"\n{len(common)} artefacts compared")
    for rel, ha, hb, sa, sb in differing:
        print(f"  DIFFERS  {rel}\n    A {ha}  {sa} bytes\n    B {hb}  {sb} bytes"
              + ("\n    (sizes differ too — a path length is leaking into the artefact)"
                 if sa != sb else ""), file=sys.stderr)
    for rel in only_a:
        print(f"  ONLY IN A  {rel}", file=sys.stderr)
    for rel in only_b:
        print(f"  ONLY IN B  {rel}", file=sys.stderr)

    if not args.keep:
        for d in (a, b):
            shutil.rmtree(d, ignore_errors=True)

    if differing or only_a or only_b:
        print(f"\nNOT REPRODUCIBLE: {len(differing)} artefact(s) differ between two builds "
              f"of identical source.", file=sys.stderr)
        return DIFFERS
    print(f"ok       all {len(common)} artefacts byte-identical across two build paths")
    return OK


if __name__ == "__main__":
    sys.exit(main())
