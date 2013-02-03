#!/bin/bash

BASEDIR=$(dirname $0)
COMPILER="$BASEDIR/../../../objdir/gcc/xgcc -B $BASEDIR/../../../objdir/gcc/ -fipa-sem-equality"

echo "IPA semantic equality tests"

echo "a) positive tests"

for i in $BASEDIR/*_eq.c
do
  echo -n "   `basename $i`"
  if $COMPILER $i 2>&1 | grep EQUAL > /dev/null
  then
    echo -e " \e[1;32m[OK]\e[00m"
  else
    echo -e " \e[00;31m[FAILED]\e[00m"
  fi
done

echo "b) negative tests"

for i in $BASEDIR/*_diff.c
do
  echo -n "   `basename $i`"
  if $COMPILER $i 2>&1 | grep different > /dev/null
  then
    echo -e " \e[1;32m[OK]\e[00m"
  else
    echo -e " \e[00;31m[FAILED]\e[00m"
  fi
done
