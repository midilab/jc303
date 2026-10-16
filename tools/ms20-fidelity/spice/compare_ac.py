"""Small-signal comparison of the DSP filter (build/probe) against the SPICE model.

The cutoff convention is the core's natural frequency w0 (resonance 0): SPICE's is read off the core's -90 degree point
(Q3's emitter vs Q2's collector), and the DSP is run at that cutoff with no fitting. Levels are relative to the
passband (100 Hz below the cutoff's decade) at resonance 0.

  python3 compare_ac.py      (run from this directory after ../build.sh)"""
import subprocess, numpy as np
from spice_util import run
import ms20_k35

PROBE = "../build/probe"
POTS = (0.0, 0.3, 0.5, 0.6, 0.7, 0.8, 0.9)
REL = (0.25, 0.5, 0.7, 0.85, 1.0, 1.2, 1.5, 2.0, 3.0, 5.0)

def spice_curve(ctl, pot, fs):
    n = ms20_k35.netlist(a=pot, vctl=ctl, ppd=200, fstart=5, fstop=60e3)
    d = run(n, ["v(nE)", "v(n2)", "v(nin)"])
    f = d[:, 0]; e = d[:, 1] + 1j * d[:, 2]; z = d[:, 4] + 1j * d[:, 5]; x = d[:, 7] + 1j * d[:, 8]
    core = np.unwrap(np.angle(z / x))
    return f, 20 * np.log10(np.abs(e)), core

def w0_spice(ctl):
    f, _, ph = spice_curve(ctl, 0.0, None)
    sel = f > 20.0   # the coupling capacitors turn the phase below a few Hz
    f, ph = f[sel], ph[sel] - 2 * np.pi * np.round(ph[sel][0] / (2 * np.pi))
    i = int(np.argmax(ph < -np.pi / 2))
    return float(np.exp(np.interp(-np.pi / 2, [ph[i], ph[i - 1]], np.log([f[i], f[i - 1]]))))

def probe(cut, pot, fs):
    out = subprocess.run([PROBE, f"{cut}", f"pot:{pot}", "0"] + [f"{x}" for x in fs], capture_output=True, text=True).stdout
    return np.array([float(l.split()[1]) for l in out.strip().split("\n")])

def main():
    worst = 0.0
    for ctl in (-1.0, 1.0, 3.0, 5.0):
        fc = w0_spice(ctl)
        fs = [fc * r for r in REL]
        f0, d0, _ = spice_curve(ctl, 0.0, fs)
        ref_s = np.interp(min(100.0, fc / 10), f0, d0)
        ref_d = probe(fc, 0.0, [min(100.0, fc / 10)])[0]
        print(f"\nctl {ctl:.4g}: SPICE w0 = {fc:.1f} Hz.  rows: pot; cols: f / w0 = {REL}; SPICE / DSP / diff dB")
        for pot in POTS:
            f, d, _ = spice_curve(ctl, pot, fs)
            s = np.interp(fs, f, d) - ref_s
            p = probe(fc, pot, fs) - ref_d
            worst = max(worst, float(np.max(np.abs(s - p))))
            print(f"  pot {pot:.1f} S " + " ".join(f"{x:6.1f}" for x in s))
            print(f"          D " + " ".join(f"{x:6.1f}" for x in p))
            print(f"          d " + " ".join(f"{x:+6.2f}" for x in p - s))
    print(f"\nworst |DSP - SPICE| = {worst:.2f} dB")

if __name__ == "__main__":
    main()
