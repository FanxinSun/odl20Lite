#!/bin/sh

# Santosh Bhattarai
# 23 January 2015
# build_and_run_OPS15a.sh

# Note. Run this script from the root directory of SGNL OPS.
./build_sgnlOPS.sh

cd bin
#./sgnlOPS
./sgnlOPS ../res/configOPS_meo.txt ../output/predicted_orbits.txt

cd ../scripts
python ops15a_plots.py