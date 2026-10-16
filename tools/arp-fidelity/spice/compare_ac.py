"""Small-signal comparison of the DSP filter (build/probe) against the SPICE model (real 2N5087s).

The cutoff convention is the stage pole (the tail from arp4075.tail_for, the same in both; the DSP prewarps it).
Levels are relative to the LP passband (fc / 20) at RES 0; the output is pin 10 after C6.

  python3 compare_ac.py [mod]      (run from this directory after ../build.sh; mod: the 2k2 mod in both)"""
import os, sys, subprocess, numpy as np
import arp4075
from spice_util import run

PROBE = "../build/probe"
MOD = len(sys.argv) > 1 and sys.argv[1] == "mod"
ENV = dict(os.environ, ARP_MOD="1" if MOD else "0")
POTS = (0.0, 0.3, 0.5, 0.7, 0.75, 0.8)
REL = (0.05, 0.25, 0.5, 0.7, 0.85, 1.0, 1.2, 1.5, 2.0, 3.0)

def spice_curve(fc, pot):
    d = run(arp4075.netlist(pot=pot, fc=fc, ppd=300, fstart=2, fstop=80e3, mod=MOD), ["v(out)"])
    return d[:, 0], 20 * np.log10(np.hypot(d[:, 1], d[:, 2]))

def probe(fc, pot, fs):
    out = subprocess.run([PROBE, f"{fc}", f"pot:{pot}", "0"] + [f"{x}" for x in fs], capture_output=True, text=True, env=ENV).stdout
    return np.array([float(l.split()[1]) for l in out.strip().split("\n")])

def main():
    worst = worstPeak = 0.0
    for fc in (100.0, 400.0, 1000.0, 4000.0, 10000.0) + ((20000.0,) if MOD else ()):
        fs = [fc * r for r in REL]
        f0, d0 = spice_curve(fc, 0.0)
        ref_s = np.interp(fs[0], f0, d0); ref_d = probe(fc, 0.0, fs[:1])[0]
        print(f"\nfc {fc:.0f} Hz.  rows: pot; cols: f / fc = {REL}; SPICE / DSP / diff dB; peak f / dB SPICE | DSP")
        for pot in POTS:
            f, d = spice_curve(fc, pot)
            s = np.interp(fs, f, d) - ref_s
            p = probe(fc, pot, fs) - ref_d
            worst = max(worst, float(np.max(np.abs(s - p))))
            sel = (f > fc * 0.3) & (f < fc * 2); i = np.argmax(np.where(sel, d, -1e9)); fp, dp = f[i], d[i] - ref_s
            pk = ""
            if dp > 1.0:
                g = np.linspace(fp * 0.95, fp * 1.05, 31); pd = probe(fc, pot, g) - ref_d; j = np.argmax(pd)
                worstPeak = max(worstPeak, abs(pd[j] - dp))
                pk = f"   peak {fp:7.1f} {dp:5.1f} | {g[j]:7.1f} {pd[j]:5.1f}"
            print(f"  pot {pot:.2f} S " + " ".join(f"{x:6.1f}" for x in s) + pk)
            print(f"           D " + " ".join(f"{x:6.1f}" for x in p))
            print(f"           d " + " ".join(f"{x:+6.2f}" for x in p - s))
    print(f"\nworst |DSP - SPICE| = {worst:.2f} dB on the grid; at the peaks {worstPeak:.2f} dB")

if __name__ == "__main__":
    main()
