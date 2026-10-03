#!/bin/bash
# Builds the Diode Octave fidelity harnesses (JUCE-free) into tools/diode-fidelity/build/.
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
$CXX $FLAGS "$HERE/parity_cells.cpp" $FILTER_SRC -o "$OUT/parity_cells"
$CXX $FLAGS "$HERE/plain_modes.cpp" $FILTER_SRC -o "$OUT/plain_modes"
$CXX $FLAGS "$HERE/render_note.cpp" $ALL_SRC -o "$OUT/render_note"
$CXX $FLAGS "$HERE/drive_level.cpp" $ALL_SRC -o "$OUT/drive_level"
$CXX $FLAGS "$HERE/k_target_solver.cpp" $FILTER_SRC -o "$OUT/k_target_solver"
echo "built into $OUT"
