#!/bin/sh
# smoke_test.sh - quick check that a freshly built UCL ODL / SGNL OPS works.
#
# Usage: ./scripts/smoke_test.sh        (run from the repository root)
#
# Builds nothing; run "make rebuild" first. Note that an incremental "make"
# does not always relink the utilities against a changed object file, so use
# "make rebuild" after editing library sources.

set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=${TMPDIR:-/tmp}/odl_smoke.$$
mkdir -p "$OUT"
cd "$ROOT/bin" || exit 1   # the utilities resolve resources via ../res

fail=0
pass() { printf '  PASS  %s\n' "$1"; }
bad()  { printf '  FAIL  %s\n' "$1"; fail=$((fail + 1)); }

echo "UCL ODL smoke test"
echo

# --- orbit propagation ------------------------------------------------------
for cfg in configOPS.txt configOPS_meo.txt; do
    if ./sgnlOPS "../res/$cfg" "$OUT/$cfg.out" >"$OUT/$cfg.log" 2>&1 &&
       [ -s "$OUT/$cfg.out" ]; then
        pass "sgnlOPS $cfg ($(wc -l < "$OUT/$cfg.out" | tr -d ' ') rows)"
    else
        bad "sgnlOPS $cfg"
    fi
done

# --- Keplerian <-> Cartesian ------------------------------------------------
# a = 6971 km, e = 0.0573805766747955817, all angles zero: the state vector at
# perigee must be r = a(1-e) = 6571 km with v = sqrt(GM(1+e)/(a(1-e))).
kc=$(./kep2car 6971 0.0573805766747955817 0 0 0 0 2>/dev/null)
x=$(echo "$kc" | awk '/^x = /{print $3; exit}')
v=$(echo "$kc" | awk '/^v = /{print $3; exit}')
if [ "${x%%.*}" = "6571" ] && [ "${v%%.*}" = "8" ]; then
    pass "kep2car perigee radius/speed ($x km, $v km/s)"
else
    bad "kep2car perigee radius/speed (got x=$x v=$v)"
fi

./car2kep 6971 0 0 0 7.5461 0 >/dev/null 2>&1 && pass "car2kep runs" || bad "car2kep"

# --- frame transforms -------------------------------------------------------
# An ECEF->ECI->ECEF round trip must return the position it started from.
EP=57372.37458333333333
set -- 6373.144386 -3485.243421 2605.215522 -0.689758464 3.306816989 6.103804423
eci=$(./ECEF2ECI "$@" $EP 2>/dev/null | awk '/^ECI:/{ $1=""; print }')
if [ -n "$eci" ]; then
    back=$(./ECI2ECEF $eci $EP 2>/dev/null | awk '/^ECEF:/{ $1=""; print }')
    d=$(echo "$back" | awk -v a="$1" -v b="$2" -v c="$3" \
        '{ dx=$1-a; dy=$2-b; dz=$3-c; printf "%.9f", sqrt(dx*dx+dy*dy+dz*dz) }')
    ok=$(echo "$d" | awk '{ print ($1 < 1e-6) ? "y" : "n" }')
    [ "$ok" = y ] && pass "ECEF->ECI->ECEF round trip (${d} km)" \
                  || bad "ECEF->ECI->ECEF round trip (${d} km)"
else
    bad "ECEF2ECI produced no output"
fi

# --- present-day epochs -----------------------------------------------------
# The ephemeris shipped originally stopped at 16 Jan 2020 and the EOP table at
# 4 Jun 2019, so any recent date used to be refused. Both have been extended;
# this guards against regressing to a pre-2020-only build.
now=$(./ECEF2ECI 6373.144386 -3485.243421 2605.215522 \
                 -0.689758464 3.306816989 6.103804423 60827.0 2>&1)
if echo "$now" | grep -q '^ECI:'; then
    pass "ECEF2ECI at MJD 60827 (1 Jun 2025)"
else
    bad "ECEF2ECI at MJD 60827 ($(echo "$now" | head -1))"
fi

# A date inside the old ephemeris range must still give the same answer it did
# before the ephemeris was widened.
ref="-7134.398676408507486"
got=$(./ECEF2ECI 6373.144386 -3485.243421 2605.215522 \
                 -0.689758464 3.306816989 6.103804423 57372.37458333 2>/dev/null |
      awk '/^ECI:/{print $2}')
[ "$got" = "$ref" ] && pass "ECEF2ECI unchanged at MJD 57372 (ephemeris regression)" \
                    || bad "ECEF2ECI changed at MJD 57372: $got vs $ref"

# --- NRLMSISE-00 drag (drag = 3) --------------------------------------------
# The orbit in analyses/qbfanxin is outside the TIE-GCM tables, but NRLMSISE-00
# reads no grid, so it must run the whole way through. Use a numerical
# propagator: with SGP4 the force models are not integrated at all.
nrl=$OUT/nrl.cfg
sed -e 's/^propagator          = 2/propagator          = 1/' \
    -e 's/^step_size           = 10/step_size           = 5/' \
    -e 's/^simulation_time     = 252000/simulation_time     = 21600/' \
    -e 's/^drag                = 2/drag                = 3/' \
    ../analyses/qbfanxin/ops_config_template.txt > "$nrl"

if ./sgnlOPS "$nrl" "$OUT/nrl.out" >"$OUT/nrl.log" 2>&1 && [ -s "$OUT/nrl.out" ]; then
    # Density must be physically plausible at ~725 km (1e-15..1e-11 kg/m^3)
    # and must vary around the orbit: NRLMSISE-00 has a diurnal bulge, so a
    # constant column would mean the model is not seeing time or position.
    read -r lo hi <<EOF
$(awk -F',' 'NR>1 { r=$18*1e-9; if(r>mx)mx=r; if(mn==0||r<mn)mn=r }
             END { printf "%.6e %.6e", mn, mx }' "$OUT/nrl.out")
EOF
    ok=$(awk -v a="$lo" -v b="$hi" \
        'BEGIN{ print (a>1e-15 && b<1e-11 && b/a>1.5) ? "y" : "n" }')
    if [ "$ok" = y ]; then
        pass "NRLMSISE-00 drag ($(awk -v a="$lo" -v b="$hi" \
              'BEGIN{printf "%.2e-%.2e kg/m3, %.1fx diurnal", a, b, b/a}'))"
    else
        bad "NRLMSISE-00 density implausible or constant (min=$lo max=$hi)"
    fi
else
    bad "NRLMSISE-00 drag run failed"
fi

# --- alternative ephemeris formats (optional) -------------------------------
# With external/calceph built, SGNL_EPHEMERIS may point at a stock JPL binary,
# an INPOP file or a SPICE kernel instead of FECsoft's own container. Those
# files are large and not shipped, so this only runs when one is offered:
#
#   SGNL_TEST_EPHEMERIS=/path/to/de440s.bsp ./scripts/smoke_test.sh
#
# The answer must match the FECsoft reader, which is the point of the exercise.
if [ -n "${SGNL_TEST_EPHEMERIS:-}" ] && [ -f "${SGNL_TEST_EPHEMERIS}" ]; then
    alt=$(SGNL_EPHEMERIS="$SGNL_TEST_EPHEMERIS" ./ECEF2ECI \
              6373.144386 -3485.243421 2605.215522 \
              -0.689758464 3.306816989 6.103804423 57372.37458333 2>&1 |
          awk '/^ECI:/{print $2}')
    if [ -z "$alt" ]; then
        bad "alternative ephemeris $(basename "$SGNL_TEST_EPHEMERIS") not readable"
    else
        # Agreement to 1e-6 km; the two readers interpolate independently.
        close=$(awk -v a="$alt" -v b="-7134.398676408507486" \
                'BEGIN{ d=a-b; if(d<0)d=-d; print (d<1e-6)?"y":"n" }')
        [ "$close" = y ] &&
            pass "alternative ephemeris $(basename "$SGNL_TEST_EPHEMERIS") agrees" ||
            bad "alternative ephemeris disagrees: $alt"
    fi
fi

# --- TEME to J2000, against a mission ephemeris ------------------------------
# SGP4 works in TEME; everything else here is J2000. The two differ by
# precession and nutation since J2000 - tens of kilometres by 2026 - and it is a
# pure rotation, so it leaves |r| alone and hides in anything that only depends
# on altitude. res/teme_check holds a TLE and the same object over the same
# hours from JPL Horizons, so this is an end-to-end check with no network. It
# checks the frame conversion, not the orbit: forward of a TLE epoch Horizons'
# ACS3 ephemeris is that same TLE.
TEMEDIR=$ROOT/res/teme_check
if [ -f "$TEMEDIR/acs3.tle" ] && [ -f "$TEMEDIR/acs3_horizons_20260914.eci" ]; then
    {
        printf 'tle0 = ACS3\n'
        printf 'tle1 = %s\n' "$(sed -n 2p "$TEMEDIR/acs3.tle" | tr -d '\r')"
        printf 'tle2 = %s\n' "$(sed -n 3p "$TEMEDIR/acs3.tle" | tr -d '\r')"
        cat <<'CFG'
spacecraft          = testRSO
mass                = 16
area                = 80
propagator          = 2
step_size           = 10
simulation_time     = 18000
output_interval     = 900
output_format       = eci
gravity_model       = 8
grav_degree         = 2
magnetic_model      = 0
antenna_thrust      = 0
drag                = 0
gr_correction       = 0
srp                 = 0
erp                 = 0
trr                 = 0
rp_model            = 0
third_body          = 0
y_bias              = 0
pole_tide           = 0
solid_earth_tide    = 0
time_var_grav       = 0
CFG
    } > "$OUT/teme.cfg"

    if ./sgnlOPS "$OUT/teme.cfg" "$OUT/teme.out" >/dev/null 2>&1 &&
       [ -s "$OUT/teme.out" ]; then
        read -r mean mx <<EOF
$(paste "$TEMEDIR/acs3_horizons_20260914.eci" "$OUT/teme.out" | awk '
   { n=NF/2; dx=$8-$(8+n); dy=$9-$(9+n); dz=$10-$(10+n)
     d=sqrt(dx*dx+dy*dy+dz*dz)*1000; s+=d; c++; if(d>mx)mx=d }
   END { printf "%.1f %.1f", (c?s/c:9e9), mx+0 }')
EOF
        # 2.2 m with the conversion, 34000 m without. 20 m is far below the
        # failure and well above the residual, which is the 0.064 arcsec
        # between this code's IAU-76/80 precession-nutation and the
        # IAU-2006/2000A Horizons uses. It is also tight enough to catch a
        # reference regenerated at a rounded epoch: 10 ms of rounding is 24 m
        # along track here. See res/teme_check/README.txt.
        ok=$(awk -v a="$mx" 'BEGIN{ print (a < 20) ? "y" : "n" }')
        [ "$ok" = y ] &&
            pass "TEME->J2000 vs JPL Horizons (mean ${mean} m, max ${mx} m)" ||
            bad "TEME->J2000 disagrees with Horizons by ${mx} m (expect < 20)"
    else
        bad "TEME check run failed"
    fi
fi

# --- the utilities that need data we do not ship ----------------------------
# These must report a missing input rather than crash.
./SP3_to_eci >/dev/null 2>&1
[ $? -lt 128 ] && pass "SP3_to_eci exits cleanly without SP3 data" \
               || bad "SP3_to_eci crashed"
./fit_orbit_to_sp3_v3 >/dev/null 2>&1
[ $? -lt 128 ] && pass "fit_orbit_to_sp3_v3 exits cleanly without a config" \
               || bad "fit_orbit_to_sp3_v3 crashed"

# --- TIE-GCM drag out-of-range path -----------------------------------------
# analyses/qbfanxin uses drag = 2 with an orbit the bundled TIE-GCM tables do
# not cover; it must halt with a diagnostic instead of reading past the grid.
./sgnlOPS ../analyses/qbfanxin/ops_config_template.txt "$OUT/tiegcm.out" \
    >"$OUT/tiegcm.log" 2>&1
rc=$?
if [ $rc -lt 128 ] && grep -q "outside the TIE-GCM data set" "$OUT/tiegcm.log"; then
    pass "TIE-GCM out-of-range halts with a diagnostic"
else
    bad "TIE-GCM out-of-range (rc=$rc)"
fi

echo
if [ "$fail" -eq 0 ]; then
    echo "All checks passed."
else
    echo "$fail check(s) failed. Logs in $OUT"
fi
rm -rf "$OUT" 2>/dev/null
exit "$fail"
