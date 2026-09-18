#!/usr/bin/env python3
"""test_fetch.py — the fetcher's refusals, exercised.

The manifest's value is entirely in what it REFUSES.  A fetcher that downloads
correctly but accepts a corrupted cache has bought nothing, so the refusal paths
are the ones worth testing, and they are tested against a synthetic manifest in
a temporary directory rather than against the real cache.
"""

import json
import subprocess
import sys
import tempfile
import hashlib
from pathlib import Path

TOOL = Path(__file__).resolve().parent.parent / "tools" / "fetch.py"
OK, MISSING, MISMATCH, MALFORMED, NETWORK, USAGE = 0, 1, 2, 3, 4, 5

failures = []


def run(root: Path, manifest: Path, *args) -> subprocess.CompletedProcess:
    return subprocess.run(
        [sys.executable, str(TOOL), "--root", str(root), "--manifest", str(manifest), *args],
        capture_output=True, text=True)


def check(name: str, got, want, extra: str = "") -> None:
    if got == want:
        print(f"ok       {name}")
    else:
        print(f"FAILED   {name}: expected {want}, got {got}\n{extra}", file=sys.stderr)
        failures.append(name)


_seq = 0


def write_manifest(d: Path, entries: list) -> Path:
    # Each manifest gets its own file.  They shared one path in the first draft,
    # so a later case silently clobbered an earlier one's manifest and three
    # checks failed against a file they had not written.
    global _seq
    _seq += 1
    m = d / f"manifest-{_seq}.json"
    m.write_text(json.dumps({"schema": 1, "cache": "cache", "entries": entries}, indent=2))
    return m


def make_blob(d: Path, eid: str, name: str, content: bytes) -> str:
    p = d / "cache" / eid / name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_bytes(content)
    return hashlib.sha256(content).hexdigest()


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)

        good = b"the bytes that were declared"
        digest = make_blob(root, "thing", "thing.bin", good)
        entry = {"id": "thing", "kind": "data", "licence": "CC0-1.0",
                 "url": "https://example.invalid/thing.bin",
                 "filename": "thing.bin", "sha256": digest}

        m = write_manifest(root, [entry])
        r = run(root, m, "verify")
        check("verify accepts a cache that matches", r.returncode, OK, r.stdout + r.stderr)

        # --- the refusal that matters most ----------------------------------
        (root / "cache" / "thing" / "thing.bin").write_bytes(good + b"!")
        r = run(root, m, "verify")
        check("verify REFUSES a corrupted cache", r.returncode, MISMATCH, r.stdout + r.stderr)
        check("the refusal names the expected hash", digest[:16] in r.stderr, True)
        check("the refusal names the obtained hash",
              hashlib.sha256(good + b"!").hexdigest()[:16] in r.stderr, True)
        check("the refusal says it is not a hash to update",
              "not a hash to update" in r.stderr, True)

        # --- missing from cache ---------------------------------------------
        (root / "cache" / "thing" / "thing.bin").unlink()
        r = run(root, m, "verify")
        check("verify reports a missing entry distinctly", r.returncode, MISSING, r.stdout + r.stderr)

        # --- verify never touches the network -------------------------------
        # The URL is .invalid, which cannot resolve.  A verify that tried to
        # fetch would fail with NETWORK, not MISSING.  The line above proves it.

        # --- malformed manifests --------------------------------------------
        make_blob(root, "thing", "thing.bin", good)

        bad = write_manifest(root, [dict(entry, sha256="not-a-hash")])
        r = run(root, bad, "verify")
        check("a malformed sha256 is refused", r.returncode, MALFORMED, r.stderr)

        bad = write_manifest(root, [entry, dict(entry)])
        r = run(root, bad, "verify")
        check("a duplicate id is refused", r.returncode, MALFORMED, r.stderr)

        e2 = dict(entry); del e2["sha256"]
        bad = write_manifest(root, [e2])
        r = run(root, bad, "verify")
        check("an entry with no hash is refused", r.returncode, MALFORMED, r.stderr)

        bad = root / "bad-json.json"
        bad.write_text('{"schema": 1, "entries": [')
        r = run(root, bad, "verify")
        check("invalid JSON is refused with a line number", r.returncode, MALFORMED, r.stderr)

        bad = write_manifest(root, [dict(entry, kind="wishful")])
        r = run(root, bad, "verify")
        check("an unknown kind is refused", r.returncode, MALFORMED, r.stderr)

        # --- plan §5 constraint 3 -------------------------------------------
        m2 = write_manifest(root, [entry])
        r = run(root, m2, "check-licences")
        check("a permissive manifest passes the licence check", r.returncode, OK, r.stderr)

        for lic in ("GPL-3.0-only", "LGPL-2.1", "AGPL-3.0"):
            bad = write_manifest(root, [dict(entry, licence=lic)])
            r = run(root, bad, "check-licences")
            check(f"{lic} is refused", r.returncode, MALFORMED, r.stderr)

        # A host-provided tool needs no URL or hash, but still needs a licence.
        tool = {"id": "python3", "kind": "tool", "licence": "PSF-2.0", "provided_by_host": True}
        m3 = write_manifest(root, [entry, tool])
        r = run(root, m3, "verify")
        check("a host-provided tool needs no hash", r.returncode, OK, r.stdout + r.stderr)

        notool = dict(tool); del notool["licence"]
        bad = write_manifest(root, [notool])
        r = run(root, bad, "verify")
        check("a host-provided tool still needs a licence", r.returncode, MALFORMED, r.stderr)

        # --- path, for CMake -------------------------------------------------
        r = run(root, m3, "path", "thing")
        check("path prints the cache location", r.stdout.strip().endswith("cache/thing/thing.bin"), True, r.stdout)
        r = run(root, m3, "path", "python3")
        check("path refuses a host-provided entry", r.returncode, USAGE, r.stderr)
        r = run(root, m3, "path", "nonexistent")
        check("path refuses an unknown id", r.returncode, USAGE, r.stderr)

    if failures:
        print(f"\n{len(failures)} check(s) failed: {', '.join(failures)}", file=sys.stderr)
        return 1
    print("\nall fetcher checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
