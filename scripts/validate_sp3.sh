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

# Sampling interval, taken from the SP3 header rather than assumed: IGS finals
# are 15-minute, the multi-GNSS products are 5-minute, and passing the wrong one
# silently misaligns the fit against the reference.
INTERVAL=$(awk 'NR==2 { printf "%d", $3 }' "$SP3DIR/$SP3NAME")
[ -z "$INTERVAL" ] || [ "$INTERVAL" -le 0 ] && INTERVAL=900

echo
OUT=$(./fit_orbit_to_sp3_v3 "$CFG" "$ECI" "$INTERVAL" 2>&1)
echo "$OUT"

# --- assert the run used the configuration we think it did -------------------
# A substitution that silently fails to apply reports a number computed from
# some other configuration, and nothing in the output says so. This happened
# during development: a sed that did not match left an earlier hand-tuned
# config in place, and the run looked entirely normal. Fail loudly instead.
fail=0
say_fail() { echo "ASSERTION FAILED: $1" >&2; fail=1; }

got_cfg=$(echo "$OUT"  | awk -F': ' '/^CONFIG  file/{print $2}' | tr -d ' ')
[ "$got_cfg" = "$CFG" ] || say_fail "fit read '$got_cfg', expected '$CFG'"

# area and mass must match the template that was actually requested
cfgline='^CONFIG  area\/mass  : \([0-9.eE+-]*\) m^2 \/ \([0-9.eE+-]*\) kg.*'
got_area=$(echo "$OUT" | sed -n "s|$cfgline|\1|p")
got_mass=$(echo "$OUT" | sed -n "s|$cfgline|\2|p")
want_area=$(awk '$1=="area" { print $3; exit }' "$TEMPLATE")
want_mass=$(awk '$1=="mass" { print $3; exit }' "$TEMPLATE")
[ -z "$want_area" ] || [ "$want_area" = "$got_area" ] ||
    say_fail "area is $got_area, template says $want_area"
[ -z "$want_mass" ] || [ "$want_mass" = "$got_mass" ] ||
    say_fail "mass is $got_mass, template says $want_mass"

# the epoch must be the reference arc's first record, not a leftover from
# whichever config was edited last
want_day=$(awk 'NR==1 { printf "%d/%2d/%d", $4, $3, $2 }' "$ECI")
echo "$OUT" | grep -q "CONFIG  epoch      : $want_day" ||
    say_fail "epoch is not the reference arc's first record ($want_day)"

if [ "$fail" -ne 0 ]; then
    echo >&2
    echo "The numbers above were NOT produced by the intended configuration." >&2
    exit 3
fi

echo
echo "  configuration asserted: $(basename "$TEMPLATE"), ${INTERVAL}s sampling"

# --- regression baselines ----------------------------------------------------
# Known-good results, so this script is a test rather than something someone
# remembers to eyeball. Tolerance is 10%: the fit is deterministic, so anything
# outside that is a real change in the dynamics, not noise. Add arcs freely.
RMS=$(echo "$OUT" | awk '/position RMS  *:/{print $4}')
case "${SAT}_${STAMP}_$(basename "$TEMPLATE")" in
    G01_20230220000_configOPS_gnss.txt) BASE=0.0645 ;;
    G01_20230230000_configOPS_gnss.txt) BASE=0.0573 ;;
    G05_20230220000_configOPS_gnss.txt) BASE=0.1870 ;;
    *) BASE="" ;;
esac

if [ -n "$BASE" ]; then
    ok=$(awk -v a="$RMS" -v b="$BASE" \
        'BEGIN { d=(a-b); if(d<0)d=-d; print (d <= 0.10*b) ? "y" : "n" }')
    if [ "$ok" = y ]; then
        echo "  baseline OK: $RMS m against $BASE m"
    else
        echo "REGRESSION: $SAT $STAMP gave $RMS m, baseline $BASE m (>10%)" >&2
        exit 4
    fi
fi
