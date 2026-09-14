#! /Library/Frameworks/Python.framework/Versions/Current/bin/python
# surfer6polReplicate.py
#
# This script is used to determine the north and south pole values of a Surfer6 grid file:
# Longitude = 0, Latitude = 90,-90
# and generate a new Surfer6 grid file where all values at +/- 90 degrees have the north and south pole values, respectively.
# This script performs pole replication after gridding, as opposed to spiralPointPadding1,2, and 3 scripts which augment a spiral points file prior to gridding.
#
# Surfer6 files contain 5 header lines, followed by data lines.
# Each line corresponds to increasing values of the y variable (latitude in our application).
# Each row increases in the x variable from left to right (longitude in our application).
# Thus Surfer6 grid files are "upside Mercator" maps in our application.
#
# This script checks to see if the 0 longitude and +/- 90 degree latitude points exist (a grid node falls on these values).
# Usage: python surfer6poleReplicate.py <input file> <output file>
# The input and output files are checked for existence and the user is prompted to supply alternate filenames if necessary.
#
# The script has been tested with our standard grid resolution [-180,180] longitude x [-90,90] latitude: 361 xnodes x 181 ynodes.
# It should be robust for other grid dimensions and resolutions (provided the poles lie on grid nodes) but this has not been tested yet.
#
# created 7-Aug-2013: Shawn Allgeier
# updated 17-Aug-2013: added south pole selection.
# updated 19-Aug-2013: added north pole selection.
# updated 20-Aug-2013: added output file with pole replication.


# Command line argument provision
# --------------------------------------------------
import math
import os # for directory information.
localFiles = os.listdir(os.getcwd()) # list of files in current working directory.
import sys # for input arguments.
arguments = sys.argv
nargs = len(sys.argv) # number or input arguments.  First argument is always the script name and is included in the count.  
if nargs < 3:
	print
	print "File argumets missing:"
	print "Usage: python surfer6poleReplicate.py <input file> <output file>"
	inputFile = raw_input('Enter input filename: ') # give user option to enter input file.
	while inputFile not in localFiles:
		print "Filename entered was not located in current directory."
		inputFile = raw_input('Enter input filename: ') # repeat until valid filename is entered.
	outputFile= raw_input('Enter output filename: ') # give user option to enter output file.
elif nargs == 3:
	inputFile = sys.argv[1] # spiral points data {latitude, longitude, x, y, z}
	outputFile = sys.argv[2] # text file to be created and padded {longitude, latitude, x, y, z}.
# This block should force the user to supply a valid input filename.

# Check for existing files
# --------------------------------------------------
if inputFile in localFiles:
	pass # source file is there, good on you!
else:
	while inputFile not in localFiles:
		print "Input file %s entered was not located in current directory." % inputFile
		inputFile = raw_input('Enter input filename: ') # repeat until valid filename is entered.
if outputFile in localFiles:
	print 
	outputOverwrite = raw_input('Output file already exists.  Overwrite: [(y)es/(n)o] ? ')
	if outputOverwrite == 'y' or outputOverwrite == 'yes':
		pass
	elif outputOverwrite == 'n' or outputOverwrite == 'no':
		outputFile = raw_input('Enter filename: ') # define new output filename.
		

# Determine existence of pole values
# --------------------------------------------------
fin = open(inputFile, 'r')
print
N = 0
fileStart = fin.tell() # cursor position of beginning of file.
for k in fin:
	N = N + 1 # number of lines in file.
# Parse data records:
fin.seek(fileStart) # return cursor to start of file.
header = fin.readline() # one line of data as a string.
nodes = fin.readline() # one line of data as a string.
xnodes = int(nodes.split(" ")[0]) # numerical version of latitude.
ynodes = int(nodes.split(" ")[1]) # numerical version of longitude.
xrange = fin.readline() # one line of data as a string.
xmin = float(xrange.split(" ")[0])
xmax = float(xrange.split(" ")[1])
yrange = fin.readline() # one line of data as a string.
ymin = float(yrange.split(" ")[0])
ymax = float(yrange.split(" ")[1])
zrange = fin.readline() # one line of data as a string.
zmin = zrange.split(" ")[0] # 
zmax = zrange.split(" ")[1]
dataStart = fin.tell() # cursor position of beginning of data.
print '%s:' % inputFile
print '%s xnodes and %s ynodes' % (xnodes, ynodes)
print 'X values are [%s, %s]' % (xmin, xmax)
print 'Y values are [%s, %s]' % (ymin, ymax)
print 'Z values are [%s, %s]' % (zmin, zmax)
# Check latitude:
if ( ymax == 90.0 and ymin == -90.0 ): # latitude ranges over appropriate values.
	latitudeCheck = 1
# Check longitude:
deltaX = (xmax - xmin) / (xnodes - 1.0) # horizontal spacing.
m = (-1*xmin / deltaX) + 1 # index of 0 degree longitude position, numbering from 1.
if ( m == math.floor(m) and xmin < 0.0 and xmax > 0.0): # 0 degree longitude IS in grid range.
	longitudeCheck = 1
	m = int(m) # is a longitude index on the latitude row

if ( not(latitudeCheck == 1) or not(longitudeCheck == 1) ):
	print "North and South poles are not included in this grid file."
	print "This tool cannot be used - choose your grid file parameters wisely!"
	print "Exiting script."
	exit
	
# Number of values on partial lines, if they exist:
partialLine = xnodes % 10
if ( partialLine == 0 ):
	print "No partial lines in this grid file."
else:
	print "Partial lines have %s value(s) on them." % partialLine
print "Each latitude occupies %s lines in the file." % (m//10 + partialLine)

# Get value at South Pole (occurs first in grid file):
# --------------------------------------------------
if ( m % 10 == 0 ): # number of longitude values divides evenly by 10.
	completeRows = (m / 10)  # last value on row after this one.
	for i in range(completeRows-1): # loop through complete rows, less one. 
		scrap = fin.readline() # read through the complete rows -1 lines.
	line = fin.readline() # this is the line we want. 
	southPole = float(line.split(" ")[9]) # last position on line.
	print "South pole = %s, and occurs in position 10 on row #%s of -90deg latitude, which is line #%s in the file." % (southPole, m % 10, 5 + m % 10)
else: # number of longitude values does not divide evenly by 10.
	dummyRows = m // 10 # floor division
	longIndex = m % 10 # longitude index.
	for i in range(dummyRows): # loop over however many dummy rows there are.
		scrap = fin.readline() # read through dummyRows lines.
	line = fin.readline() # this is the line we want.
	southPole = line.split(" ")[longIndex-1]
	sLine = 5 + dummyRows + 1 # file line # where south pole appears.
	print "South pole = %s, and occurs in position %s on row #%s of -90deg latitude, which is line #%s in the file." % (southPole, longIndex, dummyRows+1,sLine)

fin.seek(dataStart) # return cursor to start of file.
# Get value at North Pole (occurs at end of grid file):
# --------------------------------------------------
if ( m % 10 == 0 ): # number of longitude values divides evenly by 10.
	if ( partialLine == 0 ):
		completeRows = (m/10) + (ynodes - 1)*( (xnodes / 10) + 1)
	else:
		completeRows = (m/10) + (ynodes - 1)*( (xnodes // 10) + 2)
	for i in range(completeRows-1): # loop through complete rows, less one.
		scrap = fin.readline() # read through the complete rows -1 lines.
	line = fin.readline() # this is the line we want.
	northPole = float(line.split(" ")[9]) # last position on line.
	print "North pole = %s, and occurs in position 10 on row #%s of -90deg latitude, which is line #%s in the file." % (northPole, m % 10, 5 + m % 10 + (ynodes-1)*(xnodes // 10) + 1)
else: # number of longitude values does not divide evenly by 10.
	if ( partialLine == 0 ):
		dummyRows = (m // 10) + (ynodes - 1)*( (xnodes / 10) + 1)
	else:
		dummyRows = (m // 10) + (ynodes - 1)*( (xnodes / 10) + 2)
	for i in range(dummyRows): # loop over however many dummy rows there are.
		scrap = fin.readline() # read through dummyRwos lines.
	line = fin.readline() # this is the line we want.
	northPole = line.split(" ")[longIndex-1]
	nLine = 5 + dummyRows + 1 # file line # where north pole appears.
	print "North pole = %s, and occurs in position %s on row #%s of -90deg latitude, which is line #%s in the file." % (northPole, longIndex, m//10 +1,nLine)
	# north pole value = 1.990949449776261e-12 for dummy.grd (from Matlab).

# Write new file with replicated pole values.
# --------------------------------------------------
fin.seek(fileStart) # rewind to beginning of file.
fout = open(outputFile, 'w')
# Write header information
for i in range(5): # write header and grid parameter information.
	line = fin.readline()
	fout.write(line)
# Write south pole line:
spLine = southPole + " " + southPole + " " + southPole + " " + southPole + " " + southPole + " " + southPole + " " + southPole + " " + southPole + " " + southPole + " " + southPole + " \n"
for i in range(xnodes // 10):
	fout.write(spLine)
# Write a partial line if necessary.
if (partialLine != 0): 
	partialSouth = southPole
	for i in range(partialLine - 1):
		partialSouth = partialSouth + " " + southPole
	partialSouth = partialSouth + "\n"
	fout.write(partialSouth)
fout.write("\n")

# Loop and write intermediate lines:
dummyLines = (xnodes // 10) + partialLine + 1 # loop over full lines, partial line (if present), and blank line.
for i in range(dummyLines): # advance input file pointer past south pole values.
	line = fin.readline()
middleLines = ((xnodes // 10) + partialLine + 1)* (ynodes -2)
for i in range(middleLines):
	line = fin.readline()
	fout.write(line)
#fout.write("\n")

# Write north pole line: 
npLine = northPole + " " + northPole + " " + northPole + " " + northPole + " " + northPole + " " + northPole + " " + northPole + " " + northPole + " " + northPole + " " + northPole + " \n"
for i in range(xnodes // 10):
	fout.write(npLine)
# Write a partial line if necessary.
if (partialLine != 0): 
	partialNorth = northPole
	for i in range(partialLine - 1):
		partialNorth = partialNorth + " " + northPole
	partialNorth = partialNorth + "\n"
	fout.write(partialNorth)
fout.write("\n")
fout.close # done with output file.
fin.close # done with input file.
# End of surfer6poleReplicate.
