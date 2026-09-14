#! /usr/bin/env python

# @file spiral_no_earth_or_atm.py
# @author David Harrison.
# @brief A python script for plotting data from csv files.
# @date 2 November 2014.
#
# This is a simple application that generates plots of csv formatted
# data for the SGNL Orbit Prediction Software Version 2014a.
#
import math as math
import numpy as np
import matplotlib as mpl
import matplotlib.pyplot as plt
import sys #For command line arguments - there is a full library for this but it seems excessive

earth_radius = 6371 # Radius in km

output_location = "../output/"

if len(sys.argv) == 2:
    output_location = str(sys.argv[1])

print "Output directory is " + output_location
print "Plotting death spiral...................................."

data = np.genfromtxt( \
	output_location + "predicted_orbits.txt", \
	delimiter=',', names=['totalTime','epoch', 'X', 'Y', 'Z', 'U', 'V', 'W', \
	'a', 'e', 'i', 'w1', 'W2', 'v'])


radial = []
theta = []

radial.append( math.sqrt(data['X'][0]*data['X'][0] + data['Y'][0]*data['Y'][0] + data['Z'][0]*data['Z'][0]) )
theta.append( 0 )

for i in range(1, len(data['X'])):

    radial.append( math.sqrt(data['X'][i]*data['X'][i] + data['Y'][i]*data['Y'][i] + data['Z'][i]*data['Z'][i]) )
    costheta = (data['X'][i-1]*data['X'][i] + data['Y'][i-1]*data['Y'][i] + data['Z'][i-1]*data['Z'][i])/(radial[i-1]*radial[i])

    if costheta>1.0:
        theta.append( theta[i-1] )
    else:
        theta.append( theta[i-1] + math.acos( costheta ) )

    if radial[i] < earth_radius+10:
        break


for i in range(0, len(radial)):
    radial[i] -= earth_radius

radmin = min(radial)
radmax = max(radial)

print "Number of orbits =",(max(theta)/(2*math.pi))

plt.figure(figsize=(6,6))

spiral = plt.subplot(111, projection='polar')
spiral.set_title("Orbit Plot", y=1.08)
spiral.plot(theta, radial, color='red', linewidth=0.5);
spiral.set_rmax(math.ceil(radmax/91)*100)
spiral.set_rmin(0)

#atmo = plt.Circle((0, 0), 100, transform=spiral.transData._b, color="blue", alpha=0.2)
#spiral.add_artist(atmo)

#earth = plt.Circle((0, 0), 400, transform=spiral.transData._b, color="green")
#spiral.add_artist(earth)

plt.savefig(output_location + "spiral_no_earth_or_atm.png", dpi=200)

#plt.show()
