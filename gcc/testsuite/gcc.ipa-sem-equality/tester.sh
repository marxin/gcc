#!/bin/bash

BASEDIR=$(dirname $0)
COMPILER="$BASEDIR/../../../objdir/gcc/xg++ -c -B $BASEDIR/../../../objdir/gcc/ -fipa-sem-equality -fdump-ipa-sem-equality"

echo "IPA semantic equality tests"

echo "a) POSITIVE tests"

for i in $BASEDIR/*_eq.c
do
  echo -n "   `basename $i`"
  bname="`basename $i`.0*i.sem-equality"
  if $COMPILER $i 2>&1 && grep hit $bname > /dev/null
  then
    echo -e " \e[1;32m[OK]\e[00m"
  else
    echo -e " \e[00;31m[FAILED]\e[00m"
  fi
  rm $bname
done

echo "b) NEGATIVE tests"

for i in $BASEDIR/*_diff.c
do
  echo -n "   `basename $i`"
  bname="`basename $i`.0*i.sem-equality"
  if $COMPILER $i 2>&1 | grep -v hit $bname > /dev/null
  then
    echo -e " \e[1;32m[OK]\e[00m"
  else
    echo -e " \e[00;31m[FAILED]\e[00m"
  fi
  rm $bname
done
