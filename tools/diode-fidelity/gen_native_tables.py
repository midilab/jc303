#!/usr/bin/env python3
"""Regenerate the Diode feedback-law tables (KNT, KNC) from the SPICE model of the real VCF.

Runs spice/native_targets.py (ngspice, ~1 minute) and build/native_solver for each cutoff node (~1 minute in
parallel), then prints C++ arrays to paste over the ones in src/dsp/open303/dfl_DiodeLadderFilter.h
(calculateCoefficients). Run after changing the ladder, CUTOFF_TUNING, the feedback network
or the netlist.

  python3 gen_native_tables.py [--jobs N]"""
import os, re, subprocess, sys
from concurrent.futures import ThreadPoolExecutor
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, "build", "native_solver")
CURVES = "/tmp/native_curves.txt"
CUTS = [300, 500, 800, 1000, 2000, 3000, 5000, 8000, 12000, 16000, 20000]

def main():
    jobs = int(sys.argv[sys.argv.index("--jobs") + 1]) if "--jobs" in sys.argv else os.cpu_count() or 4
    if not os.path.exists(BIN): sys.exit("build first: tools/diode-fidelity/build.sh")
    subprocess.run([sys.executable, "native_targets.py", CURVES], cwd=os.path.join(HERE, "spice"), check=True, stdout=sys.stderr)
    curves = open(CURVES).read()
    run = lambda c: subprocess.run([BIN, str(c)], input=curves, capture_output=True, text=True).stdout
    with ThreadPoolExecutor(jobs) as ex: outs = dict(zip(CUTS, ex.map(run, CUTS)))
    for c in CUTS: print("// " + outs[c].strip(), file=sys.stderr)
    kc = [[float(re.search(r"K_crit ([\d.]+) ([\d.]+) ([\d.]+)", outs[c])[h + 1]) for c in CUTS] for h in range(3)]
    k = [[0.0] + [float(x) for x in re.findall(r"a[\d.]+: K ([\d.]+)", outs[c])] for c in CUTS]
    f = lambda row: ", ".join("0" if x == 0 else f"{x:.3f}" for x in row)
    print("      static const double KNT[11][7] = {")
    print(",\n".join("        { " + f(r) + " }" for r in k) + " };")
    print("      static const double KNC[3][11] = {")
    print(",\n".join("        { " + f(r) + " }" for r in kc) + " };")

if __name__ == "__main__":
    main()
