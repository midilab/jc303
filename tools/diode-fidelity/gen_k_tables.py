#!/usr/bin/env python3
"""Regenerate the Diode Octave feedback-law tables (K100T, KCT, RHOT) with k_target_solver.

Run after changing CUTOFF_TUNING_OCTAVE (or the low-cutoff boost), the loop highpass, or the ladder:
the tables are a fit to the TeeBee at the current tuning. Prints C++ arrays to paste over the ones in
src/dsp/open303/dfl_DiodeLadderFilter.h (calculateCoefficients).  Takes ~3-4 minutes.

  python3 gen_k_tables.py [--jobs N]
Cutoffs are the table nodes; the three feedback-HP nodes are (TeeBee knob, diode corner):
B = knob 1.0 (100 / 76.7 Hz), A = default (159 / 122 Hz), C = knob 0 (350 / 268 Hz)."""
import re, subprocess, sys, os, collections
from concurrent.futures import ThreadPoolExecutor
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, "build", "k_target_solver")
CUTS = [300, 500, 800, 1000, 2000, 3000, 5000, 8000, 12000, 16000, 20000]
NODES = {"B": (100, 76.7), "A": (159, 122.0), "C": (350, 268.0)}

def run(job):
    c, name = job; t, d = NODES[name]
    return job, subprocess.run([BIN, str(c), str(t), str(d)], capture_output=True, text=True).stdout

def main():
    jobs = int(sys.argv[sys.argv.index("--jobs") + 1]) if "--jobs" in sys.argv else os.cpu_count() or 4
    if not os.path.exists(BIN): sys.exit("build first: tools/diode-fidelity/build.sh")
    res = {}
    with ThreadPoolExecutor(jobs) as ex:
        for (c, name), out in ex.map(run, [(c, n) for c in CUTS for n in NODES]):
            m = re.match(r"cut\s+(\d+) fhT (\d+) fhD ([\d.]+) \| K_crit ([\d.]+) \|(.*)", out)
            ks = {int(a): float(b) for a, b in re.findall(r"r(\d+): TB [\d.]+ dB -> K ([\d.]+)", m[5])}
            res[(c, name)] = (float(m[4]), ks, "*" in m[5])
    K100 = {n: [res[(c, n)][1][100] for c in CUTS] for n in NODES}
    KC = {n: [res[(c, n)][0] for c in CUTS] for n in NODES}
    for n in NODES:   # glitch guard: with the highest HP corner resonance barely exists at the lowest cutoff
        for i in range(len(CUTS)):
            if K100[n][i] < 10:
                print(f"// warning: K100 {n}@{CUTS[i]} = {K100[n][i]:.2f} looks like a solver glitch; extrapolating", file=sys.stderr)
                K100[n][i] = K100[n][i + 1] - (K100[n][i + 2] - K100[n][i + 1])
    flagged = [(c, n) for (c, n), v in res.items() if v[2]]
    if flagged: print("// note: boost could not be matched (TeeBee nearer oscillation than the diode threshold) at", flagged, file=sys.stderr)
    rho = [[res[(c, "A")][1][r] / res[(c, "A")][1][100] for r in (20, 40, 60, 80)] + [1.0] for c in CUTS]
    f = lambda a: ", ".join(f"{x:.3f}" for x in a)
    print("      static const double K100T[3][11] = {")
    print("        { " + f(K100["B"]) + " },\n        { " + f(K100["A"]) + " },\n        { " + f(K100["C"]) + " } };")
    print("      static const double KCT[3][11] = {")
    print("        { " + f(KC["B"]) + " },\n        { " + f(KC["A"]) + " },\n        { " + f(KC["C"]) + " } };")
    print("      static const double RHOT[11][5] = {")
    print(",\n".join("        { " + f(r) + " }" for r in rho))
    print("      };")

if __name__ == "__main__":
    main()
