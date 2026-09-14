#!/bin/sh

# David Harrison
# 5 December 2015
# memtest_OPS.sh

./make_debug.sh

echo ""
echo "Starting memory leak test:"
echo ""

valgrind --leak-check=full --show-leak-kinds=all -v ../bin/sgnlOPS ../analyses/gpsIIR/configOPS_JPL_gpsIIR.txt ../analyses/gpsIIR/orbit_compare/jpl/ucl_svn46_01mar04.eci
