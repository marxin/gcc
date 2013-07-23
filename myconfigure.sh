#!/bin/bash
PREFIX=/home/marxin/gcc-marxin

# CFLAGS="-ggdb3 -O0"
CFLAGS=""

CXXFLAGS=$CFLAGS CFLAGS=$CFLAGS ../configure --enable-languages=c,c++,java,fortran --enable-bootstrap --prefix=$PREFIX
