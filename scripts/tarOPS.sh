#!/bin/sh

# @file tarOPS.sh
# @author Santosh Bhattarai
# @brief A script for packaging SGNL OPS into a single tar file.
# @date 3 Feb 2015

echo "Packaging up SGNL OPS into sgnlOPS.tgz..."

cd ..
tar -zcvf sgnlOPS.tgz Doxyfile Makefile readme.md include bin res src output \
            scripts doc lib
