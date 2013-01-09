#!/bin/bash

CXXFLAGS="-ggdb3 -O0" CFLAGS="-ggdb3 -O0" ../configure --enable-languages=c,c++ --disable-bootstrap --disable-libsanitizer
