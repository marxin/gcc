#!/bin/bash
# CXXFLAGS="-ggdb3 -O0" CFLAGS="-ggdb3 -O0" ../configure --enable-languages=c --disable-bootstrap --disable-libsanitizer --prefix=/home/marxin/Programming/gcc-mainline
../configure --enable-languages=c,c++,fortran,java --enable-bootstrap --disable-libsanitizer --prefix=/home/marxin/Programming/gcc-mainline
