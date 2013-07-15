#!/bin/bash
CXXFLAGS="-ggdb3 -O0" CFLAGS="-ggdb3 -O0" ../configure --enable-languages=c,c++ --disable-bootstrap --prefix=/home/marxin/gcc-marxin

# CXXFLAGS="-O0 -flto-partition=none -flto -fno-fat-lto-objects -fipa-sem-equality" CFLAGS="-O0 -flto-partition=none -flto -fno-fat-lto-objects -fipa-sem-equality" ../configure --enable-languages=c --disable-bootstrap --prefix=/home/marxin/gcc-test
# ../configure --enable-languages=c --enable-bootstrap --prefix=/home/marxin/gcc-test
