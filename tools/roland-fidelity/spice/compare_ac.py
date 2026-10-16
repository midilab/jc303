"""Small-signal comparison of the DSP filter (build/probe) against the SPICE model.

The cutoff convention is the stage pole (Iabc from juno_vcf.iabc_for, the same in both). Levels are relative to the
passband (fc / 20) at resonance 0; the output is Out4.

  python3 compare_ac.py      (run from this directory after ../build.sh)"""
import subprocess, numpy as np
import juno_vcf
from spice_util import run

PROBE = "../build/probe"
POTS = (0.0, 0.3, 0.5, 0.7, 0.8, 0.85, 0.87)
REL = (0.05, 0.25, 0.5, 0.7, 0.85, 1.0, 1.2, 1.5, 2.0, 3.0)

def spice_curve(fc, pot):
    d = run(juno_vcf.netlist(ires=juno_vcf.juno_ires(pot), fc=fc, ppd=200, fstart=2, fstop=80e3), ["v(o4)"])
    return d[:, 0], 20 * np.log10(np.hypot(d[:, 1], d[:, 2]))

def probe(fc, pot, fs):
    out = subprocess.run([PROBE, f"{fc}", f"pot:{pot}", "0"] + [f"{x}" for x in fs], capture_output=True, text=True).stdout
    return np.array([float(l.split()[1]) for l in out.strip().split("\n")])

def main():
    worst = 0.0
    for fc in (200.0, 1000.0, 4000.0, 10000.0):
        fs = [fc * r for r in REL]
        f0, d0 = spice_curve(fc, 0.0)
        ref_s = np.interp(fs[0], f0, d0); ref_d = probe(fc, 0.0, fs[:1])[0]
        print(f"\nfc {fc:.0f} Hz.  rows: pot; cols: f / fc = {REL}; SPICE / DSP / diff dB")
        for pot in POTS:
            f, d = spice_curve(fc, pot)
            s = np.interp(fs, f, d) - ref_s
            p = probe(fc, pot, fs) - ref_d
            worst = max(worst, float(np.max(np.abs(s - p))))
            print(f"  pot {pot:.2f} S " + " ".join(f"{x:6.1f}" for x in s))
            print(f"           D " + " ".join(f"{x:6.1f}" for x in p))
            print(f"           d " + " ".join(f"{x:+6.2f}" for x in p - s))
    print(f"\nworst |DSP - SPICE| = {worst:.2f} dB")

if __name__ == "__main__":
    main()
