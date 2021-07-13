#!/bin/bash
#
# Use this script to automatically format the c++ files in
# this repository.

FIX=0
while test $# -gt 0; do
  case "$1" in
    --fix)
      shift
      FIX=1
      ;;
  esac
done

FAILED=0
FORMAT_MESSAGE="is formatted incorrectly. Use ./check-format.sh --fix to format all source files."

# C++ Formatting
for f in $(find ./ -name "*.cc") $(find ./ -name "*.h"); do
  clang-format --style=Google --output-replacements-xml $f | grep -q "<replacement "
  if [ $? -eq 0 ]; then
    if [ $FIX -eq 1 ]; then
      clang-format --style=Google -i $f
    else
      echo $f $FORMAT_MESSAGE
      FAILED=1
    fi
  fi
done

if [ $FAILED -eq 1 ]; then
  exit -1
else
  echo "Formatting is correct :)"
fi
