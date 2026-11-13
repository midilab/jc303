"""Large-signal comparison of the DSP filter (build/render) against SPICE transients (real 2N5087s).

  self-oscillation: a kick at 1 ms, then the steady ring (p-p, frequency) at the output
  distortion:       sine at the mixer pot through the passband and near the peak at rising levels (gain compression,
                    THD, H2), LP output; also at Rail 0

  python3 tran_compare.py      (run from this directory after ../build.sh)"""
import subprocess, numpy as np
import arp4075
from spice_util import run, harmonics

SR = 4 * 44100.0
RENDER = "../build/render"

def dsp(fc, pot, amp, freq, secs, kick=0.0, rail=1.0):
    out = subprocess.run([RENDER, f"{fc}", f"{pot}", "0", f"{amp}", f"{freq}", f"{secs}", f"{kick}", "out", "0", f"{rail}"],
                         capture_output=True).stdout
    v = np.frombuffer(out, dtype=np.float32).astype(float); return np.arange(len(v)) / SR, v

def spice(fc, pot, amp, freq, secs, kick=0.0, rail=1.0):
    d = run(arp4075.netlist(pot=pot, fc=fc, analysis="tran", amp=amp, freq=freq, tstop=secs, tstep=2e-6, kick=kick,
                            rail=rail), ["v(out)"])
    return d[:, 0], d[:, 1]

def ring(t, v, tail=0.05):
    m = t > t[-1] - tail; tt, vv = t[m], v[m]
    if np.ptp(vv) < 1e-4: return np.ptp(vv), 0.0
    zc = np.where(np.diff(np.sign(vv - vv.mean())) > 0)[0]
    fr = (len(zc) - 1) / (tt[zc[-1]] - tt[zc[0]]) if len(zc) > 2 else 0.0
    return np.ptp(vv), fr

th = lambda h: 100 * np.sqrt(sum(x * x for x in h[1:])) / h[0]

def distortion(cases, amps, rail=1.0):
    worst = 0.0
    for fc, pot, rel in cases:
        freq = round(fc * rel)
        print(f"  fc {fc:.0f} pot {pot} sine {freq} Hz")
        ref = None
        for amp in amps:
            hs = harmonics(*spice(fc, pot, amp, freq, 0.25, rail=rail), freq, 0.05)
            hd = harmonics(*dsp(fc, pot, amp, freq, 0.25, rail=rail), freq, 0.05)
            gs, gd = 20 * np.log10(hs[0] / amp), 20 * np.log10(hd[0] / amp)
            if ref is None: ref = (gs, gd)
            worst = max(worst, abs((gd - ref[1]) - (gs - ref[0])))
            print(f"    {amp:5.2f} V: {gs - ref[0]:+6.2f} dB {th(hs):6.2f}% {100 * hs[1] / hs[0]:5.2f}% | "
                  f"{gd - ref[1]:+6.2f} dB {th(hd):6.2f}% {100 * hd[1] / hd[0]:5.2f}%")
    return worst

def main():
    print("ring (kick 2 V at the mixer pot, output over the last 50 ms of 0.35 s): p-p V, freq Hz   SPICE | DSP")
    for fc in (300.0, 1000.0, 4000.0):
        for pot in (0.85, 0.9, 1.0):
            s = ring(*spice(fc, pot, 0.0, 100.0, 0.35, kick=2.0))
            d = ring(*dsp(fc, pot, 0.0, 100.0, 0.35, kick=2.0))
            print(f"  fc {fc:6.0f} pot {pot:.2f}: {s[0]:7.3f} {s[1]:7.1f} | {d[0]:7.3f} {d[1]:7.1f}")
    print("\ndistortion (LP): input amp V at the mixer pot -> fundamental gain dB re the smallest, THD %, H2 %   SPICE | DSP")
    cases = ((1000.0, 0.0, 0.1), (1000.0, 0.5, 0.8), (1000.0, 0.75, 0.9), (200.0, 0.75, 0.9))
    amps = (0.5, 2.0, 5.0, 10.0, 20.0)
    w = distortion(cases, amps)
    print("\nRail 0 (the +15 V and -14 V supplies sagged 60 %)")
    w = max(w, distortion(cases[:1] + cases[2:3], amps, rail=0.0))
    print(f"\nworst gain-compression difference {w:.2f} dB")

if __name__ == "__main__":
    main()
