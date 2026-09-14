#!/bin/sh
# update_iersb.sh - rebuild res/eopc04, the Earth orientation table.
#
# Frame_transform refuses any epoch past the end of this table, so how far the
# software can propagate is set here and nowhere else.
#
# Two IERS products are combined:
#
#   EOP 14 C04   the definitive solution, but only for days already measured
#                and reprocessed - it currently ends months behind today.
#   finals.all   Bulletin A: measured values to within a few days of now, then
#                about a year of predictions.
#
# C04 wins wherever it exists (it is the better solution, and covers 1962-1972
# which finals does not); finals supplies everything after it. Re-run this
# periodically: each run pulls newly measured days in over the predicted ones.
#
# Predicted days are fine for planning but not for precise work. UT1-UTC
# prediction error grows to roughly tens of milliseconds a year out, and 1 ms
# of UT1 is about 0.46 m of position at the Earth's surface.

set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEST=$ROOT/res/eopc04
C04URL="https://datacenter.iers.org/data/latestVersion/EOP_14_C04_IAU1980_one_file_1962-now.txt"
FINURL="https://datacenter.iers.org/data/latestVersion/finals.all.iau1980.txt"

TMPC04=$(mktemp)
TMPFIN=$(mktemp)
TMPOUT=$(mktemp)
trap 'rm -f "$TMPC04" "$TMPFIN" "$TMPOUT"' EXIT

fetch() {
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL --retry 3 "$1" -o "$2"
    else
        wget -O "$2" "$1"
    fi
}

echo "Downloading IERS EOP 14 C04 ..."
fetch "$C04URL" "$TMPC04"
echo "Downloading IERS finals (Bulletin A) ..."
fetch "$FINURL" "$TMPFIN"

python3 "$ROOT/scripts/build_eop.py" "$TMPC04" "$TMPFIN" "$TMPOUT"

# The reader skips exactly 14 header lines and then expects contiguous daily
# records, so check that before replacing a working file.
if ! awk 'NR==15 { exit !(NF >= 10 && $4 > 30000 && $4 < 90000) }' "$TMPOUT"; then
    echo "Generated file does not start records on line 15; not installing." >&2
    exit 1
fi

[ -f "$DEST" ] && cp "$DEST" "$DEST.bak"
cp "$TMPOUT" "$DEST"
echo "Installed $DEST"
