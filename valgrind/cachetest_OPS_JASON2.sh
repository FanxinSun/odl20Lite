#!/bin/sh

# David Harrison
# 13 June 2016
# cachetest_OPS_JASON2.sh

./make_debug.sh

echo ""
echo "Starting cache test:"
echo ""

valgrind --tool=cachegrind ../bin/sgnlOPS ../analyses/jason2/configOPS_jason2.txt ../analyses/jason2/ucl_cnes_comparison/ucl_jason2.eci
