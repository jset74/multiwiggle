multiwiggle
===========

Tool to create and query files containing multiple wiggle plots.

It accepts WIG and BEDGRAPH file formats.

Example:

mwiggle create mytest.mw *.bedgraph -t 9060 -a NCBI37 -d "My test tracks"

run mwiggle in the command line for help


to install
==========
make

make test

add to the path

export PATH=`pwd`/bin:$PATH


add libs to the path

export LD_LIBRARY_PATH=`pwd`/lib:$LD_LIBRARY_PATH

