"""Large-signal comparison of the DSP filter (build/render) against SPICE transients.

  self-oscillation: a kick at 1 ms, then the steady ring (frequency, amplitude, asymmetry) at Out4
  distortion:       sine through the passband and near the peak at rising levels (gain compression, THD)

  python3 tran_compare.py      (run from this directory after ../build.sh)"""
import subprocess, numpy as np
import juno_vcf
from spice_util import run, harmonics

SR = 4 * 44100.0
RENDER = "../build/render"

def dsp(fc, pot, amp, freq, secs, kick=0.0, node="out", mode=0.0, ch=(0.0, 0.5, 0.0)):
    bias = ch[3] if len(ch) > 3 else 0.0
    out = subprocess.run([RENDER, f"{fc}", f"{pot}", f"{mode}", f"{amp}", f"{freq}", f"{secs}", f"{kick}", node,
                          f"{ch[0]}", f"{ch[1]}", f"{ch[2]}", "0.5", f"{bias}"], capture_output=True).stdout
    v = np.frombuffer(out, dtype=np.float32).astype(float); return np.arange(len(v)) / SR, v

def spice(fc, pot, amp, freq, secs, kick=0.0, node="out", ch=(0.0, 0.5, 0.0)):
    kw = dict(bite=ch[0], clip=ch[1], warmth=ch[2], bias=ch[3] if len(ch) > 3 else 0.0)
    n = juno_vcf.netlist(ires=juno_vcf.juno_ires(pot), fc=fc, analysis="tran", amp=amp, freq=freq, tstop=secs, tstep=2e-6, kick=kick, **kw)
    node = "nout" if node == "out" else node
    d = run(n, [f"v({node})"]); return d[:, 0], d[:, 1]

def ring(t, v, tail=0.05):
    m = t > t[-1] - tail; tt, vv = t[m], v[m]
    if np.ptp(vv) < 1e-6: return 0.0, 0.0, 0.0, 0.0
    zc = np.where(np.diff(np.sign(vv - vv.mean())) > 0)[0]
    fr = (len(zc) - 1) / (tt[zc[-1]] - tt[zc[0]]) if len(zc) > 2 else 0.0
    return np.ptp(vv), fr, vv.max() - vv.mean(), vv.min() - vv.mean()

def main():
    print("self-oscillation (kick, last 50 ms of 0.3 s): Out4 p-p V, freq Hz, +peak / -peak V   SPICE | DSP")
    for fc in (200.0, 1000.0, 4000.0):
        for pot in (0.9, 0.95, 1.0):
            s = ring(*spice(fc, pot, 0.0, 100.0, 0.3, kick=0.1, node="o4"))
            d = ring(*dsp(fc, pot, 0.0, 100.0, 0.3, kick=0.1, node="o4"))
            print(f"  fc {fc:6.0f} pot {pot:.2f}: {s[0]:6.3f} {s[1]:7.1f} {s[2]:+.3f}/{s[3]:+.3f} | {d[0]:6.3f} {d[1]:7.1f} {d[2]:+.3f}/{d[3]:+.3f}")
    print("\ndistortion (LP output): input amp V -> gain dB re the smallest, THD %, H2 %   SPICE | DSP")
    fc = 1000.0
    th = lambda h: 100 * np.sqrt(sum(x * x for x in h[1:])) / h[0]
    for pot, rel in ((0.0, 0.1), (0.5, 0.9), (0.85, 0.95)):
        freq = round(fc * rel)
        print(f"  fc {fc:.0f} pot {pot} sine {freq} Hz")
        ref = None
        for amp in (0.1, 0.5, 1.0, 1.5, 2.0, 3.0):
            hs = harmonics(*spice(fc, pot, amp, freq, 0.2), freq, 0.05)
            hd = harmonics(*dsp(fc, pot, amp, freq, 0.2), freq, 0.05)
            gs, gd = 20 * np.log10(hs[0] / amp), 20 * np.log10(hd[0] / amp)
            if ref is None: ref = (gs, gd)
            print(f"    {amp:4.1f} V: {gs - ref[0]:+6.2f} dB {th(hs):6.2f}% {100 * hs[1] / hs[0]:5.2f}% | {gd - ref[1]:+6.2f} dB {th(hd):6.2f}% {100 * hd[1] / hd[0]:5.2f}%")

def character():
    """Each character control at its extremes: self-oscillation (Out4) and a resonant sine (output, THD / H2).
    (bite, clip, warmth[, bias]); bias is the saturation bias (setSaturationBias), clip 0..0.5 the plugin's Rail."""
    th = lambda h: 100 * np.sqrt(sum(x * x for x in h[1:])) / h[0]
    fc, pot, amp = 1000.0, 0.95, 0.8
    print(f"\ncharacter: (bite, clip, warmth) -> osc p-p V, +/- peak | sine {amp} V at 300 Hz, RES 0: THD %, H2 %   SPICE | DSP")
    for ch in ((0.0, 0.5, 0.0), (0.5, 0.5, 0.0), (1.0, 0.5, 0.0), (0.0, 0.0, 0.0), (0.0, 0.25, 0.0), (0.0, 1.0, 0.0),
               (0.0, 0.5, 0.5), (0.0, 0.5, 1.0), (1.0, 0.0, 1.0), (0.0, 0.5, 0.0, 0.5), (0.0, 0.5, 0.0, 1.0),
               (0.0, 0.0, 0.0, 1.0)):
        s = ring(*spice(fc, pot, 0.0, 100.0, 0.3, kick=0.1, node="o4", ch=ch))
        d = ring(*dsp(fc, pot, 0.0, 100.0, 0.3, kick=0.1, node="o4", ch=ch))
        hs = harmonics(*spice(fc, 0.0, amp, 300.0, 0.2, ch=ch), 300.0, 0.05)
        hd = harmonics(*dsp(fc, 0.0, amp, 300.0, 0.2, ch=ch), 300.0, 0.05)
        print(f"  {ch}: {s[0]:6.3f} {s[2]:+.3f}/{s[3]:+.3f} {th(hs):6.2f}% {100 * hs[1] / hs[0]:5.2f}% | "
              f"{d[0]:6.3f} {d[2]:+.3f}/{d[3]:+.3f} {th(hd):6.2f}% {100 * hd[1] / hd[0]:5.2f}%")

if __name__ == "__main__":
    main()
    character()
