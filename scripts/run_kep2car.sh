#!/bin/sh

# David Harrison
# 19 November 2015
# run_kep2car.sh

./build_all.sh

# Order of elements:
#       semi-major axis
#       eccentricity
#       inclination
#       arg or pri
#       arg of ascending node
#       true anomaly
#
# Units of semi-major axis are km
# Output units are in km and km/s

cd ../bin
./kep2car 6971 0.05738057667479558169559604074021 0 0 0 0


./kep2car 6871 0 0 0 0 0


./kep2car 7371 0 0 0 0 0
