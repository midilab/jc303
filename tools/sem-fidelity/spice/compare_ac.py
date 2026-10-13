"""Small-signal comparison of the SEM DSP filter (build/probe) against the SPICE model.

Same Iabc in both (sem_vcf.iabc_for; the DSP prewarps it for its trapezoidal integrators), no fitting. Levels are
relative to the LP passband (fc / 20) at RES 0; LP (the output at MODE 0), BP / HP at their circuit nodes (probe bp / hp).

  python3 compare_ac.py      (run from this directory after ../build.sh)"""
import sys, subprocess, numpy as np
import sem_vcf
from spice_util import run

PROBE = "../build/probe"
POTS = (0.0, 0.5, 0.8, 0.9, 0.95, 1.0)
REL = (0.05, 0.25, 0.5, 0.7, 0.8, 0.9, 1.0, 1.2, 1.5, 2.0, 3.0)

def spice_curve(fc, pot, node="lp"):
    d = run(sem_vcf.netlist(pot=pot, fc=fc, ppd=400, fstart=2, fstop=80e3), [f"v({node})"])
    return d[:, 0], 20 * np.log10(np.hypot(d[:, 1], d[:, 2]))

def probe(fc, pot, fs, mode=0.0):
    out = subprocess.run([PROBE, f"{fc}", f"pot:{pot}", f"{mode}"] + [f"{x}" for x in fs], capture_output=True, text=True).stdout
    return np.array([float(l.split()[1]) for l in out.strip().split("\n")])

def main():
    worst = worstPeak = 0.0
    for fc in (100.0, 400.0, 1000.0, 4000.0, 10000.0):
        fs = [fc * r for r in REL]
        f0, d0 = spice_curve(fc, 0.0)
        ref_s = np.interp(fs[0], f0, d0); ref_d = probe(fc, 0.0, fs[:1])[0]
        print(f"\nfc {fc:.0f} Hz.  rows: pot (LP); cols: f / fc = {REL}; SPICE / DSP / diff dB; peak f / dB SPICE | DSP")
        for pot in POTS:
            f, d = spice_curve(fc, pot)
            s = np.interp(fs, f, d) - ref_s
            p = probe(fc, pot, fs) - ref_d
            worst = max(worst, float(np.max(np.abs(s - p))))
            # peak: SPICE's, then the DSP probed on a fine grid around it
            sel = (f > fc * 0.3) & (f < fc * 2); i = np.argmax(np.where(sel, d, -1e9)); fp, dp = f[i], d[i] - ref_s
            pk = ""
            if dp > 3.0:
                g = np.linspace(fp * 0.97, fp * 1.03, 25); pd = probe(fc, pot, g) - ref_d; j = np.argmax(pd)
                worstPeak = max(worstPeak, abs(pd[j] - dp))
                pk = f"   peak {fp:7.1f} {dp:5.1f} | {g[j]:7.1f} {pd[j]:5.1f}"
            print(f"  pot {pot:.2f} S " + " ".join(f"{x:6.1f}" for x in s) + pk)
            print(f"           D " + " ".join(f"{x:6.1f}" for x in p))
            print(f"           d " + " ".join(f"{x:+6.2f}" for x in p - s))
        for node in ("bp", "hp"):
            for pot in (0.0, 0.8):
                f, d = spice_curve(fc, pot, node)
                s = np.interp(fs, f, d) - ref_s
                p = probe(fc, pot, fs, node) - ref_d
                worst = max(worst, float(np.max(np.abs(s - p))))
                print(f"  {node} pot {pot:.1f} d " + " ".join(f"{x:+6.2f}" for x in p - s))
    print(f"\nworst |DSP - SPICE| = {worst:.2f} dB on the grid; at the peaks {worstPeak:.2f} dB")

if __name__ == "__main__":
    main()
