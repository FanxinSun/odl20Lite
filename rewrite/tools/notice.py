#!/usr/bin/env python3
"""notice.py — generate NOTICE from the manifest.

Plan L0 step 5.  A hand-maintained NOTICE is wrong within two dependencies, so
this one is not maintained at all: it is a pure function of manifest.json, and
`--check` makes CI fail if the committed file has drifted from what the manifest
says.

D1 removed the `cargo license` route and the C++ ecosystem has no equivalent.
It does not need one.  The manifest already carries a licence and a licence note
per entry because plan L0 step 3 requires it, so NOTICE generation needs no tool
beyond the manifest and no dependency at all — which is a better answer than any
third-party licence scanner, because a scanner infers licences and this reads
the declaration that the fetcher already enforces.

Where an entry names a `licence_file` inside its archive, the licence text is
quoted VERBATIM, extracted from the exact bytes the SHA-256 pins.  A NOTICE that
paraphrases a licence is a NOTICE that can be wrong; one that quotes the pinned
archive cannot be.

Usage:  notice.py            write NOTICE
        notice.py --check    exit 1 if NOTICE differs from what would be written
        notice.py --stdout   print it instead
"""

from __future__ import annotations

import argparse
import difflib
import json
import sys
import tarfile
import textwrap
import zipfile
from pathlib import Path

OK, DRIFT, MALFORMED = 0, 1, 3
WIDTH = 78


def tree_root() -> Path:
    return Path(__file__).resolve().parent.parent


def rule(ch: str = "-") -> str:
    return ch * WIDTH


def licence_text(root: Path, doc: dict, e: dict) -> str | None:
    """Pull the licence file out of the cached, hash-verified archive."""
    if not e.get("licence_file") or e.get("provided_by_host"):
        return None
    archive = root / doc.get("cache", "data/cache") / e["id"] / e["filename"]
    if not archive.exists():
        return None
    inner = e["licence_file"]
    root_dir = e.get("unpacked_root")
    wanted = f"{root_dir}/{inner}" if root_dir else inner
    try:
        if e.get("unpack") == "tar.gz" or archive.name.endswith((".tar.gz", ".tgz")):
            with tarfile.open(archive, "r:gz") as tf:
                member = tf.extractfile(wanted)
                if member is None:
                    return None
                return member.read().decode("utf-8", "replace")
        if archive.name.endswith(".zip"):
            with zipfile.ZipFile(archive) as zf:
                return zf.read(wanted).decode("utf-8", "replace")
    except (KeyError, tarfile.TarError, zipfile.BadZipFile, OSError):
        return None
    return None


def render(root: Path, doc: dict) -> str:
    out: list[str] = []
    a = out.append

    a("NOTICE — third-party components")
    a(rule("="))
    a("")
    a("GENERATED FILE.  Do not edit.  Produced by tools/notice.py from")
    a("manifest/manifest.json, which is the single declaration of every external")
    a("input to this tree.  To change anything here, change the manifest and run")
    a("    python3 tools/notice.py")
    a("CI runs `notice.py --check` and fails if this file has drifted.")
    a("")
    a("This file does not describe the licence of THIS tree, which grants nothing")
    a("for now and is stated in LICENSE.  It describes what this tree depends on.")
    a("")

    code = [e for e in doc["entries"] if e["kind"] == "code"]
    data = [e for e in doc["entries"] if e["kind"] == "data"]
    tools = [e for e in doc["entries"] if e["kind"] == "tool"]

    a(rule())
    a("SUMMARY")
    a(rule())
    a("")
    a(f"  {'component':<18} {'version':<12} {'licence':<16} kind")
    for e in doc["entries"]:
        a(f"  {e['id']:<18} {str(e.get('version', '')):<12} {e['licence']:<16} {e['kind']}")
    a("")
    a("  Every licence above is permissive.  Plan §5 constraint 3 forbids GPL,")
    a("  LGPL and AGPL anywhere in what could ship; tools/fetch.py check-licences")
    a("  enforces it and CI runs it.")
    a("")

    for heading, group, blurb in (
        ("COMPONENTS COMPILED INTO OR LINKED WITH THIS TREE", code,
         "Fetched from origin, pinned by SHA-256, and verified twice — once by\n"
         "  tools/fetch.py and once by CMake's URL_HASH."),
        ("DATA INPUTS", data,
         "External data, pinned by hash.  The identity of an input is its hash,\n"
         "  not its URL: at least one upstream here revises its published series\n"
         "  retroactively at an unchanged address."),
        ("BUILD-TIME TOOLS", tools,
         "Present on the build host.  Not vendored, not linked, shipped in nothing.\n"
         "  Listed because an external input is an external input."),
    ):
        a(rule())
        a(heading)
        a(rule())
        a("")
        if not group:
            a("  None at this revision.")
            a("")
            continue
        a(f"  {blurb}")
        a("")
        for e in group:
            a(f"  {e['id']}  {e.get('version', '')}")
            a(f"    role      {e.get('role', '—')}")
            a(f"    licence   {e['licence']}")
            if e.get("provided_by_host"):
                a("    source    provided by the build host; not fetched")
            else:
                a(f"    url       {e['url']}")
                a(f"    sha256    {e['sha256']}")
                if e.get("archived_url") and e["archived_url"] != e.get("url"):
                    a(f"    archived  {e['archived_url']}")
                if e.get("retrieved"):
                    a(f"    retrieved {e['retrieved']}")
            if e.get("licence_note"):
                for line in textwrap.wrap(e["licence_note"], width=WIDTH,
                                          initial_indent="    note      ",
                                          subsequent_indent="              "):
                    a(line)
            a("")

    quoted = [(e, licence_text(root, doc, e)) for e in doc["entries"]]
    quoted = [(e, t) for e, t in quoted if t]
    if quoted:
        a(rule())
        a("LICENCE TEXTS, QUOTED VERBATIM FROM THE PINNED ARCHIVES")
        a(rule())
        a("")
        for e, text in quoted:
            a(f"--- {e['id']} {e.get('version', '')} — {e['licence']} "
              f"({e['licence_file']}) ".ljust(WIDTH, "-"))
            a("")
            for line in text.rstrip().splitlines():
                a("  " + line.rstrip())
            a("")

    return "\n".join(out).rstrip() + "\n"


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(prog="notice.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true", help="fail if NOTICE has drifted")
    ap.add_argument("--stdout", action="store_true", help="print instead of writing")
    ap.add_argument("--root", type=Path, default=tree_root())
    args = ap.parse_args(argv)

    root = args.root.resolve()
    manifest = root / "manifest" / "manifest.json"
    try:
        doc = json.loads(manifest.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"notice.py: cannot read {manifest}: {exc}", file=sys.stderr)
        return MALFORMED

    text = render(root, doc)
    target = root / "NOTICE"

    if args.stdout:
        sys.stdout.write(text)
        return OK

    if args.check:
        current = target.read_text(encoding="utf-8") if target.exists() else ""
        if current == text:
            print(f"ok       NOTICE matches the manifest ({len(doc['entries'])} entries)")
            return OK
        print("NOTICE HAS DRIFTED from the manifest.\n", file=sys.stderr)
        for line in difflib.unified_diff(
                current.splitlines(), text.splitlines(),
                fromfile="NOTICE (committed)", tofile="NOTICE (from manifest)", lineterm=""):
            print(line, file=sys.stderr)
        print("\n  NOTICE is generated, not maintained.  Run: python3 tools/notice.py",
              file=sys.stderr)
        return DRIFT

    target.write_text(text, encoding="utf-8")
    print(f"wrote    {target} ({len(doc['entries'])} entries)")
    return OK


if __name__ == "__main__":
    sys.exit(main())
