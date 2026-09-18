#!/usr/bin/env python3
"""fetch.py — the manifest fetcher.

Plan L0 step 3: every external input is declared with URL, SHA-256 and a licence
note, and is fetched from origin into a cache.  Nothing enters the tree
undeclared.  This is rule R11's discipline — the identity of an input is its
hash, not its URL and not its version number — applied to code as well as data,
because upstream is mutable in both.

The tool is deliberately stdlib-only.  A fetcher that needs a package installed
before it can fetch anything is a bootstrapping regress, and the whole point of
this step is that the chain from a clean clone to a verified input has no
undeclared link in it.

Commands
--------
  verify              offline.  Every entry present in the cache and hash-correct?
  fetch               download what is missing, verify, then stop.
  fetch --refresh     re-download everything and report upstream drift.
  list [--json]       the entries, for the NOTICE generator and for humans.
  path <id>           the cache path of one entry, for CMake.

Exit codes are distinct because CI reads them:
  0 ok   1 missing from cache   2 HASH MISMATCH   3 manifest malformed
  4 network failure             5 usage error
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

OK, MISSING, MISMATCH, MALFORMED, NETWORK, USAGE = 0, 1, 2, 3, 4, 5

CHUNK = 1 << 20
TIMEOUT = 120


# --------------------------------------------------------------------------- #
# manifest


def tree_root() -> Path:
    return Path(__file__).resolve().parent.parent


def load_manifest(path: Path) -> dict:
    try:
        text = path.read_text(encoding="utf-8")
    except OSError as exc:
        die(MALFORMED, f"cannot read manifest {path}: {exc}")
    try:
        doc = json.loads(text)
    except json.JSONDecodeError as exc:
        die(MALFORMED, f"{path}: not valid JSON at line {exc.lineno}: {exc.msg}")

    if doc.get("schema") != 1:
        die(MALFORMED, f"{path}: unsupported schema {doc.get('schema')!r}; this tool speaks schema 1")
    if not isinstance(doc.get("entries"), list):
        die(MALFORMED, f"{path}: 'entries' must be a list")

    seen = {}
    for i, e in enumerate(doc["entries"]):
        where = f"{path}: entry {i}"
        for field in ("id", "kind", "licence"):
            if not e.get(field):
                die(MALFORMED, f"{where}: missing required field {field!r}")
        if e["id"] in seen:
            die(MALFORMED, f"{where}: duplicate id {e['id']!r} (first seen at entry {seen[e['id']]})")
        seen[e["id"]] = i
        if e["kind"] not in ("code", "data", "tool"):
            die(MALFORMED, f"{where}: kind must be code, data or tool, not {e['kind']!r}")
        if e.get("provided_by_host"):
            continue
        for field in ("url", "filename", "sha256"):
            if not e.get(field):
                die(MALFORMED, f"{where} ({e['id']}): missing required field {field!r}; "
                               f"an entry that is not provided_by_host must be pinned by URL and hash")
        h = e["sha256"]
        if len(h) != 64 or any(c not in "0123456789abcdef" for c in h.lower()):
            die(MALFORMED, f"{where} ({e['id']}): sha256 {h!r} is not 64 lowercase hex characters")
    return doc


def fetchable(doc: dict) -> list[dict]:
    return [e for e in doc["entries"] if not e.get("provided_by_host")]


def cache_dir(root: Path, doc: dict) -> Path:
    return root / doc.get("cache", "data/cache")


def entry_path(root: Path, doc: dict, e: dict) -> Path:
    return cache_dir(root, doc) / e["id"] / e["filename"]


# --------------------------------------------------------------------------- #
# hashing and download


def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as fh:
        for block in iter(lambda: fh.read(CHUNK), b""):
            h.update(block)
    return h.hexdigest()


def download(url: str, dest: Path) -> str:
    """Download to a .part file, return its hash.  The caller renames only after
    the hash has been checked, so a failed or truncated download can never leave
    something behind that a later `verify` would accept."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    part = dest.with_suffix(dest.suffix + ".part")
    h = hashlib.sha256()
    req = urllib.request.Request(url, headers={"User-Agent": "odl-self_built-fetch/1"})
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT) as resp, part.open("wb") as out:
            while True:
                block = resp.read(CHUNK)
                if not block:
                    break
                h.update(block)
                out.write(block)
    except (urllib.error.URLError, urllib.error.HTTPError, OSError, TimeoutError) as exc:
        part.unlink(missing_ok=True)
        die(NETWORK, f"download failed for {url}: {exc}")
    return h.hexdigest()


def mismatch(e: dict, got: str, where: str) -> None:
    die(
        MISMATCH,
        "HASH MISMATCH — refusing.\n"
        f"  entry     {e['id']} ({e.get('role', e['kind'])})\n"
        f"  url       {e['url']}\n"
        f"  expected  {e['sha256']}\n"
        f"  obtained  {got}\n"
        f"  at        {where}\n"
        "\n"
        "  The identity of an input is its hash.  Upstream changing under an unchanged\n"
        "  URL is the documented behaviour of at least one of this tree's data sources\n"
        "  (IERS EOP, revised retroactively three times: 2025-06-05, 2026-02-05,\n"
        "  2026-03-09), so this is a fault to investigate, not a hash to update.\n"
        "  If the change is intended, updating the manifest is a deliberate, reviewed,\n"
        "  logged act — see plan §3.11 point 3.",
    )


# --------------------------------------------------------------------------- #
# receipts


def write_receipt(root: Path, doc: dict, results: list[tuple[str, str, str]]) -> None:
    rec = cache_dir(root, doc) / "receipts.json"
    rec.parent.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    existing = {}
    if rec.exists():
        try:
            existing = {r["id"]: r for r in json.loads(rec.read_text()).get("receipts", [])}
        except (json.JSONDecodeError, OSError, KeyError, TypeError):
            existing = {}
    for eid, url, digest in results:
        existing[eid] = {"id": eid, "url": url, "sha256": digest, "retrieved": now}
    rec.write_text(
        json.dumps({"receipts": sorted(existing.values(), key=lambda r: r["id"])}, indent=2) + "\n",
        encoding="utf-8",
    )


# --------------------------------------------------------------------------- #
# commands


def cmd_verify(root: Path, doc: dict, args) -> int:
    missing, bad, ok = [], [], []
    for e in fetchable(doc):
        p = entry_path(root, doc, e)
        if not p.exists():
            missing.append(e)
            continue
        got = sha256_file(p)
        if got != e["sha256"].lower():
            bad.append((e, got, p))
        else:
            ok.append(e)

    for e, got, p in bad:
        mismatch(e, got, str(p))

    for e in ok:
        print(f"ok       {e['id']:<16} {e['sha256'][:16]}…  {entry_path(root, doc, e)}")
    for e in fetchable(doc):
        if e in missing:
            print(f"MISSING  {e['id']:<16} {e['url']}", file=sys.stderr)
    for e in doc["entries"]:
        if e.get("provided_by_host"):
            print(f"host     {e['id']:<16} {e.get('version', '')}  (not fetched: provided by the build host)")

    if missing:
        print(
            f"\n{len(missing)} entr{'y is' if len(missing) == 1 else 'ies are'} not in the cache. "
            f"Run: tools/fetch.py fetch",
            file=sys.stderr,
        )
        return MISSING
    return OK


def cmd_fetch(root: Path, doc: dict, args) -> int:
    results = []
    for e in fetchable(doc):
        p = entry_path(root, doc, e)
        if p.exists() and not args.refresh:
            got = sha256_file(p)
            if got != e["sha256"].lower():
                mismatch(e, got, str(p))
            print(f"cached   {e['id']:<16} {e['sha256'][:16]}…")
            results.append((e["id"], e["url"], got))
            continue

        print(f"fetching {e['id']:<16} {e['url']}")
        part = p.with_suffix(p.suffix + ".part")
        got = download(e["url"], p)
        if got != e["sha256"].lower():
            if args.refresh and p.exists():
                part.unlink(missing_ok=True)
                print(
                    f"\nUPSTREAM DRIFT: {e['id']} now hashes {got}, manifest says {e['sha256']}.\n"
                    f"The cached copy is untouched.  This is the condition R11 exists to detect.",
                    file=sys.stderr,
                )
                return MISMATCH
            part.unlink(missing_ok=True)
            mismatch(e, got, f"{e['url']} (download discarded)")
        shutil.move(str(part), str(p))
        print(f"ok       {e['id']:<16} {got[:16]}…")
        results.append((e["id"], e["url"], got))

    write_receipt(root, doc, results)
    return OK


def cmd_list(root: Path, doc: dict, args) -> int:
    if args.json:
        print(json.dumps(doc, indent=2))
        return OK
    for e in doc["entries"]:
        src = "host" if e.get("provided_by_host") else e["url"]
        print(f"{e['id']:<16} {e['kind']:<6} {e['licence']:<16} {e.get('version', ''):<10} {src}")
    return OK


def cmd_path(root: Path, doc: dict, args) -> int:
    for e in doc["entries"]:
        if e["id"] == args.id:
            if e.get("provided_by_host"):
                die(USAGE, f"{args.id} is provided by the build host and has no cache path")
            print(entry_path(root, doc, e))
            return OK
    die(USAGE, f"no manifest entry with id {args.id!r}")


def cmd_check_licences(root: Path, doc: dict, args) -> int:
    """Plan §5 constraint 3: no GPL/LGPL/AGPL in anything that could ship."""
    forbidden = ("GPL", "AGPL", "LGPL")
    allowed_substrings = ("LGPL-EXCEPTION",)
    bad = []
    for e in doc["entries"]:
        lic = e["licence"].upper()
        if any(f in lic for f in forbidden) and not any(a in lic for a in allowed_substrings):
            bad.append(e)
    for e in bad:
        print(
            f"FORBIDDEN LICENCE  {e['id']}: {e['licence']}\n"
            f"  Plan §5 constraint 3: no GPL/LGPL/AGPL anywhere in what could ship.",
            file=sys.stderr,
        )
    if bad:
        return MALFORMED
    print(f"ok       {len(doc['entries'])} entries, all permissive")
    return OK


# --------------------------------------------------------------------------- #


def die(code: int, msg: str) -> None:
    print(f"fetch.py: {msg}", file=sys.stderr)
    raise SystemExit(code)


def main(argv: list[str] | None = None) -> int:
    root_default = tree_root()
    ap = argparse.ArgumentParser(prog="fetch.py", description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--manifest", type=Path, default=None,
                    help="manifest path (default: <tree>/manifest/manifest.json)")
    ap.add_argument("--root", type=Path, default=root_default,
                    help="tree root the cache is relative to")
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("verify", help="offline: is every entry cached and hash-correct?")
    f = sub.add_parser("fetch", help="download what is missing")
    f.add_argument("--refresh", action="store_true",
                   help="re-download everything and report upstream drift without overwriting the cache")
    l = sub.add_parser("list", help="show the entries")
    l.add_argument("--json", action="store_true")
    p = sub.add_parser("path", help="print the cache path of one entry")
    p.add_argument("id")
    sub.add_parser("check-licences", help="plan §5 constraint 3: refuse GPL/LGPL/AGPL")

    args = ap.parse_args(argv)
    root = args.root.resolve()
    manifest = args.manifest or (root / "manifest" / "manifest.json")
    doc = load_manifest(manifest)

    return {
        "verify": cmd_verify,
        "fetch": cmd_fetch,
        "list": cmd_list,
        "path": cmd_path,
        "check-licences": cmd_check_licences,
    }[args.cmd](root, doc, args)


if __name__ == "__main__":
    sys.exit(main())
