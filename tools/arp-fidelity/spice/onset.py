"""Self-oscillation onset (RES pot) of SPICE vs the DSP: bisection on a kick (the ring still alive after 0.3 s).

  python3 onset.py [mod] [fc ...]      (run from this directory after ../build.sh; mod: the 2k2 mod in both)"""
import os, sys, subprocess, numpy as np
import arp4075
from spice_util import run

SR = 4 * 44100.0
MOD = len(sys.argv) > 1 and sys.argv[1] == "mod"
ENV = dict(os.environ, ARP_MOD="1" if MOD else "0")

def alive_spice(fc, pot):
    d = run(arp4075.netlist(pot=pot, fc=fc, analysis="tran", amp=0.0, tstop=0.35, tstep=4e-6, kick=2.0, mod=MOD), ["v(out)"])
    t, v = d[:, 0], d[:, 1]
    e = np.ptp(v[(t > 0.05) & (t < 0.08)]); l = np.ptp(v[t > 0.32])
    return l > 1e-3 and l > 0.3 * e

def alive_dsp(fc, pot):
    out = subprocess.run(["../build/render", f"{fc}", f"{pot}", "0", "0", "100", "0.35", "2.0"], capture_output=True, env=ENV).stdout
    v = np.frombuffer(out, dtype=np.float32).astype(float); t = np.arange(len(v)) / SR
    e = np.ptp(v[(t > 0.05) & (t < 0.08)]); l = np.ptp(v[t > 0.32])
    return l > 1e-3 and l > 0.3 * e

def onset(alive, fc, lo=0.6, hi=1.0, n=9):
    for _ in range(n):
        m = 0.5 * (lo + hi); (lo, hi) = (lo, m) if alive(fc, m) else (m, hi)
    return 0.5 * (lo + hi)

if __name__ == "__main__":
    for fc in [float(x) for x in sys.argv[1 + MOD:]] or [300.0, 1000.0, 4000.0]:
        print(f"fc {fc:6.0f}: onset pot SPICE {onset(alive_spice, fc):.3f} | DSP {onset(alive_dsp, fc):.3f}")
