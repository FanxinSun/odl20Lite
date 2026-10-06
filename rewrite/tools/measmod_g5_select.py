#!/usr/bin/env python3
"""measmod_g5_select.py — the G5 pass-selection rule of SPEC-measmod.md §8.9, applied to the normal-point file's METADATA.

THE RULE (written and committed, 0a48ec0, before this tool existed): among the CRD sessions of the pinned normal-point file whose station is Yarragadee (CDP pad 7090, the second
field of the `H2` record) and which lie inside the window — start >= 2026-01-01 00:00:00 UTC and end <= 2026-01-03 23:00:00 UTC — and whose `H4` data-quality-alert indicator is 0,
take the session with the most normal points (records of type 11), ties broken by the earliest start; a session the observation builder refuses for a reason of its own metadata is
skipped and recorded (the builder is C++: the envelope's test applies that clause and says so; this tool applies the rest).

No observed range is read: the normal points are COUNTED, their fields are not parsed. The tool prints every Yarragadee session of the file with the rule's verdict on it.

Usage:  measmod_g5_select.py [--check]       (--check: exit 1 unless the chosen session is the one recorded in SPEC-measmod.md §8.9 / CHOSEN below)
Exit:   0 printed (and, with --check, the choice reproduced)   1 the choice differs   2 an input file is missing
"""
from __future__ import annotations

import argparse
import datetime
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NP = ROOT / "data/cache/ilrs-lageos1-np-202601/lageos1_202601.np2"

PAD = 7090
WINDOW_START = datetime.datetime(2026, 1, 1, 0, 0, 0)
WINDOW_END = datetime.datetime(2026, 1, 3, 23, 0, 0)

# the choice the rule made when it was applied (SPEC-measmod.md §8.9, "the selection"): filled in by the first run, and then frozen with the commit that records it
CHOSEN = ("2026-01-02 04:04:45", 18)        # (start "YYYY-MM-DD HH:MM:SS" UTC, number of normal points): what the rule chose when it was applied, 2026-10-06


def sessions():
    out = []
    cur = None
    for ln in NP.read_text(encoding="utf-8").splitlines():
        tag = ln[:2].lower()
        f = ln.split()
        if tag == "h1":
            cur = {"pad": None, "name": None, "start": None, "end": None, "alert": None, "nps": 0, "line": 0}
            out.append(cur)
        elif cur is None:
            continue
        elif tag == "h2":
            cur["name"], cur["pad"] = f[1], int(f[2])
        elif tag == "h4":
            # H4: type, start y m d h m s, end y m d h m s, release, trop, CoM, rx amp, sys delay, tx amp, range type, quality alert
            y, mo, d, h, mi, s = (int(x) for x in f[2:8])
            cur["start"] = datetime.datetime(y, mo, d, h, mi, s)
            y, mo, d, h, mi, s = (int(x) for x in f[8:14])
            cur["end"] = datetime.datetime(y, mo, d, h, mi, s)
            cur["alert"] = int(f[-1])
        elif ln[:2] == "11":
            cur["nps"] += 1                      # counted, not parsed: no range enters
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    try:
        all_sessions = sessions()
    except OSError as exc:
        print(f"measmod_g5_select.py: {exc} (run `python3 tools/fetch.py fetch`)", file=sys.stderr)
        return 2
    mine = [s for s in all_sessions if s["pad"] == PAD]
    print(f"{len(all_sessions)} sessions in the file, {len(mine)} of pad {PAD}")
    rows = []
    for s in mine:
        inside = WINDOW_START <= s["start"] and s["end"] <= WINDOW_END
        eligible = inside and s["alert"] == 0
        rows.append((s, inside, eligible))
    print(f"{len([r for r in rows if not r[1]])} of them lie outside the window (not listed)")
    print(f"{'start (UTC)':>19} {'end (UTC)':>19} {'alert':>5} {'NPs':>4}  verdict")
    for s, inside, eligible in sorted(rows, key=lambda r: r[0]["start"]):
        if not inside:
            continue
        verdict = "eligible" if eligible else f"quality alert {s['alert']}"
        print(f"{s['start']:%Y-%m-%d %H:%M:%S} {s['end']:%Y-%m-%d %H:%M:%S} {s['alert']:>5} {s['nps']:>4}  {verdict}")
    eligible = [s for s, _, e in rows if e]
    ranked = sorted(eligible, key=lambda s: (-s["nps"], s["start"]))
    print("\nthe rule's order (most normal points, ties by the earliest start):")
    for i, s in enumerate(ranked[:6], 1):
        print(f"  {i}. {s['start']:%Y-%m-%d %H:%M:%S}  {s['nps']} normal points")
    if not ranked:
        print("no eligible session", file=sys.stderr)
        return 1
    top = ranked[0]
    print(f"\nCHOSEN: the session of {top['start']:%Y-%m-%d %H:%M:%S} UTC ({top['name']}, pad {top['pad']}), {top['nps']} normal points")
    if len(ranked) > 1 and ranked[1]["nps"] == top["nps"]:
        print(f"  (a tie of {top['nps']} points with {ranked[1]['start']:%Y-%m-%d %H:%M:%S}, broken by the earlier start)")
    if a.check:
        if CHOSEN is None:
            print("FAILED   the choice is not recorded in CHOSEN yet", file=sys.stderr)
            return 1
        ok = (f"{top['start']:%Y-%m-%d %H:%M:%S}", top["nps"]) == CHOSEN
        print("ok       the recorded choice is reproduced" if ok else f"FAILED   the rule now chooses {top['start']} with {top['nps']} points, not {CHOSEN}")
        return 0 if ok else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
