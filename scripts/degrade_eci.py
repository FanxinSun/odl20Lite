#!/usr/bin/env python3
"""Degrade a reference ephemeris, to find where orbit fitting stops working.

The arcs this tree validates against are the easiest case there is: 22 hours of
continuous 15-minute sampling of a large, cooperative, continuously tracked
satellite. Real tracking of a small uncooperative object is short, sparse,
irregular and noisy. This takes a working arc and degrades it along those axes
so the breakdown point can be measured rather than guessed.

Usage:
    degrade_eci.py <in.eci> <out.eci> [options]

    --hours H      keep only the first H hours
    --every N      keep every Nth record (thins the sampling)
    --passes N:M   irregular: N passes of M minutes, spread evenly over the arc.
                   Closer to real tracking than uniform thinning - the same
                   number of observations bunched into a few visibilities
                   constrains an orbit far less than the same number spread out.
    --noise M      add Gaussian noise of M metres to each position component
    --seed S       RNG seed, so a degraded arc is reproducible (default 1)

Epochs are never moved, only dropped, because the fitter matches observations to
propagated states by epoch and every epoch must stay on the propagation grid.
"""

import math
import random
import sys


def main(argv):
    if len(argv) < 3:
        raise SystemExit(__doc__)

    src, dst = argv[1], argv[2]
    hours = None
    every = 1
    passes = None
    noise = 0.0
    seed = 1

    i = 3
    while i < len(argv):
        a = argv[i]
        if a == "--hours":
            hours = float(argv[i + 1]); i += 2
        elif a == "--every":
            every = int(argv[i + 1]); i += 2
        elif a == "--passes":
            passes = argv[i + 1]; i += 2
        elif a == "--noise":
            noise = float(argv[i + 1]); i += 2
        elif a == "--seed":
            seed = int(argv[i + 1]); i += 2
        else:
            raise SystemExit("unknown option: " + a)

    rows = [l.rstrip("\n") for l in open(src) if l.strip()]
    if not rows:
        raise SystemExit("empty input")

    # Seconds from the first record, from the calendar fields, so a dropped
    # record never shifts anything after it.
    def stamp(line):
        f = line.split()
        d, h, m, s = int(f[3]), int(f[4]), int(f[5]), float(f[6])
        return ((d * 24 + h) * 60 + m) * 60.0 + s

    t0 = stamp(rows[0])
    times = [stamp(r) - t0 for r in rows]

    keep = list(range(len(rows)))

    if hours is not None:
        limit = hours * 3600.0
        keep = [i for i in keep if times[i] <= limit + 1.0]

    if passes:
        n_pass, minutes = passes.split(":")
        n_pass, minutes = int(n_pass), float(minutes)
        span = times[keep[-1]] - times[keep[0]]
        width = minutes * 60.0
        # Spread the pass centres evenly, then keep whatever falls inside one.
        centres = [times[keep[0]] + span * (k + 0.5) / n_pass
                   for k in range(n_pass)]
        keep = [i for i in keep
                if any(abs(times[i] - c) <= width / 2.0 for c in centres)]
    elif every > 1:
        base = keep[0]
        keep = [i for i in keep if (i - base) % every == 0]

    if len(keep) < 8:
        print("warning: only %d observations left; the fit needs at least 8"
              % len(keep), file=sys.stderr)

    rng = random.Random(seed)
    out = []
    for i in keep:
        f = rows[i].split()
        if noise > 0.0:
            # Positions are in km in this format; noise is quoted in metres.
            for c in (7, 8, 9):
                f[c] = "%.6f" % (float(f[c]) + rng.gauss(0.0, noise / 1000.0))
        out.append("%3s %5s %3s %3s %3s %3s %10s %18s%18s%18s%18s%18s%18s"
                   % tuple(f[:13]))

    with open(dst, "w") as fh:
        fh.write("\n".join(out) + "\n")

    span_h = (times[keep[-1]] - times[keep[0]]) / 3600.0 if keep else 0.0
    print("%d of %d records, %.2f h span, noise %.1f m"
          % (len(out), len(rows), span_h, noise))


if __name__ == "__main__":
    main(sys.argv)
