#!/bin/sh

# David Harrison
# 13 June 2016
# cachetest_OPS.sh

./make_debug.sh

echo ""
echo "Starting cache test:"
echo ""

valgrind --tool=cachegrind ../bin/sgnlOPS ../analyses/gpsIIR/configOPS_JPL_gpsIIR.txt ../analyses/gpsIIR/orbit_compare/jpl/ucl_svn46_01mar04.eci
