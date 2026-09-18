#!/bin/sh
# capture.sh — run the predecessor as a black box and freeze what it says.
#
# This is the whole of plan rule R2 made operational. The predecessor's role in
# this project is a test oracle: a program that is RUN and never READ. Running it
# is explicitly permitted to anyone, including a session that has never seen its
# source, because observing what a program outputs is not derived from its
# expression. Reading it is what is forbidden.
#
# The intent is that this script is run once, its output is frozen, and the
# predecessor is then never touched again by this project. An oracle that has
# already answered every question cannot tempt anyone into opening it.
#
# Usage:  oracle/capture.sh [outdir]      (default: the directory holding this file)
#
# WHAT THIS EMITS, and what it deliberately does not. It emits extracted scalar
# values, the exact command that produced each, and hashes of every input. It does
# NOT emit raw program output: logs carry configuration echoes, internal file names
# and diagnostic text, and none of that is a measurement. The artefact is numbers
# and provenance, nothing else.
#
# THE INTERPRETATION CAVEAT, which matters more than any number below. The
# predecessor computes with IAU-76/1980 precession-nutation and consumes the IAU
# 1980 variants of the IERS products. The rewrite uses IAU 2006/2000A and the IAU
# 2000A variants. Certain disagreements between the two are therefore REQUIRED, of
# a predictable size, and agreement would be the failure. See ORACLE.md §4.

set -u
# The predecessor is this repository's root: since the 2026-09-18 merge the
# rewrite lives in rewrite/ inside it, rather than in a tree of its own. Resolved
# RELATIVELY so that moving the tree cannot break this again - it already did
# once, and a frozen cases.tsv whose capture script exits 1 is an authority with
# no route back to what produced it. ODL_PREDECESSOR overrides, for the case
# where the two are split apart again.
OLD=${ODL_PREDECESSOR:-$(cd "$(dirname "$0")/../.." && pwd)}
OUT=${1:-$(cd "$(dirname "$0")" && pwd)}
CASES=$OUT/cases.tsv
ENVF=$OUT/environment.txt

[ -d "$OLD" ] || { echo "predecessor tree not at $OLD" >&2; exit 1; }
[ -x "$OLD/bin/sgnlOPS" ] || { echo "predecessor not built; run 'make rebuild' there" >&2; exit 1; }

now=$(date -u +%Y-%m-%dT%H:%M:%SZ)

# ---- environment ----------------------------------------------------------
{
  echo "# Oracle environment, captured $now"
  echo
  echo "predecessor_commit\t$(git -C "$OLD" rev-parse HEAD 2>/dev/null)"
  echo "predecessor_describe\t$(git -C "$OLD" log --oneline -1 2>/dev/null | cut -c1-60)"
  echo "predecessor_dirty\t$(test -n "$(git -C "$OLD" status --porcelain 2>/dev/null)" && echo yes || echo no)"
  echo "compiler\t$(g++ --version 2>/dev/null | head -1)"
  echo "host\t$(uname -sr)"
  echo
  echo "# Input data, by SHA-256. Every number below is conditional on these."
  for f in res/eopc04 res/1980_2020 res/SW-All.csv \
           res/teme_check/acs3.tle res/teme_check/acs3_horizons_20260914.eci \
           res/slr/lightsail2.slrobs res/slr_stations.txt \
           res/angles/acs3.angles res/obs_sites.txt; do
    if [ -f "$OLD/$f" ]; then
      printf '%s\t%s\n' "$f" "$(sha256sum "$OLD/$f" | cut -c1-64)"
    fi
  done
  for f in "$OLD"/res/sp3/*.SP3; do
    [ -f "$f" ] && printf 'res/sp3/%s\t%s\n' "$(basename "$f")" "$(sha256sum "$f" | cut -c1-64)"
  done
} > "$ENVF" 2>/dev/null

# ---- case recorder --------------------------------------------------------
: > "$CASES"
# The header is emitted here, not only kept in the frozen copy: a regeneration
# that dropped it would leave the file depending on this script to be read, which
# is the defect it exists to remove.
{
  echo "# Frozen predecessor measurements. See ORACLE.md; capture.sh regenerates all but B-* and E-*."
  echo "#"
  echo "# The F-* cases are this ECEF state, which was previously only in capture.sh and so"
  echo "# made this file not self-contained:"
  echo "#   position  6373.144386  -3485.243421   2605.215522  km"
  echo "#   velocity    -0.689758464  3.306816989  6.103804423  km/s"
  echo "#   epoch     MJD 57372.37458333 (F-01..F-04), MJD 60827.0 (F-05)"
  echo "#"
} >> "$CASES"
printf 'id\tquantity\tvalue\tunit\tcommand\n' >> "$CASES"
rec() { printf '%s\t%s\t%s\t%s\t%s\n' "$1" "$2" "$3" "$4" "$5" >> "$CASES"; }

cd "$OLD/bin" || exit 1

# ---- frames: the P1 tranche's oracle cases --------------------------------
EP=57372.37458333
SV="6373.144386 -3485.243421 2605.215522 -0.689758464 3.306816989 6.103804423"

eci=$(./ECEF2ECI $SV $EP 2>/dev/null | awk '/^ECI:/{ $1=""; print }')
rec F-01 "ECEF->ECI x at MJD $EP" "$(echo "$eci" | awk '{print $1}')" km "ECEF2ECI <state> $EP"
rec F-02 "ECEF->ECI y at MJD $EP" "$(echo "$eci" | awk '{print $2}')" km "ECEF2ECI <state> $EP"
rec F-03 "ECEF->ECI z at MJD $EP" "$(echo "$eci" | awk '{print $3}')" km "ECEF2ECI <state> $EP"

back=$(./ECI2ECEF $eci $EP 2>/dev/null | awk '/^ECEF:/{ $1=""; print }')
rt=$(echo "$back" | awk '{dx=$1-6373.144386; dy=$2+3485.243421; dz=$3-2605.215522;
      printf "%.12f", sqrt(dx*dx+dy*dy+dz*dz)}')
rec F-04 "ECEF->ECI->ECEF round-trip closure" "$rt" km "ECEF2ECI then ECI2ECEF, same epoch"

eci2=$(./ECEF2ECI $SV 60827.0 2>/dev/null | awk '/^ECI:/{print $2}')
rec F-05 "ECEF->ECI x at MJD 60827.0" "$eci2" km "ECEF2ECI <state> 60827.0"

# ---- TEME: the frame-conversion gate --------------------------------------
T=$OLD/res/teme_check
if [ -f "$T/acs3.tle" ]; then
  cfg=$(mktemp); out=$(mktemp)
  { printf 'tle0 = ACS3\n'
    printf 'tle1 = %s\n' "$(sed -n 2p "$T/acs3.tle" | tr -d '\r')"
    printf 'tle2 = %s\n' "$(sed -n 3p "$T/acs3.tle" | tr -d '\r')"
    printf 'spacecraft = testRSO\nmass = 16\narea = 80\npropagator = 2\n'
    printf 'step_size = 10\nsimulation_time = 18000\noutput_interval = 900\n'
    printf 'output_format = eci\ngravity_model = 8\ngrav_degree = 2\n'
    for k in magnetic_model antenna_thrust drag gr_correction srp erp trr \
             rp_model third_body y_bias pole_tide solid_earth_tide time_var_grav; do
      printf '%s = 0\n' "$k"
    done
  } > "$cfg"
  ./sgnlOPS "$cfg" "$out" >/dev/null 2>&1
  read -r mean mx <<EOF
$(paste "$T/acs3_horizons_20260914.eci" "$out" | awk '
   { n=NF/2; dx=$8-$(8+n); dy=$9-$(9+n); dz=$10-$(10+n)
     d=sqrt(dx*dx+dy*dy+dz*dz)*1000; s+=d; c++; if(d>mx)mx=d }
   END { printf "%.3f %.3f", (c?s/c:0), mx+0 }')
EOF
  rec T-01 "SGP4 TEME->J2000 vs Horizons, mean" "$mean" m "sgnlOPS, SGP4, ACS3 TLE, 5 h"
  rec T-02 "SGP4 TEME->J2000 vs Horizons, max" "$mx" m "sgnlOPS, SGP4, ACS3 TLE, 5 h"
  rm -f "$cfg" "$out"
fi

# ---- atmosphere: the density envelope --------------------------------------
if [ -f "$OLD/analyses/qbfanxin/ops_config_template.txt" ]; then
  cfg=$(mktemp); out=$(mktemp)
  sed -e 's/^propagator          = 2/propagator          = 1/' \
      -e 's/^step_size           = 10/step_size           = 5/' \
      -e 's/^simulation_time     = 252000/simulation_time     = 21600/' \
      -e 's/^drag                = 2/drag                = 3/' \
      "$OLD/analyses/qbfanxin/ops_config_template.txt" > "$cfg"
  ./sgnlOPS "$cfg" "$out" >/dev/null 2>&1
  read -r dmin dmax drat <<EOF
$(awk -F',' 'NR>1 { r=$18*1e-9; if(r>mx)mx=r; if(mn==0||r<mn)mn=r }
             END { printf "%.6e %.6e %.3f", mn, mx, mx/mn }' "$out")
EOF
  rec D-01 "NRLMSISE-00 density at ~725 km, 6 h orbit minimum" "$dmin" kg/m^3 "sgnlOPS, RK8/7, drag=3, qbfanxin template 6 h"
  rec D-02 "NRLMSISE-00 density at ~725 km, 6 h orbit maximum" "$dmax" kg/m^3 "same run, orbit maximum"
  rec D-03 "NRLMSISE-00 diurnal ratio max/min over the orbit" "$drat" ratio "same run"
  rm -f "$cfg" "$out"
fi

# ---- estimator campaigns ---------------------------------------------------
for a in "2246 20230220000 G01:G-01" "2246 20230230000 G01:G-02" "2246 20230220000 G05:G-03"; do
  args=${a%%:*}; id=${a##*:}
  v=$(cd "$OLD" && timeout 600 ./scripts/validate_sp3.sh $args 2>/dev/null |
      awk '/baseline OK|REGRESSION/{print $3}')
  rec "$id" "GNSS 7-parameter fit residual RMS ($args)" "${v:-FAILED}" m "validate_sp3.sh $args"
done

v=$(cd "$OLD" && timeout 600 ./scripts/validate_slr.sh 2>/dev/null |
    awk '/range residual RMS/{print $5}')
rec S-01 "LightSail-2 SLR range residual RMS" "${v:-FAILED}" m "validate_slr.sh"
v=$(cd "$OLD" && timeout 600 ./scripts/validate_slr.sh 2>/dev/null |
    awk '/effective A\*C_R\/m/{print $4}')
rec S-02 "LightSail-2 effective A*C_R/m" "${v:-FAILED}" m^2/kg "validate_slr.sh"

v=$(cd "$OLD" && timeout 600 ./scripts/validate_angles.sh 2>/dev/null |
    awk '/baseline OK|REGRESSION/{print $3}')
rec O-01 "ACS3 optical angular residual RMS" "${v:-FAILED}" arcsec "validate_angles.sh"

echo "captured $(( $(wc -l < "$CASES") - 1 )) cases -> $CASES"
echo "environment -> $ENVF"
