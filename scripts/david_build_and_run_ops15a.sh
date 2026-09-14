#!/bin/sh

# Santosh Bhattarai
# 23 January 2015
# build_and_run_OPS15a.sh

# Note. Run this script from the root directory of SGNL OPS.
./build_sgnlOPS.sh

cd bin
#./sgnlOPS
./sgnlOPS ../res/configOPS_david.txt ../output/predicted_orbits.txt

cd ../scripts
python ops15a_plots.py
python spiral_no_earth_or_atm.py
python spiral_no_earth.py
python spiral.py
