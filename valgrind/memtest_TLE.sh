#!/bin/sh

# David Harrison
# 27 June 2017
# memtest_TLE.sh

./make_debug.sh

echo ""
echo "Starting memory leak test:"
echo ""

cd ../bin

valgrind --leak-check=full --show-leak-kinds=all -v ./TLE_analysis ../analyses/TLE_analysis/output
