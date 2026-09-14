#!/bin/sh

#
# Santosh Bhattarai
# 14 November 2017
# Setting up MacOS High Sierra for running ops17b
#

# Install Homebrew if it doesn't exist
which -s brew
if [[ $? != 0 ]] ; then
	# Install Homebrew
	ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
else
	brew update
fi

# Install dependencies for ops: ccache, boost and libpng
brew install ccache
brew install boost
brew install libpng

