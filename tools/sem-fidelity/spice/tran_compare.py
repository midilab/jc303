"""Large-signal comparison of the DSP filter (build/render) against SPICE transients.

  ring:        a kick at 1 ms, then the output over the last 50 ms (p-p, frequency): whether and how it oscillates
  distortion:  sine through the passband and near the peak at rising levels (gain compression, THD, H2), LP output
  character:   the same at Saturation Bias 1 and Rail 0

  python3 tran_compare.py      (run from this directory after ../build.sh)"""
import sys, subprocess, numpy as np
import sem_vcf
from spice_util import run, harmonics

SR = 4 * 44100.0
RENDER = "../build/render"

def dsp(fc, pot, amp, freq, secs, kick=0.0, node="n4", bias=0.0, rail=1.0):
    out = subprocess.run([RENDER, f"{fc}", f"{pot}", "0", f"{amp}", f"{freq}", f"{secs}", f"{kick}", node,
                          f"{bias}", f"{rail}"], capture_output=True).stdout
    v = np.frombuffer(out, dtype=np.float32).astype(float); return np.arange(len(v)) / SR, v

NODES = {"n8": "hp", "n2": "bp", "n4": "lp"}

def spice(fc, pot, amp, freq, secs, kick=0.0, node="n4", bias=0.0, rail=1.0):
    n = sem_vcf.netlist(pot=pot, fc=fc, analysis="tran", amp=amp, freq=freq, tstop=secs, tstep=2e-6, kick=kick,
                         bias=bias, rail=rail)
    d = run(n, [f"v({NODES[node]})"])
    if d[-1, 0] < 0.99 * secs: raise RuntimeError(f"ngspice stopped at {d[-1, 0]:.4f} s of {secs} s")
    return d[:, 0], d[:, 1]

def ring(t, v, tail=0.05):
    m = t > t[-1] - tail; tt, vv = t[m], v[m]
    if np.ptp(vv) < 1e-4: return np.ptp(vv), 0.0
    zc = np.where(np.diff(np.sign(vv - vv.mean())) > 0)[0]
    fr = (len(zc) - 1) / (tt[zc[-1]] - tt[zc[0]]) if len(zc) > 2 else 0.0
    return np.ptp(vv), fr

th = lambda h: 100 * np.sqrt(sum(x * x for x in h[1:])) / h[0]

def distortion(cases, amps, bias=0.0, rail=1.0):
    worst = 0.0
    for fc, pot, rel in cases:
        freq = round(fc * rel)
        print(f"  fc {fc:.0f} pot {pot} sine {freq} Hz")
        ref = None
        for amp in amps:
            hs = harmonics(*spice(fc, pot, amp, freq, 0.25, bias=bias, rail=rail), freq, 0.05)
            hd = harmonics(*dsp(fc, pot, amp, freq, 0.25, bias=bias, rail=rail), freq, 0.05)
            gs, gd = 20 * np.log10(hs[0] / amp), 20 * np.log10(hd[0] / amp)
            if ref is None: ref = (gs, gd)
            worst = max(worst, abs((gd - ref[1]) - (gs - ref[0])))
            print(f"    {amp:4.2f} V: {gs - ref[0]:+6.2f} dB {th(hs):6.2f}% {100 * hs[1] / hs[0]:5.2f}% | "
                  f"{gd - ref[1]:+6.2f} dB {th(hd):6.2f}% {100 * hd[1] / hd[0]:5.2f}%")
    return worst

def main():
    print("ring (kick 2 V at R155, LP over the last 50 ms of 0.3 s): p-p V, freq Hz   SPICE | DSP")
    for fc in (200.0, 1000.0, 4000.0):
        for pot in (0.9, 1.0):
            s = ring(*spice(fc, pot, 0.0, 100.0, 0.3, kick=2.0))
            d = ring(*dsp(fc, pot, 0.0, 100.0, 0.3, kick=2.0))
            print(f"  fc {fc:6.0f} pot {pot:.2f}: {s[0]:7.4f} {s[1]:7.1f} | {d[0]:7.4f} {d[1]:7.1f}")
    print("\ndistortion (LP): input amp V at R155 -> fundamental gain dB re the smallest, THD %, H2 %   SPICE | DSP")
    cases = ((1000.0, 0.0, 0.1), (1000.0, 0.8, 0.89), (1000.0, 0.95, 0.94), (200.0, 0.95, 0.94))
    amps = (0.5, 2.0, 5.0, 10.0, 20.0)
    w = distortion(cases, amps)
    print("\ncharacter: Saturation Bias 1")
    w = max(w, distortion(cases[:2], amps, bias=1.0))
    print("\ncharacter: Rail 0 (supplies sagged 30 %)")
    w = max(w, distortion(cases[:2], amps, rail=0.0))
    print(f"\nworst gain-compression difference {w:.2f} dB")

if __name__ == "__main__":
    main()
