"""SPICE targets for the Diode Native feedback law (dfl_DiodeLadderFilter.h, KNT / KNC).

For each cutoff parameter and resonance-pot position, the real VCF's response ratio |H_pot| / |H_0| over
0.4..2.4 x cutoff, plus its resonant boost and peak frequency. native_solver fits the DSP's loop gain K to them.
Cutoff convention: cutoff parameter = 2^(1/4) x Stinchcombe's corner wc (the ladder core's 180-degree point).
WC_PER_UA is the netlist's wc per uA of ladder current, fitted to its resonance-0 response (Stinchcombe's polynomial,
0.25..4 x wc, shape within 0.14 dB rms); it falls from 61 Hz/uA at 4 uA to 54 at 400 uA, so If_for interpolates it.

  python3 native_targets.py [out=native_curves.txt]      (run from this directory; ~1 minute)"""
import sys, subprocess, uuid, numpy as np
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, '.'); from tb303_vcf import netlist
WC_UA     = [4, 8, 15, 21, 30, 60, 120, 240, 400]
WC_PER_UA = [61.36, 58.39, 56.47, 55.60, 55.08, 54.45, 54.12, 53.85, 53.64]
CUTS = [300, 500, 800, 1000, 2000, 3000, 5000, 8000, 12000, 16000, 20000]
POTS = [0.2, 0.4, 0.6, 0.8, 0.9, 1.0]
NJ = 25

def If_for(cut):
    """Ladder current (A) whose netlist corner wc is cut / 2^(1/4)."""
    wc = cut / 2 ** 0.25; lu = np.log(WC_UA)
    lwc = lu + np.log(WC_PER_UA)   # log wc at each table current, increasing
    return float(np.exp(np.interp(np.log(wc), lwc, lu, left=None, right=None))) * 1e-6

def ac(pot, cut):
    base = f"/tmp/_nat_{uuid.uuid4().hex[:10]}"
    n = netlist(pot, If_for(cut), mode="ac").replace(".ac dec 60 1 20000", f".ac dec 600 {max(1.0, 0.2 * cut)} {min(60e3, 3 * cut)}")
    n = n.replace(".end\n", f".control\nrun\nwrdata {base}.dat vdb(vout)\nquit\n.endc\n.end\n")
    open(base + ".cir", "w").write(n)
    subprocess.run(["ngspice", "-b", base + ".cir"], capture_output=True)
    d = np.loadtxt(base + ".dat"); return d[:, 0], d[:, 1]

def boost(pot, cut):
    """Resonant boost (dB, peak over 0.4..2.4 x cutoff relative to pot 0 at that frequency) and peak / cutoff."""
    f0, d0 = ac(0.0, cut); f, d = ac(pot, cut)
    sel = (f > 0.4 * cut) & (f < 2.4 * cut); i = np.argmax(np.where(sel, d, -1e9))
    return d[i] - np.interp(f[i], f0, d0), f[i] / cut

def curves(cut):
    f0, d0 = ac(0.0, cut); rows = []
    fj = 0.4 * cut * 6.0 ** (np.arange(NJ) / (NJ - 1))
    for pot in POTS:
        f, d = ac(pot, cut); sel = (f > 0.4 * cut) & (f < 2.4 * cut); i = np.argmax(np.where(sel, d, -1e9))
        r = np.interp(fj, f, d) - np.interp(fj, f0, d0)
        rows.append(f"{cut} {pot} {d[i] - np.interp(f[i], f0, d0):.3f} {f[i]:.2f} " + " ".join(f"{x:.3f}" for x in r))
    return rows

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "native_curves.txt"
    with ThreadPoolExecutor(8) as ex: rows = sum(ex.map(curves, CUTS), [])
    open(out, "w").write("\n".join(rows) + "\n")
    print("cutoff " + " ".join(f"   pot {p:.1f}" for p in POTS))
    for c in CUTS:
        print(f"{c:6d} " + " ".join(f"{float(r.split()[2]):10.1f}" for r in rows if int(r.split()[0]) == c))
