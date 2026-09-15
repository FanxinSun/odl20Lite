#!/bin/sh
# validate_angles.sh - fit an orbit to real optical angles, and measure how
# little one pass determines.
#
# Usage:
#   scripts/validate_angles.sh                 # shipped observations
#   scripts/validate_angles.sh --refresh       # re-harvest from SeeSat-L first
#
# The target is NASA's ACS3 solar sail: 80 m^2 on 16 kg, 5.0 m^2/kg. It carries
# no retroreflector, so there is no laser ranging for it and there never will
# be - which is exactly the position every object in the population this
# software is aimed at is in. What exists is angles, measured by one amateur
# observer in England on a handful of nights.
#
# Two numbers matter here and they pull against each other. The residual says
# how well the force model reproduces where the object was seen. The condition
# number says how much of the answer is the data and how much is the prior. A
# single short pass from one site fixes a direction and its rate and says
# nothing about range, so the orbit is not determined at all; loosening the
# prior improves the residual and makes the geometry worse, and that trade is
# the whole difficulty of tracking uncooperative objects.

set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OBS=$ROOT/res/angles/acs3.angles

if [ "${1:-}" = "--refresh" ]; then
    echo "Re-harvesting SeeSat-L; this fetches a few thousand small pages."
    python3 "$ROOT/scripts/seesat2angles.py" 2407702 "$OBS" \
        --cache "${TMPDIR:-/tmp}/seesat_cache" --from 2024-09
fi

if [ ! -f "$OBS" ]; then
    echo "No observations at $OBS; run with --refresh" >&2
    exit 1
fi

echo
echo "--- initial orbit determination, from the angles alone ---"
# No element set anywhere in this chain: the starting orbit comes from the three
# angles of the first pass and a circular assumption, which is what an object
# with no catalogue entry leaves you.
python3 "$ROOT/scripts/angles_iod.py" "$OBS" "$ROOT/res/obs_sites.txt" \
    --night 2024-09-03

echo
echo "--- fit, and the price of the prior ---"
cd "$ROOT/bin"
for ap in "5000 5.0" "500 1.0" "100 0.2"; do
    out=$(./fit_orbit_to_angles "$ROOT/res/configOPS_acs3_angles.txt" "$OBS" \
              --six --apriori $ap --sigma 20 2>/dev/null)
    rms=$(echo "$out" | awk '/angular residual RMS/{print $5}')
    cond=$(echo "$out" | awk '/condition number/{print $6}')
    printf "  prior %-11s  residual %7s arcsec   condition %s\n" \
        "$ap" "$rms" "$cond"
done

echo
BASE=26.14
RMS=$(./fit_orbit_to_angles "$ROOT/res/configOPS_acs3_angles.txt" "$OBS" \
          --six --apriori 500 1.0 --sigma 20 2>/dev/null |
      awk '/angular residual RMS/{print $5}')
echo "-----------------------------------------"
ok=$(awk -v a="$RMS" -v b="$BASE" \
    'BEGIN{ d=(a-b)/b; if(d<0)d=-d; print (d<0.10)?"y":"n" }')
if [ "$ok" = y ]; then
    echo "  baseline OK: $RMS arcsec against $BASE arcsec"
else
    echo "REGRESSION: ACS3 angles gave $RMS arcsec, baseline $BASE" >&2
    exit 1
fi
