"""Large-signal comparison of the DSP filter (build/render) against SPICE transients.

  self-oscillation: a kick at 1 ms, then the steady ring (onset pot, frequency, amplitude, asymmetry)
  distortion:       sine through the passband and near the peak at rising levels (gain compression, THD)

  python3 tran_compare.py      (run from this directory after ../build.sh)"""
import subprocess, numpy as np
from spice_util import tran, harmonics
import ms20_k35
from compare_ac import w0_spice

SR = 4 * 44100.0
RENDER = "../build/render"

def dsp(cut, pot, amp, freq, secs, kick=0.0, mode=0.0, bias=0.0, clip=0.5):
    out = subprocess.run([RENDER, f"{cut}", f"{pot}", f"{mode}", f"{amp}", f"{freq}", f"{secs}", f"{kick}", f"{bias}", f"{clip}"],
                         capture_output=True).stdout
    v = np.frombuffer(out, dtype=np.float32).astype(float); return np.arange(len(v)) / SR, v

def spice(ctl, pot, amp, freq, secs, kick=0.0, bias=0.0, clip=0.5, swap=False):
    n = ms20_k35.netlist(a=pot, vctl=ctl, analysis="tran", amp=amp, freq=freq, tstop=secs, tstep=4e-6, bias=bias, clip=clip, swap=swap)
    node = "nE"
    if kick:
        n = n.replace(f"SIN(0 {amp} {freq})", f"PULSE(0 {kick} 1m 1u 1u 50u 1)")
    return tran(n, node)

def ring(t, v, tail=0.05):
    m = t > t[-1] - tail; tt, vv = t[m], v[m]
    if np.ptp(vv) < 1e-6: return 0.0, 0.0, 0.0, 0.0
    zc = np.where(np.diff(np.sign(vv - vv.mean())) > 0)[0]
    fr = (len(zc) - 1) / (tt[zc[-1]] - tt[zc[0]]) if len(zc) > 2 else 0.0
    return np.ptp(vv), fr, vv.max() - vv.mean(), vv.min() - vv.mean()

def main():
    ctls = (-1.0, 1.0, 3.0)
    pots = (0.6, 0.7, 0.75, 0.8, 0.9, 1.0)
    print("self-oscillation (kick, last 50 ms of 0.4 s): p-p V, freq Hz, +peak / -peak V   SPICE | DSP")
    for ctl in ctls:
        fc = w0_spice(ctl)
        for pot in pots:
            s = ring(*spice(ctl, pot, 0.0, 100.0, 0.4, kick=0.5))
            d = ring(*dsp(fc, pot, 0.0, 100.0, 0.4, kick=0.5))
            print(f"  w0 {fc:7.0f} pot {pot:.2f}: {s[0]:6.3f} {s[1]:7.1f} {s[2]:+.3f}/{s[3]:+.3f} | {d[0]:6.3f} {d[1]:7.1f} {d[2]:+.3f}/{d[3]:+.3f}")
    print("\ndistortion: input amp V -> gain dB re the smallest, THD %, H2 %   SPICE | DSP")
    ctl = ctls[1]; fc = w0_spice(ctl)
    amps = (0.1, 0.3, 1.0, 2.0, 4.0)
    for pot, rel in ((0.0, 0.1), (0.5, 0.95)):
        freq = round(fc * rel)
        print(f"  w0 {fc:.0f} pot {pot} sine {freq} Hz")
        ref = None
        for amp in amps:
            secs = 0.25
            hs = harmonics(*spice(ctl, pot, amp, freq, secs), freq, 0.05)
            hd = harmonics(*dsp(fc, pot, amp, freq, secs), freq, 0.05)
            gs, gd = 20 * np.log10(hs[0] / amp), 20 * np.log10(hd[0] / amp)
            if ref is None: ref = (gs, gd)
            th = lambda h: 100 * np.sqrt(sum(x * x for x in h[1:])) / h[0]
            print(f"    {amp:4.1f} V: {gs - ref[0]:+6.2f} dB {th(hs):6.2f}% {100 * hs[1] / hs[0]:5.2f}% | {gd - ref[1]:+6.2f} dB {th(hd):6.2f}% {100 * hd[1] / hd[0]:5.2f}%")

def character():
    """The saturation bias and the limiter threshold (the plugin's Saturation Bias / Rail): self-oscillation and a
    driven sine through the passband (THD, H2, gain re the circuit).  (bias, clip)   SPICE | DSP"""
    ctl = -1.0; fc = w0_spice(ctl)
    pot = 1.0
    amp, freq = 2.0, 100.0
    th = lambda h: 100 * np.sqrt(sum(x * x for x in h[1:])) / h[0]
    print(f"\ncharacter (w0 {fc:.0f}): (bias, clip) -> osc p-p V, +/- peak | sine {amp} V {freq:.0f} Hz, pot 0: gain dB, THD %, H2 %   SPICE | DSP")
    ref = None
    for bias, clip in ((0.0, 0.5), (0.5, 0.5), (1.0, 0.5), (0.0, 0.25), (0.0, 0.0), (1.0, 0.0)):
        s = ring(*spice(ctl, pot, 0.0, 100.0, 0.4, kick=0.5, bias=bias, clip=clip))
        d = ring(*dsp(fc, pot, 0.0, 100.0, 0.4, kick=0.5, bias=bias, clip=clip))
        hs = harmonics(*spice(ctl, 0.0, amp, freq, 0.25, bias=bias, clip=clip), freq, 0.05)
        hd = harmonics(*dsp(fc, 0.0, amp, freq, 0.25, bias=bias, clip=clip), freq, 0.05)
        if ref is None: ref = (hs[0], hd[0])
        print(f"  ({bias:.1f}, {clip:.2f}): {s[0]:6.3f} {s[2]:+.3f}/{s[3]:+.3f} {20 * np.log10(hs[0] / ref[0]):+5.2f} {th(hs):6.2f}% {100 * hs[1] / hs[0]:5.2f}% | "
              f"{d[0]:6.3f} {d[2]:+.3f}/{d[3]:+.3f} {20 * np.log10(hd[0] / ref[1]):+5.2f} {th(hd):6.2f}% {100 * hd[1] / hd[0]:5.2f}%")

if __name__ == "__main__":
    main()
    character()
