 #!/bin/sh
 
 #Santosh Bhattarai, SGNL, UCL
 #15 April 2014
 #bbuild.sh Build purely through bash script i.e. no cmake here.
 
 echo "Testing the geomagnetic field software..."
 
 g++ src/global.cpp src/cartesian.cpp src/timeUCL.cpp mains/geomagnetic_field.cpp -o apps/geomag
 
 #make ./build directory it doesn't exist already
 mkdir -p ./apps
 
#run lor_for_analysis command line application--->period: jan 2014.
./apps/geomag
