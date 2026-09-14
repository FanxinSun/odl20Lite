#! /usr/bin/env python

# @file plot_ecef_eci_test.py
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

hello = "Printing orbit prediction results...................................."
print hello

data = np.genfromtxt( \
	"/Users/ucessbh/Dropbox/dev/sgnlOPS_2014/tests/sgnlOPS_integrator_test.txt", \
	delimiter=',', names=['epoch', 'ak', 'ek', 'ik', 'wk', 'Wk', 'vk', \
	'af', 'ef', 'if', 'wf', 'Wf', 'vf', \
	'aDiff', 'eDiff', 'iDiff', 'wDiff', 'WDiff', 'vDiff'])

#read_datafile\
#("/Users/ucessbh/Dropbox/dev/sgnlOPS_2014/tests/sgnlOPS_integrator_test.txt")

plt.plot(data['epoch'], data['vDiff']);

plt.xlabel('time (s)')
plt.ylabel('difference (radians)')
plt.title('comparison keplerian orbit vs integrated orbit')
plt.ylim(min(data['vDiff']), max(data['vDiff']))
plt.savefig("plots/comparison_0_025s_delta.jpg")
#plt.savefig("./plots/comparison_1s_delta.jpg")
#plt.show()


#function for reading data files output form orbit_propagate
# def read_datafile(filename)

# 	data = np.genfromtxt(filename, delimiter=',', \ 
# 		names=['epoch', 'ak', 'ek', 'ik', 'wk', 'Wk', 'vk',\
# 		 'af', 'ef', 'if', 'wf', 'Wf', 'vf', \
# 		 'aDiff', 'eDiff', 'iDiff', 'wDiff', 'WDiff', 'vDiff'])

# 	return data