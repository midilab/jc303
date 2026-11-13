"""BP / HP injection points: DSP vs SPICE response shape (each normalised to its own maximum), plus raw levels.

  python3 mode_compare.py"""
import subprocess, numpy as np
from spice_util import ac_db
import ms20_k35
from compare_ac import w0_spice, REL, PROBE

def dsp(cut, pot, mode, fs):
    out = subprocess.run([PROBE, f"{cut}", f"pot:{pot}", f"{mode}"] + [f"{x}" for x in fs], capture_output=True, text=True).stdout
    return np.array([float(l.split()[1]) for l in out.strip().split("\n")])

ctl = 1.0
fc = w0_spice(ctl)
rel = (0.05, 0.1, 0.25) + REL + (10.0,)
fs = [fc * r for r in rel]
print(f"w0 {fc:.0f} Hz; cols f / w0 = {rel}")
for mname, m in (("bp", 0.5), ("hp", 1.0)):
    for pot in (0.0, 0.5):
        net = ms20_k35.netlist(a=pot, vctl=ctl, mode=mname, ppd=100, fstart=5, fstop=60e3)
        f, d = ac_db(net, "nE"); s = np.interp(fs, f, d)
        p = dsp(fc, pot, m, fs)
        print(f" {mname} pot {pot}: S " + " ".join(f"{x - s.max():6.1f}" for x in s) + f"   (max {s.max():+.1f} dB re input)")
        print(f"            D " + " ".join(f"{x - p.max():6.1f}" for x in p) + f"   (max {p.max():+.1f} dB, DSP normalised)")
