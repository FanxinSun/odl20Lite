#!/usr/bin/env python3
"""measmod_registry_facts.py — the registry's data facts, counted independently of the registry.

SPEC-measmod.md MEAS-A-004 / MEAS-A-010 state the numbers of the pinned station files (SLRF2020 release
2026.02.05, the ILRS eccentricity file of 2026-05-27, the ITRF2020 SLR post-seismic event list).  The C++
registry is tested against them, so they must come from somewhere that is NOT the registry: this reads the
three cached files with the standard library only, by column and by whitespace field, and counts.  It
reads nothing from `modules/`.

A rule 3 record (plan §4): the header of the SLRF2020 file says "184 unique sites" and the file holds 186
distinct 4-character codes.  The difference is explained here by the file's own FILE/COMMENT history (Xian,
7329, and Ishioka, 7317, added in 2025 after the sentence was written), and checked: removing those two
pads leaves exactly 184.

Usage:  measmod_registry_facts.py [--root DIR] [--check]
Exit:   0 printed (and, with --check, every pinned number matched)   1 a pinned number did not match   2 a file is missing
"""
from __future__ import annotations

import argparse
import datetime
import sys
from pathlib import Path

CACHE = {
    "slrf": "data/cache/ilrs-slrf2020-20260205/SLRF2020_POS+VEL_2026.02.05.snx",
    "ecc": "data/cache/ilrs-slrecc-une-20260527/slrecc.260527.ILRS.une.snx",
    "psd": "data/cache/itrf2020-psd-slr/ITRF2020-psd-slr.dat",
}

# The numbers SPEC-measmod.md states for the pinned release (MEAS-A-004, MEAS-A-006, MEAS-R-006, §9.1).
EXPECTED = {
    "slrf_site_id_rows": 483, "slrf_distinct_sods": 483, "slrf_pads": 186, "slrf_markers": 190, "slrf_markers_with_solution": 189,
    "slrf_solutions": 237, "slrf_markers_with_more_than_one_soln": 28, "slrf_gaps_between_solutions": 48,
    "slrf_header_sites": 184, "pads_added_after_header": 2,
    "ecc_site_id_rows": 543, "ecc_distinct_sods": 542, "ecc_pads": 235, "ecc_identical_duplicate_rows": 1,
    "placed_sods": 482, "unplaced_sods": 60, "unplaced_pad_absent": 57, "unplaced_sod_absent_on_known_pad": 2, "unplaced_marker_without_solution": 1,
    "pad_point_domes_disagreements": 0, "psd_sites": 8, "psd_events": 12,
}


def block(lines: list[str], name: str) -> list[str]:
    out, on = [], False
    for ln in lines:
        if ln.startswith("+" + name):
            on = True
            continue
        if ln.startswith("-" + name):
            break
        if on and not ln.startswith("*"):
            out.append(ln)
    return out


def snx_epoch(e: str, end: bool = False):
    yy, doy, sec = e.split(":")
    if yy == "00" and doy == "000":
        return None  # open end (or unknown start)
    y = int(yy) + (2000 if int(yy) < 50 else 1900)
    if end and int(sec) == 86399:  # the file's day-end convention (SPEC-measmod MEAS-R-002)
        return datetime.datetime(y, 1, 1) + datetime.timedelta(days=int(doy))
    return datetime.datetime(y, 1, 1) + datetime.timedelta(days=int(doy) - 1, seconds=int(sec))


def site_key(line: str):
    return (line[1:5], line[7:8], line[9:18].strip())  # pad, point, DOMES


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    try:
        slrf = (a.root / CACHE["slrf"]).read_text(encoding="utf-8").splitlines()
        ecc = (a.root / CACHE["ecc"]).read_text(encoding="utf-8").splitlines()
        psd = (a.root / CACHE["psd"]).read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        print(f"measmod_registry_facts.py: {exc} (run `python3 tools/fetch.py fetch`)", file=sys.stderr)
        return 2

    got: dict[str, int] = {}
    sid_s, sid_e = block(slrf, "SITE/ID"), block(ecc, "SITE/ID")
    sod_s = {ln.split()[-1]: site_key(ln) for ln in sid_s}
    sod_e: dict[str, tuple] = {}
    dup_identical = 0
    for ln in sid_e:
        s = ln.split()[-1]
        if s in sod_e:
            dup_identical += 1 if sod_e[s] == site_key(ln) else 0
        sod_e[s] = site_key(ln)
    pads_s, pads_e = {k[0] for k in sod_s.values()}, {k[0] for k in sod_e.values()}

    epochs = block(slrf, "SOLUTION/EPOCHS")
    solns: dict[tuple, list] = {}
    for ln in epochs:
        f = ln.split()
        solns.setdefault((f[0], f[1]), []).append((int(f[2]), snx_epoch(f[4]), snx_epoch(f[5], True)))
    gaps = 0
    for v in solns.values():
        v.sort()
        gaps += sum(1 for x, y in zip(v, v[1:]) if x[2] is not None and y[1] is not None and x[2] < y[1])
    markers_s = {(k[0], k[1]) for k in sod_s.values()}

    got["slrf_site_id_rows"], got["slrf_distinct_sods"], got["slrf_pads"] = len(sid_s), len(sod_s), len(pads_s)
    got["slrf_markers"], got["slrf_markers_with_solution"] = len(markers_s), len(solns)
    got["slrf_solutions"] = len(epochs)
    got["slrf_markers_with_more_than_one_soln"], got["slrf_gaps_between_solutions"] = sum(1 for v in solns.values() if len(v) > 1), gaps
    header = [ln for ln in slrf[:200] if "unique sites" in ln]
    got["slrf_header_sites"] = int(header[0].split("for")[1].split()[0]) if header else -1
    got["pads_added_after_header"] = len(pads_s & {"7329", "7317"}) if len(pads_s - {"7329", "7317"}) == got["slrf_header_sites"] else -1
    got["ecc_site_id_rows"], got["ecc_distinct_sods"], got["ecc_pads"] = len(sid_e), len(sod_e), len(pads_e)
    got["ecc_identical_duplicate_rows"] = dup_identical

    disagree = [s for s in set(sod_s) & set(sod_e) if sod_s[s] != sod_e[s]]
    got["pad_point_domes_disagreements"] = len(disagree)
    placed = [s for s in sod_e if s in sod_s and sod_s[s] == sod_e[s] and (sod_s[s][0], sod_s[s][1]) in solns]
    unplaced = [s for s in sod_e if s not in placed]
    got["placed_sods"], got["unplaced_sods"] = len(placed), len(unplaced)
    got["unplaced_pad_absent"] = sum(1 for s in unplaced if sod_e[s][0] not in pads_s)
    got["unplaced_sod_absent_on_known_pad"] = sum(1 for s in unplaced if sod_e[s][0] in pads_s and s not in sod_s)
    got["unplaced_marker_without_solution"] = sum(1 for s in unplaced if s in sod_s and (sod_s[s][0], sod_s[s][1]) not in solns)

    events: dict[str, list[str]] = {}
    for ln in psd:
        f = ln.split()
        if len(f) >= 4 and f[0].isdigit() and f[3].count(":") == 2:
            events.setdefault(f[0], []).append(f[3])
    got["psd_sites"], got["psd_events"] = len(events), sum(len(v) for v in events.values())

    width = max(len(k) for k in got)
    bad = 0
    for k, v in got.items():
        mark = ""
        if a.check and EXPECTED[k] != v:
            mark, bad = f"   <- EXPECTED {EXPECTED[k]}", bad + 1
        print(f"  {k:<{width}}  {v:>5}{mark}")
    print("  PSD events (site: SINEX epochs):", {k: v for k, v in events.items()})
    print("  unplaced SODs on pads SLRF2020 lists:", sorted(s for s in unplaced if sod_e[s][0] in pads_s))
    if a.check:
        print("ok       every pinned number matches" if not bad else f"FAILED   {bad} number(s) differ")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
