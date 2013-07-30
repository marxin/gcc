#!/bin/bash
PREFIX=/home/marxin/gcc-marxin

# CFLAGS="-ggdb3 -O0"
# CFLAGS="-O2"

CXXFLAGS=$CFLAGS CFLAGS=$CFLAGS ../configure --enable-languages=c,c++ --disable-bootstrap --prefix=$PREFIX --enable-checking=release
