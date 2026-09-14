#!/bin/sh

# David Harrison
# 5 December 2015
# profile_OPS.sh

./make_debug.sh

echo ""
echo "Starting profiler:"
echo ""

valgrind --tool=callgrind ../bin/sgnlOPS ../analyses/gpsIIR/configOPS_JPL_gpsIIR.txt ../analyses/gpsIIR/orbit_compare/jpl/ucl_svn46_01mar04.eci

echo ""
echo "Now run callgrind_annotate [options] callgrind.out.<pid>"
echo ""
echo "Some good options:"
echo "--inclusive=yes: Instead of using exclusive cost of functions as sorting order, use and show inclusive cost."
echo "--tree=both: Interleave into the top level list of functions, information on the callers and the callees of each function."
echo "--auto=yes: Get annotated source code for all relevant functions for which the source can be found."
echo ""
echo "More options can be found here:"
echo "http://valgrind.org/docs/manual/cl-manual.html"
