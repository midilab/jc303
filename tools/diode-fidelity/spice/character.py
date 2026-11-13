"""The diode filter's character controls against SPICE: Bite / Sharpness / Warmth realised in the netlist (tb303_vcf.py's
bite / sharp / warm), a sine at the VCF input, compression (fundamental re its own small-signal gain, dB), THD and H2 (%)
at the output, SPICE (vout) vs the DSP (../build/character, Hi-Fi). Clip is checked by its identity instead
(../build/character identity) plus the plain large-signal match at the scaled level.

  python3 character.py      (run from this directory after ../build.sh; a few minutes on 10 cores)"""
import os, sys, subprocess, hashlib, tempfile, uuid, numpy as np
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, '.')
from tb303_vcf import netlist
from native_targets import If_for

SR = 4 * 44100.0
CACHE = os.path.join(tempfile.gettempdir(), "diode_char_cache"); os.makedirs(CACHE, exist_ok=True)

def run(n, outs):
    key = hashlib.md5((n + outs).encode()).hexdigest()[:14]; dat = f"{CACHE}/{key}.dat"
    if not os.path.exists(dat):
        base = f"{CACHE}/{key}_{uuid.uuid4().hex[:8]}"   # unique: points sharing a netlist may run concurrently
        m = n.replace(".end\n", f".control\nrun\nwrdata {base}.dat {outs}\nquit\n.endc\n.end\n")
        open(base + ".cir", "w").write(m); subprocess.run(["ngspice", "-b", base + ".cir"], capture_output=True)
        os.replace(base + ".dat", dat); os.remove(base + ".cir")
    return np.atleast_2d(np.loadtxt(dat))

def harmonics(t, v, freq, tail, nh=5):
    m = t > t[-1] - tail; tt = t[m]; nc = int(round(tail * freq)); T = nc / freq
    ts = np.linspace(tt[-1] - T, tt[-1], 1 << 15, endpoint=False); vs = np.interp(ts, tt, v[m]); vs = vs - vs.mean()
    X = np.fft.rfft(vs) * 2 / len(vs); return [abs(X[k * nc]) for k in range(1, nh + 1)]

def spice_point(cut, pot, freq, amp, ch):
    kw = dict(bite=ch[0], sharp=ch[1], warm=ch[2])
    n = netlist(pot, If_for(cut), mode="ac", **kw).replace(".ac dec 60 1 20000", f".ac lin 1 {freq} {freq}")
    d = run(n, "v(vout)"); g = abs(d[0, 1] + 1j * d[0, 2])
    tail = max(0.1, 10.0 / freq); dt = min(4e-6, 1 / freq / 200)
    n = netlist(pot, If_for(cut), mode="tran", amp=amp, freq=freq, tran=f".tran {dt} 1.0 0 {dt}\n.option reltol=1e-4 abstol=1e-13\n", **kw)
    d = run(n, "v(vout)"); h = harmonics(d[:, 0], d[:, 1], freq, tail)
    return 20 * np.log10(h[0] / amp / g), h

def dsp_point(cut, pot, freq, amp, ch):
    def render(a):
        out = subprocess.run(["../build/character", f"{cut}", f"{pot}", f"{a}", f"{freq}", "1.0", f"{ch[0]}", "0.5",
                              f"{ch[1]}", f"{ch[2]}", "1"], capture_output=True).stdout
        v = np.frombuffer(out, dtype=np.float32).astype(float); return np.arange(len(v)) / SR, v
    tail = max(0.1, 10.0 / freq)
    hs = harmonics(*render(1e-3), freq, tail); h = harmonics(*render(amp), freq, tail)
    return 20 * np.log10((h[0] / amp) / (hs[0] / 1e-3)), h

SETTINGS = [(0.0, 0.5, 0.0), (0.5, 0.5, 0.0), (1.0, 0.5, 0.0), (0.0, 0.75, 0.0), (0.0, 1.0, 0.0),
            (0.0, 0.5, 0.5), (0.0, 0.5, 1.0), (1.0, 1.0, 1.0)]
POINTS = [(1350.0, pot, f, a) for pot in (0.0, 0.6, 0.9) for f in (110.0, 440.0) for a in (3.25, 6.5)]

def main():
    th = lambda h: 100 * np.sqrt(sum(x * x for x in h[1:])) / h[0]
    jobs = [(p, ch) for ch in SETTINGS for p in POINTS]
    with ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        sp = list(ex.map(lambda j: spice_point(*j[0], j[1]), jobs))
    worst = {}
    print("(bite, sharp, warm) cutoff pot freq amp: compression dB, THD %, H2 %   SPICE | DSP")
    for (p, ch), s in zip(jobs, sp):
        d = dsp_point(*p, ch)
        print(f"  {ch} {p[0]:.0f} {p[1]:.1f} {p[2]:4.0f} {p[3]:4.2f}: {s[0]:+6.2f} {th(s[1]):6.2f} {100*s[1][1]/s[1][0]:5.2f} | "
              f"{d[0]:+6.2f} {th(d[1]):6.2f} {100*d[1][1]/d[1][0]:5.2f}")
        e = worst.setdefault(ch, [0.0, 0.0, 0.0])
        e[0] = max(e[0], abs(s[0] - d[0])); e[1] = max(e[1], abs(th(s[1]) - th(d[1])))
        e[2] = max(e[2], abs(100 * s[1][1] / s[1][0] - 100 * d[1][1] / d[1][0]))
    print("\nworst |DSP - SPICE| per setting: compression dB, THD pts, H2 pts")
    for ch, e in worst.items():
        print(f"  {ch}: {e[0]:.2f} dB  {e[1]:.2f}  {e[2]:.2f}")

if __name__ == "__main__":
    main()
