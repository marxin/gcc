#!/bin/bash
PREFIX=/home/marxin/gcc-marxin

#CFLAGS="-ggdb3 -O0"
CFLAGS=""

CXXFLAGS=$CFLAGS CFLAGS=$CFLAGS ../configure --enable-languages=c,c++ --disable-bootstrap --prefix=$PREFIX --enable-checking=release 
