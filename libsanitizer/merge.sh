#!/bin/bash

# FIXME: do we need a license (or whatever else) header here?

# This script merges libsanitizer sources from upstream.

if [ "$#" -lt 1 ]; then
  echo "Usage: merge.sh llvm_project_repository [vcs]"
  exit 1
fi

UPSTREAM=$1
VCS=${2:-svn}

get_current_rev() {
  cd $UPSTREAM
  git rev-parse HEAD
}

list_files() {
  (cd $1; ls *.{cc,cpp,h,inc,S} 2> /dev/null)

}

change_comment_headers() {
  for f in $(list_files $1); do
    sed -n 3p $1/$f | grep -q 'The LLVM Compiler Infrastructure' || continue
    changed=$(awk 'NR != 2 && NR != 3' < $1/$f)
    echo "$changed" > $1/$f
  done
}

# ARGUMENTS: upstream_path local_path
# This function merges changes from the directory upstream_path to
# the directory  local_path.
merge() {
  upstream_path=$UPSTREAM/compiler-rt/$1
  local_path=$2
  change_comment_headers $upstream_path
  echo MERGE: $upstream_path
  all=$( (list_files $upstream_path; list_files $local_path) | sort | uniq)
  #echo $all
  for f in $all; do
    if  [ -f $upstream_path/$f -a -f $local_path/$f ]; then
      echo "FOUND IN BOTH     :" $f
      # diff -u $local_path/$f $upstream_path/$f
      cp -v $upstream_path/$f $local_path
    elif [ -f $upstream_path/$f ]; then
      echo "FOUND IN UPSTREAM :" $f
      cp -v $upstream_path/$f $local_path
      $VCS add $local_path/$f
    elif [ -f $local_path/$f ]; then
      echo "FOUND IN LOCAL    :" $f
      $VCS rm $local_path/$f
    fi
  done

}

fatal() {
  echo "$1"
  exit 1;
}

pwd | grep 'libsanitizer$' || \
  fatal "Run this script from libsanitizer dir"
CUR_REV=$(get_current_rev)
echo Current upstream revision: $CUR_REV
merge include/sanitizer include/sanitizer
merge lib/asan asan
merge lib/lsan lsan
merge lib/tsan/rtl tsan
merge lib/sanitizer_common sanitizer_common
merge lib/interception interception
merge lib/ubsan ubsan

# Need to merge lib/builtins/assembly.h file:
mkdir -p builtins
cp -v $UPSTREAM/compiler-rt/lib/builtins/assembly.h builtins/assembly.h

# Update the MERGE file.
cat << EOF > MERGE
$CUR_REV

The first line of this file holds the git revision number of the
last merge done from the master library sources.
EOF
