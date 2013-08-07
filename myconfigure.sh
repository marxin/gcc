#!/bin/bash
../configure --enable-languages=c --disable-bootstrap --disable-libsanitizer --prefix=/home/marxin/gcc-marxin

# CXXFLAGS="-O0 -flto-partition=none -flto -fno-fat-lto-objects -fipa-sem-equality" CFLAGS="-O0 -flto-partition=none -flto -fno-fat-lto-objects -fipa-sem-equality" ../configure --enable-languages=c --disable-bootstrap --disable-libsanitizer --prefix=/home/marxin/gcc-test
# ../configure --enable-languages=c --enable-bootstrap --disable-libsanitizer --prefix=/home/marxin/gcc-test
