#!/bin/bash
# Builds the ARP 4075 filter fidelity harnesses (JUCE-free) into tools/wasp-fidelity/build/.
#   CXX=<compiler>  default clang++
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
CXX="${CXX:-clang++}"
DSP="$REPO/src/dsp/open303"
OUT="$HERE/build"; mkdir -p "$OUT"
FLAGS="-std=c++17 -O2 -I$DSP"
FILTER_SRC="$DSP/dfl_ARP4075Filter.cpp $DSP/rosic_RealFunctions.cpp"
for src in "$HERE"/*.cpp; do
  name="$(basename "$src" .cpp)"
  $CXX $FLAGS "$src" $FILTER_SRC -o "$OUT/$name"
done
echo "built into $OUT"
