#!/bin/sh

# @file run_trajANA1.sh
# @author Santosh Bhattarai
# @brief Script for building, running and testing the SGNL OPS trajectory
#        analysis tool - trajANA1.sh
#
# @date 6 February 2015
#
./build_all.sh

cd bin
./trajANA1 ../output/predicted_orbit.txt

