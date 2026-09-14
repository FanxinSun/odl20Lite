#!/bin/sh
# validate_sp3.sh - measure how well this software reproduces an independently
# determined orbit, by fitting to an IGS precise ephemeris.
#
# Usage:
#   scripts/validate_sp3.sh                 # default arc, downloads if needed
#   scripts/validate_sp3.sh <gpsweek> <yyyydddhhmm> <satid>
#
# e.g. scripts/validate_sp3.sh 2246 20230220000 G01
#
# IGS final orbits are good to about 2.5 cm, so for this purpose they are
# truth. The residual RMS reported at the end is the accuracy figure for this
# software's force models over the arc - the one number worth quoting.
#
# Note what is and is not being tested. The fit solves for the six initial
# state elements, so an error that looks like a state offset is absorbed. What
# it cannot absorb is mismodelled dynamics, which is what shows up in the
# residuals. The spacecraft area in the config matters a great deal at GPS
# altitude, where solar radiation pressure is the largest error source: with
# the default 10 m^2 the residuals are ~22 m, and at the published GPS
# area-to-mass ratio they fall below half a metre.

set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)

WEEK=${1:-2246}
STAMP=${2:-20230220000}
SAT=${3:-G01}
TEMPLATE=${4:-$ROOT/res/configOPS_gnss.txt}

SP3DIR=$ROOT/res/sp3
SP3NAME=IGS0OPSFIN_${STAMP}_01D_15M_ORB.SP3
URL=https://igs.bkg.bund.de/root_ftp/IGS/products/${WEEK}/${SP3NAME}.gz

mkdir -p "$SP3DIR" "$ROOT/output"

if [ ! -f "$SP3DIR/$SP3NAME" ]; then
    echo "Fetching $SP3NAME ..."
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "$URL" -o "$SP3DIR/$SP3NAME.gz"
    else
        wget -q -O "$SP3DIR/$SP3NAME.gz" "$URL"
    fi
    gunzip -f "$SP3DIR/$SP3NAME.gz"
fi

ECI=$ROOT/output/${SAT}_${STAMP}.eci
CFG=$ROOT/output/${SAT}_${STAMP}.cfg

cd "$ROOT/bin"

echo
./SP3_to_eci "$SAT" "$ECI" "$SP3DIR/$SP3NAME"

# Build a config for this arc: the template supplies the spacecraft and force
# models, and the epoch and starting state come from the reference itself.
# The fit only needs to start close enough to converge.
set -- $(head -1 "$ECI")
YEAR=$2; MON=$3; DAY=$4; HR=$5; MIN=$6; SEC=$7
X=$8; Y=$9
shift 9
Z=$1; U=$2; V=$3; W=$4

awk -v y="$YEAR" -v mo="$MON" -v d="$DAY" -v h="$HR" -v mi="$MIN" -v s="$SEC" \
    -v x="$X" -v yy="$Y" -v z="$Z" -v u="$U" -v v="$V" -v w="$W" '
    /^t0 / { printf "t0                  = %04d,%02d,%02d,%02d,%02d,%05.2f\n", y,mo,d,h,mi,s; next }
    /^x0 / { print "x0                  = " x;  next }
    /^y0 / { print "y0                  = " yy; next }
    /^z0 / { print "z0                  = " z;  next }
    /^u0 / { print "u0                  = " u;  next }
    /^v0 / { print "v0                  = " v;  next }
    /^w0 / { print "w0                  = " w;  next }
    { print }
' "$TEMPLATE" > "$CFG"

echo
./fit_orbit_to_sp3_v3 "$CFG" "$ECI" 900
