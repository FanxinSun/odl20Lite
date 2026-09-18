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
import tarfile
import zipfile
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


def entry_members(e: dict) -> list[dict]:
    """The archive members this tree reads individually, each with its own hash.

    AN ARCHIVE HASH SAYS NOTHING ABOUT WHAT IS TAKEN OUT OF AN ARCHIVE.  EGM2008
    ships as seven members of which this tree consumes two, and one of the five
    it does not is a FORTRAN harmonic-synthesis program that plan §3.3 step 2
    forbids as a source of recursions.  Declaring the members by hash is what
    turns "we did not open it" from an assertion into something a reviewer can
    check.

    That case prompted the rule and the rule is not about that case.  Every
    multi-member archive here has members read out of it individually — a licence
    text quoted verbatim into NOTICE, a file compared against a populated
    FetchContent tree — and each of those is now pinned as well as its container.
    A rule that applies only to the case that revealed it is the shape this
    project keeps catching.

    Members with `extract` land in the cache because something outside Python
    needs a path to them; the rest are verified in place, read straight out of
    the archive, because duplicating bytes to check them is not an improvement.
    """
    return list(e.get("members") or [])


def extract_path(root: Path, doc: dict, e: dict, m: dict) -> Path:
    return cache_dir(root, doc) / e["id"] / "extracted" / Path(m["member"]).name


def open_member(archive: Path, e: dict, member: str):
    """Read one member out of a pinned archive, whatever kind of archive it is."""
    kind = e.get("unpack")
    if kind == "zip":
        with zipfile.ZipFile(archive) as zf:
            return zf.read(member)
    if kind == "tar.gz":
        root = e.get("unpacked_root")
        inner = f"{root}/{member}" if root else member
        with tarfile.open(archive, "r:gz") as tf:
            f = tf.extractfile(inner)
            if f is None:
                raise KeyError(inner)
            return f.read()
    raise KeyError(f"entry {e['id']} declares members but unpack is {kind!r}")


def verify_members(root: Path, doc: dict, e: dict) -> list[str]:
    """Check every declared member against its own hash.  Returns their names."""
    seen = []
    for m in entry_members(e):
        if m.get("extract"):
            q = extract_path(root, doc, e, m)
            if not q.exists():
                raise FileNotFoundError(str(q))
            got = sha256_file(q)
        else:
            try:
                got = hashlib.sha256(open_member(entry_path(root, doc, e), e, m["member"])).hexdigest()
            except (KeyError, tarfile.TarError, zipfile.BadZipFile, OSError) as exc:
                die(MALFORMED, f"{e['id']}: cannot read member {m['member']!r}: {exc}")
        if got != m["sha256"].lower():
            die(MISMATCH,
                "ARCHIVE MEMBER HASH MISMATCH — refusing.\n"
                f"  entry    {e['id']}\n"
                f"  member   {m['member']}\n"
                f"  declared {m['sha256']}\n"
                f"  got      {got}\n"
                "\n"
                "  The archive's own hash may still match: an archive hash says nothing about\n"
                "  what is taken out of an archive, which is why the members are declared.")
        seen.append(m["member"])
    return seen


def extract_members(root: Path, doc: dict, e: dict) -> list[tuple[str, str]]:
    """Unpack the declared members, AFTER the archive's own hash has verified.

    Each member is checked against its own declared SHA-256 as it lands, so a
    change in upstream's zip tooling cannot quietly change what this tree reads
    while the archive hash still matches — the archive hash would change too, but
    the member hash is what the module actually consumes and it is the one worth
    stating.
    """
    out = []
    wanted = [m for m in entry_members(e) if m.get("extract")]
    if not wanted:
        return out
    archive = entry_path(root, doc, e)
    for m in wanted:
        dest = extract_path(root, doc, e, m)
        if dest.exists() and sha256_file(dest) == m["sha256"].lower():
            out.append((m["member"], "cached"))
            continue
        dest.parent.mkdir(parents=True, exist_ok=True)
        try:
            dest.with_suffix(dest.suffix + ".part").write_bytes(open_member(archive, e, m["member"]))
        except (KeyError, tarfile.TarError, zipfile.BadZipFile, OSError) as exc:
            die(MALFORMED, f"{e['id']}: cannot extract {m['member']!r} from {archive}: {exc}")
        part = dest.with_suffix(dest.suffix + ".part")
        got = sha256_file(part)
        if got != m["sha256"].lower():
            part.unlink(missing_ok=True)
            die(MISMATCH,
                f"EXTRACTED MEMBER HASH MISMATCH — refusing.\n"
                f"  entry    {e['id']}\n"
                f"  member   {m['member']}\n"
                f"  declared {m['sha256']}\n"
                f"  got      {got}\n"
                f"  archive  {archive}")
        shutil.move(str(part), str(dest))
        out.append((m["member"], got))
    return out


# WHAT A FILE HAS TO LOOK LIKE BEFORE IT IS WORTH HASHING.
#
# `content/chapter10/icc10.pdf` is a 404 — every other chapter of the IERS
# Conventions is `iccN.pdf` and chapter 10 is `tn36_c10.pdf` — and curl saves the
# error page with exit status 0.  The SHA-256 then catches it, but only on the
# SECOND fetch: the first fetch is the one that computes the hash you write into
# the manifest, and that is the fetch that matters.  An HTML error page hashes
# perfectly well.
#
# So the bytes are sniffed before they are hashed.  Two rules, and the first is
# the one that earns its place: anything that begins as an HTML document is
# refused whatever it was supposed to be, because no input this tree declares is
# HTML.  The second checks the magic against the declared extension where there
# is one to check.
HTML_STARTS = (b"<!doctype", b"<html", b"<?xml")
MAGIC = {
    ".pdf":  (b"%PDF",            "a PDF"),
    ".gz":   (b"\x1f\x8b",        "a gzip stream"),
    ".tgz":  (b"\x1f\x8b",        "a gzip stream"),
    ".zip":  (b"PK\x03\x04",      "a zip archive"),
    ".bsp":  (b"DAF/SPK",         "a DAF/SPK kernel"),
}


def sniff(head: bytes, e: dict) -> None:
    """Refuse an error page, or a file that is not what its name says."""
    low = head.lstrip()[:16].lower()
    if any(low.startswith(h) for h in HTML_STARTS):
        die(MALFORMED,
            f"{e['id']}: the server returned an HTML document, not a data file.\n"
            f"  url    {e['url']}\n"
            f"  begins {head[:48]!r}\n"
            "\n"
            "  This is almost always a 404 page saved with exit status 0. It would hash\n"
            "  perfectly well, and the manifest would then pin the error page.")
    want = MAGIC.get(Path(e["filename"]).suffix.lower())
    if want and not head.startswith(want[0]):
        die(MALFORMED,
            f"{e['id']}: {e['filename']} does not begin like {want[1]}.\n"
            f"  url    {e['url']}\n"
            f"  begins {head[:48]!r}\n"
            f"  expected the first bytes to be {want[0]!r}")


def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with p.open("rb") as fh:
        for block in iter(lambda: fh.read(CHUNK), b""):
            h.update(block)
    return h.hexdigest()


def download(url: str, dest: Path, e: dict) -> str:
    """Download to a .part file, return its hash.  The caller renames only after
    the hash has been checked, so a failed or truncated download can never leave
    something behind that a later `verify` would accept.

    The FIRST block is sniffed before anything is hashed (see `sniff`): an HTML
    error page hashes perfectly well, and on a first fetch — the one whose hash
    goes into the manifest — the hash has nothing to disagree with."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    part = dest.with_suffix(dest.suffix + ".part")
    h = hashlib.sha256()
    req = urllib.request.Request(url, headers={"User-Agent": "odl-self_built-fetch/1"})
    first = True
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT) as resp, part.open("wb") as out:
            while True:
                block = resp.read(CHUNK)
                if not block:
                    break
                if first:
                    sniff(block, e)
                    first = False
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

    undeclared = [e for e in fetchable(doc) if e.get("unpack") and not e.get("consumes")]
    if undeclared:
        die(MALFORMED,
            "an archive entry does not say what this tree consumes from it: "
            + ", ".join(e["id"] for e in undeclared)
            + "\n  Every entry with 'unpack' must declare 'consumes' — 'whole-tree' or\n"
              "  'declared-members' — with a note, and list the members it reads individually\n"
              "  with their own SHA-256. An archive hash says nothing about what is taken out\n"
              "  of an archive.")

    for e in ok:
        print(f"ok       {e['id']:<16} {e['sha256'][:16]}…  {entry_path(root, doc, e)}")
        try:
            names = verify_members(root, doc, e)
        except FileNotFoundError as exc:
            missing.append(e)
            print(f"MISSING  {e['id']:<16} declared member not extracted: {exc}", file=sys.stderr)
            continue
        for m in entry_members(e):
            where = "in the cache" if m.get("extract") else "in place"
            print(f"  member {m['member']:<30} {m['sha256'][:16]}…  verified {where}")
        if names:
            print(f"  consumes {e['consumes']}, {len(names)} member(s) read individually")
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
            for member, how in extract_members(root, doc, e):
                print(f"  member {member:<30} {how if how == 'cached' else how[:16] + '…'}")
            results.append((e["id"], e["url"], got))
            continue

        print(f"fetching {e['id']:<16} {e['url']}")
        part = p.with_suffix(p.suffix + ".part")
        got = download(e["url"], p, e)
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
        for member, how in extract_members(root, doc, e):
            print(f"  member {member:<30} {how if how == 'cached' else how[:16] + '…'}")
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
            if getattr(args, "member", None):
                for m in entry_extracts(e):
                    if Path(m["member"]).name == args.member or m["member"] == args.member:
                        print(extract_path(root, doc, e, m))
                        return OK
                die(USAGE, f"{args.id} declares no extracted member {args.member!r}")
            print(entry_path(root, doc, e))
            return OK
    die(USAGE, f"no manifest entry with id {args.id!r}")


def cmd_verify_populated(root: Path, doc: dict, args) -> int:
    """Is the tree FetchContent populated actually the archive we pinned?

    FetchContent honours the FIRST declaration of a name and silently ignores
    later ones.  That is how a top-level project pins a transitive dependency,
    and equally how a transitive dependency captures one of ours: tl::expected's
    own CMakeLists declares Catch2 v2.13.10 by URL with no hash, and before the
    declaration order was fixed the build fetched that over the network while
    reporting the pinned 3.16.0 from the cache.

    So this compares a file inside the populated tree against the same file
    inside the archive whose SHA-256 the manifest pins.  Nothing is inferred
    from a version string: the comparison is against the bytes.
    """
    for e in doc["entries"]:
        if e["id"] != args.id:
            continue
        rel = e.get("verify_path")
        if not rel:
            print(f"ok       {args.id:<16} populated (no verify_path declared)")
            return OK
        inner = f"{e['unpacked_root']}/{rel}" if e.get("unpacked_root") else rel
        archive = entry_path(root, doc, e)
        try:
            with tarfile.open(archive, "r:gz") as tf:
                member = tf.extractfile(inner)
                if member is None:
                    die(MALFORMED, f"{args.id}: {inner!r} is not in {archive}")
                pinned = member.read()
        except (tarfile.TarError, OSError) as exc:
            die(MALFORMED, f"{args.id}: cannot read {inner!r} from {archive}: {exc}")

        landed_path = Path(args.populated) / rel
        if not landed_path.exists():
            die(MISMATCH, f"{args.id}: populated tree has no {rel} at {landed_path}")
        landed = landed_path.read_bytes()
        if landed != pinned:
            die(MISMATCH,
                "PINNED DEPENDENCY SUBSTITUTED — refusing.\n"
                f"  entry      {args.id}\n"
                f"  pinned     {archive} (sha256 {e['sha256']})\n"
                f"  populated  {landed_path}\n"
                f"  compared   {rel}: {len(pinned)} bytes pinned, {len(landed)} bytes populated\n"
                "\n"
                "  Something other than this tree's declaration populated the content.\n"
                "  FetchContent honours the first declaration of a name, so the usual cause\n"
                "  is a transitive dependency's own FetchContent_Declare getting in first —\n"
                "  typically by URL, typically with no hash. Declare this tree's pins before\n"
                "  anything is made available (odl_declare_all_code) and find the competitor.")
        print(f"ok       {args.id:<16} populated tree matches the pinned archive ({rel})")
        return OK
    die(USAGE, f"no manifest entry with id {args.id!r}")


# Plan §5 constraint 3, as an ALLOWLIST.
#
# The first version of this check was a denylist: refuse anything whose name
# contained GPL, LGPL or AGPL.  That reports success without checking the thing.
# CALCEPH is triple-licensed CeCILL-C / CeCILL-B / CeCILL v2.1, and of those
# CeCILL-C is close to the LGPL and CeCILL v2.1 is GPL-compatible — yet neither
# string contains "GPL", so both would have passed.  So would MPL, EPL, CDDL,
# SSPL, OSL and EUPL.
#
# A denylist of forbidden licences can never be complete; an allowlist of
# permitted ones can.  Adding an entry here is a deliberate act with a reason
# attached, which is the point.
PERMISSIVE_LICENCES = {
    "0BSD":          "public-domain-equivalent",
    "APACHE-2.0":    "permissive, with an express patent grant",
    "BSD-2-CLAUSE":  "permissive",
    "BSD-3-CLAUSE":  "permissive",
    "BSL-1.0":       "Boost; permissive, and exempts object code from the notice requirement",
    "CC0-1.0":       "public-domain dedication",
    "CECILL-B":      "French BSD-like. NOT CeCILL-C (close to LGPL) and NOT CeCILL v2.1 "
                     "(GPL-compatible). Carries a citation obligation — see the entry's note.",
    "ISC":           "permissive",
    "MIT":           "permissive",
    "MIT-0":         "permissive, no attribution",
    "NCSA":          "permissive",
    "PSF-2.0":       "Python Software Foundation; permissive",
    "UNLICENSE":     "public-domain dedication",
    "ZLIB":          "permissive",
    "IERS-PUBLIC":   "not an SPDX identifier: IERS public data products, published for "
                     "unrestricted use. Data, never linked.",
    "NASA-PUBLIC":   "not an SPDX identifier: NASA/JPL published data products (NAIF generic\n"
                     "                    kernels, JPL SSD test sets). Data, never linked.",
    "NGA-PUBLIC":    "not an SPDX identifier: NGA published geospatial standards and models\n"
                     "                    (EGM2008, WGS 84). Unrestricted use; the EGM2008 README asks for a\n"
                     "                    citation, which PROVENANCE.md §4 carries. Data, never linked.",
}


def cmd_check_licences(root: Path, doc: dict, args) -> int:
    """Plan §5 constraint 3: only known-permissive licences, by allowlist."""
    bad = []
    for e in doc["entries"]:
        if e["licence"].upper().strip() not in PERMISSIVE_LICENCES:
            bad.append(e)
    for e in bad:
        print(
            f"LICENCE NOT ON THE PERMISSIVE LIST  {e['id']}: {e['licence']}\n"
            f"  Plan §5 constraint 3 permits only licences on tools/fetch.py's allowlist.\n"
            f"  This is an allowlist and not a denylist on purpose: a denylist of forbidden\n"
            f"  licences can never be complete, and CeCILL v2.1 — GPL-compatible copyleft —\n"
            f"  contains no forbidden substring and would pass one.\n"
            f"  If this licence is genuinely permissive, add it to PERMISSIVE_LICENCES with a\n"
            f"  one-line reason. That is meant to be a deliberate act.\n"
            f"  Permitted today: {', '.join(sorted(PERMISSIVE_LICENCES))}",
            file=sys.stderr,
        )
    if bad:
        return MALFORMED
    print(f"ok       {len(doc['entries'])} entries, every licence on the permissive allowlist")
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
    p.add_argument("--member", help="print the path of an extracted archive member instead")
    sub.add_parser("check-licences", help="plan §5 constraint 3: refuse GPL/LGPL/AGPL")
    vp = sub.add_parser("verify-populated",
                        help="is a populated FetchContent tree the archive we pinned?")
    vp.add_argument("id")
    vp.add_argument("populated", help="the populated source directory")

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
        "verify-populated": cmd_verify_populated,
    }[args.cmd](root, doc, args)


if __name__ == "__main__":
    sys.exit(main())
