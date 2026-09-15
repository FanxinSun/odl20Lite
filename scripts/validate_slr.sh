#!/bin/sh
# validate_slr.sh - fit an orbit to real satellite laser ranging.
#
# Usage:
#   scripts/validate_slr.sh                  # LightSail-2, Aug-Sep 2019
#   scripts/validate_slr.sh <srp_scale>      # with the scale fixed, for a scan
#
# What this is for. Every other check in this tree compares against a
# trajectory somebody else computed, and for anything that is not a GNSS
# satellite that trajectory comes, directly or indirectly, from two-line
# elements. Laser ranging is different: a normal point is a measured round trip
# time, good to a centimetre or two, with no orbit model behind it.
#
# The target is LightSail-2 - a 32 m^2 sail on a 5.035 kg CubeSat, 6.36 m^2/kg.
# It is the ONLY high area-to-mass object in the public laser ranging archive;
# everything else the ILRS tracks is a dense sphere or a large spacecraft,
# because geodetic targets are deliberately built to make non-gravitational
# forces small. That makes this the one real test of the radiation pressure
# work in this tree against something other than itself.
#
# Read the residual with the arithmetic in mind. The data is capable of
# centimetres and the fit reaches tens of metres, so what is left is the force
# model and the things it leaves out - the retroreflector's offset from the
# centre of mass, tidal displacement of the station, and above all the sail's
# actual attitude history - not the measurement.

set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)

SCALE=${1:-}
EDC=https://edc.dgfi.tum.de/pub/slr
SLRDIR=$ROOT/res/slr
OBS=$SLRDIR/lightsail2.slrobs

mkdir -p "$SLRDIR"

fetch() { # url, destination
    [ -f "$2" ] && return 0
    echo "Fetching $(basename "$2") ..."
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "$1" -o "$2" || { rm -f "$2"; return 1; }
    else
        wget -q "$1" -O "$2" || { rm -f "$2"; return 1; }
    fi
}

# --- the measurements -------------------------------------------------------
# Public, no credentials. CDDIS holds the same data behind an Earthdata login.
for m in 201908 201909; do
    fetch "$EDC/data/npt_crd/lightsail2/2019/lightsail2_$m.npt" \
          "$SLRDIR/lightsail2_$m.npt" || {
        echo "Could not fetch normal points; is the network up?" >&2
        exit 1
    }
done

if [ ! -f "$OBS" ]; then
    python3 "$ROOT/scripts/crd2obs.py" "$SLRDIR"/lightsail2_2019*.npt "$OBS"
fi

# --- the starting guess -----------------------------------------------------
# The ILRS prediction the stations pointed with. It is wrong almost entirely
# along track - about 3.5 s a day for this object, which is 26 km a day - so
# cpf2eci.py takes a time bias out before handing over an initial state. The
# config already carries the result; this is here so it can be regenerated.
CPF=$SLRDIR/lightsail2_cpf_190831_7428.nxt
fetch "$EDC/cpf_predicts/2019/lightsail2/lightsail2_cpf_190831_7428.nxt" "$CPF" \
    || echo "note: CPF prediction not fetched; the config's state still works"

# --- the fit ----------------------------------------------------------------
CFG=$ROOT/res/configOPS_lightsail2.txt
if [ -n "$SCALE" ]; then
    CFG=${TMPDIR:-/tmp}/lightsail2_$SCALE.txt
    sed -e "s/^spacecraft          = testRSO/srp_scale           = $SCALE\nspacecraft          = testRSO/" \
        "$ROOT/res/configOPS_lightsail2.txt" > "$CFG"
fi

cd "$ROOT/bin"
# The prior is the prediction, and it is anisotropic on purpose: a kilometre
# radially and across track, fifty along it. An isotropic prior either pins the
# one direction that has to move or lets the orbit wander sideways into a
# solution that fits the ranges and is not an orbit.
./fit_orbit_to_slr "$CFG" "$OBS" --seven --apriori-rtn 1 50 1 0.05 \
    | tee "${TMPDIR:-/tmp}/slrfit.$$"

RMS=$(awk '/range residual RMS/{print $5}' "${TMPDIR:-/tmp}/slrfit.$$")
rm -f "${TMPDIR:-/tmp}/slrfit.$$"

echo
echo "-----------------------------------------"
if [ -z "$SCALE" ]; then
    BASE=57.177
    ok=$(awk -v a="$RMS" -v b="$BASE" \
        'BEGIN{ d=(a-b)/b; if(d<0)d=-d; print (d<0.10)?"y":"n" }')
    if [ "$ok" = y ]; then
        echo "  baseline OK: $RMS m against $BASE m"
    else
        echo "REGRESSION: LightSail-2 gave $RMS m, baseline $BASE m (>10%)" >&2
        exit 1
    fi
fi
