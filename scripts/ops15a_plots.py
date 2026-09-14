#! /usr/bin/env python

# @file ops15a_plots.py
# @author Santosh Bhattarai.
# @brief A python script for plotting data from csv files.
# @date 28 March 2014.
#
# This is a simple application that generates plots of csv formatted
# data for the SGNL Orbit Prediction Software Version 2014a.
#
import numpy as np
import matplotlib as mpl
import matplotlib.pylab as plt
import sys #For command line arguments - there is a full library for this but it seems excessive

output_location = "../output/"

if len(sys.argv) == 2:
    output_location = str(sys.argv[1])

print "Output directory is " + output_location


hello = "Plotting orbit prediction results...................................."
print hello

data = np.genfromtxt( \
	output_location + "predicted_orbits.txt", \
	delimiter=',', names=['totalTime','epoch', 'X', 'Y', 'Z', 'U', 'V', 'W', \
	'a', 'e', 'i', 'w1', 'W2', 'v'])

#read_datafile\
#("/Users/ucessbh/Dropbox/dev/sgnlOPS_2014/tests/sgnlOPS_integrator_test.txt")

elements = ['a', 'e', 'i', 'w1', 'W2', 'v'];

for el in elements:

    plt.figure(el)

    time_since = data['epoch']-data['epoch'][0]
    data2 = data[el]-data[el][0]

    #plt.plot(data['epoch'], data[el]);
    plt.plot(time_since, data[el]);
    #plt.plot(time_since, data2)

    ymin = min(data[el])
    ymax = max(data[el])
    
    #ymin = min(data2)
    #ymax = max(data2)
    #ymax = ymin + (ymax - ymin)*1.1

    plt.xlabel('time (days)')
    plt.ylabel(el)
    plt.title('evolution of orbital element '+ el + '\n' + str(data[el][0]) + '\n' + str(data['epoch'][0]))
    plt.ylim(ymin, ymax)
    plt.savefig(output_location + "element-"+el+".jpg")
#   plt.show()
    plt.close()

## Semi-major axis plot
# plt.figure()
# plt.plot(data['epoch'], data['a']);
# plt.xlabel('time (s)')
# plt.ylabel('a, semi-major axis (km)')
# plt.title('evolution of the semi-major axis')
# plt.ylim(min(data['a']), max(data['a']))
# plt.savefig("ops15a_tests/sma1.jpg")
# #plt.savefig("./plots/comparison_1s_delta.jpg")
# #plt.show()

# ## Eccentricity plot
# plt.figure()
# plt.plot(data['epoch'], data['e']);
# plt.xlabel('time (s)')
# plt.ylabel('e, eccentricity')
# plt.title('evolution of the eccentricity')
# plt.ylim(min(data['e']), max(data['e']))
# plt.savefig("ops15a_tests/ecc1.jpg")
# #plt.savefig("./plots/comparison_1s_delta.jpg")
# #plt.show()

# ## Inclination plot
# plt.figure()
# plt.plot(data['epoch'], data['i']);
# plt.xlabel('time (s)')
# plt.ylabel('i, inclination')
# plt.title('evolution of the inclination')
# plt.ylim(min(data['i']), max(data['i']))
# plt.savefig("ops15a_tests/inc1.jpg")
# #plt.savefig("./plots/comparison_1s_delta.jpg")
# #plt.show()

# ## Inclination plot
# plt.figure()
# plt.plot(data['epoch'], data['v']);
# plt.xlabel('time (s)')
# plt.ylabel('v, true anomaly')
# plt.title('evolution of the true anomaly')
# plt.ylim(min(data['v']), max(data['v']))
# plt.savefig("ops15a_tests/nu1.jpg")
#plt.savefig("./plots/comparison_1s_delta.jpg")
#plt.show()

#function for reading data files output form orbit_propagate
# def read_datafile(filename)

# 	data = np.genfromtxt(filename, delimiter=',', \
# 		names=['epoch', 'ak', 'ek', 'ik', 'wk', 'Wk', 'vk',\
# 		 'af', 'ef', 'if', 'wf', 'Wf', 'vf', \
# 		 'aDiff', 'eDiff', 'iDiff', 'wDiff', 'WDiff', 'vDiff'])

# 	return data
