#!/bin/bash
# Builds the diode filter fidelity harnesses (JUCE-free) into tools/diode-fidelity/build/.
#   MATH_APPROX=<dir>  include dir of math_approx (default: <repo>/lib/math_approx-src/include)
#   CXX=<compiler>     default clang++
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
CXX="${CXX:-clang++}"
DSP="$REPO/src/dsp/open303"
MATH_APPROX="${MATH_APPROX:-$REPO/lib/math_approx-src/include}"
if [ ! -f "$MATH_APPROX/math_approx/math_approx.hpp" ]; then
  # fall back to a sibling checkout's lib (git worktrees do not share the ignored lib/ directory)
  for c in "$REPO"/../../db303/lib/math_approx-src/include "$REPO"/../db303/lib/math_approx-src/include; do
    [ -f "$c/math_approx/math_approx.hpp" ] && MATH_APPROX="$c" && break
  done
fi
[ -f "$MATH_APPROX/math_approx/math_approx.hpp" ] || { echo "math_approx not found; set MATH_APPROX=<include dir> (CPM fetches it to lib/math_approx-src during a normal configure)"; exit 1; }
OUT="$HERE/build"; mkdir -p "$OUT"
FLAGS="-std=c++17 -O2 -I$DSP -I$MATH_APPROX"
FILTER_SRC="$DSP/dfl_DiodeLadderFilter.cpp $DSP/rosic_TeeBeeFilter.cpp $DSP/rosic_OnePoleFilter.cpp $DSP/rosic_RealFunctions.cpp"
ALL_SRC="$(ls "$DSP"/*.cpp | tr '\n' ' ')"
echo "using math_approx: $MATH_APPROX"
$CXX $FLAGS "$HERE/stability_sweep.cpp" $FILTER_SRC -o "$OUT/stability_sweep"
$CXX $FLAGS "$HERE/stress_test.cpp" $ALL_SRC -o "$OUT/stress_test"
$CXX $FLAGS "$HERE/render_note.cpp" $ALL_SRC -o "$OUT/render_note"
$CXX $FLAGS "$HERE/drive_level.cpp" $ALL_SRC -o "$OUT/drive_level"
$CXX $FLAGS "$HERE/mode_level.cpp" $ALL_SRC -o "$OUT/mode_level"
$CXX $FLAGS "$HERE/model_level.cpp" $ALL_SRC -o "$OUT/model_level"
$CXX $FLAGS "$HERE/large_signal.cpp" $FILTER_SRC -o "$OUT/large_signal"
$CXX $FLAGS "$HERE/native_solver.cpp" $FILTER_SRC -o "$OUT/native_solver"
$CXX $FLAGS "$HERE/native_output.cpp" $ALL_SRC -o "$OUT/native_output"
$CXX $FLAGS "$HERE/switch_click.cpp" $ALL_SRC -o "$OUT/switch_click"
echo "built into $OUT"
