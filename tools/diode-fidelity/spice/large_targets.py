"""Large-signal SPICE targets for the diode filter (build/large_signal checks the DSP against them).

A sine at the VCF input ("in", the VCO amplitude: the real saw is 6.5 Vpp, so amp 3.25 is the 303's level) on a grid
of cutoff x pot x frequency x amplitude. For each point: the steady-state fundamental gain relative to the AC
(small-signal) gain (compression, dB) and the THD (harmonics 2-5, %), at the ladder top v(L0,R0) and at vout.

  python3 large_targets.py [out=large_targets.txt]      (run from this directory; about a minute on 10 cores)
Output lines: cut pot freq amp comp_top thd_top h2_top h3_top comp_out thd_out"""
import sys, subprocess, hashlib, os, tempfile, numpy as np
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, '.')
from tb303_vcf import netlist
from native_targets import If_for
CACHE = os.path.join(tempfile.gettempdir(), "diode_large_cache"); os.makedirs(CACHE, exist_ok=True)
def run(n, outs):
    key = hashlib.md5((n + outs).encode()).hexdigest()[:12]; base = f"{CACHE}/{key}"
    if not os.path.exists(base + ".dat"):
        n = n.replace(".end\n", f".control\nrun\nwrdata {base}.dat {outs}\nquit\n.endc\n.end\n")
        open(base + ".cir", "w").write(n); subprocess.run(["ngspice", "-b", base + ".cir"], capture_output=True)
    return np.loadtxt(base + ".dat")
def ac_gain(cut, pot, freq):
    n = netlist(pot, If_for(cut), mode="ac").replace(".ac dec 60 1 20000", f".ac lin 1 {freq} {freq}")
    d = np.atleast_2d(run(n, "v(L0,R0) v(vout)"))
    return abs(d[0, 1] + 1j * d[0, 2]), abs(d[0, 4] + 1j * d[0, 5])
def harmonics(t, v, freq, tail, nh=5):
    m = t > t[-1] - tail; tt = t[m]; nc = int(round(tail * freq)); T = nc / freq
    ts = np.linspace(tt[-1] - T, tt[-1], 1 << 15, endpoint=False); vs = np.interp(ts, tt, v[m]); vs = vs - vs.mean()
    X = np.fft.rfft(vs) * 2 / len(vs); return [abs(X[k * nc]) for k in range(1, nh + 1)]
def point(job):
    cut, pot, freq, amp = job
    gt, go = ac_gain(cut, pot, freq); tail = max(0.1, 10.0 / freq); dt = min(4e-6, 1 / freq / 200)
    n = netlist(pot, If_for(cut), mode="tran", amp=amp, freq=freq, tran=f".tran {dt} 1.0 0 {dt}\n.option reltol=1e-4 abstol=1e-13\n")
    d = run(n, "v(L0,R0) v(vout)"); t = d[:, 0]
    ht, ho = harmonics(t, d[:, 1], freq, tail), harmonics(t, d[:, 3], freq, tail)
    thd = lambda h: 100 * np.sqrt(sum(x * x for x in h[1:])) / h[0]
    return (f"{cut:g} {pot:g} {freq:g} {amp:g} {20*np.log10(ht[0]/amp/gt):.3f} {thd(ht):.3f} {100*ht[1]/ht[0]:.3f} "
            f"{100*ht[2]/ht[0]:.3f} {20*np.log10(ho[0]/amp/go):.3f} {thd(ho):.3f}")
GRID = [(c, p, f, a) for c in (500, 1350, 3000) for p in (0.0, 0.6, 0.9) for f in (55, 110, 220, 440, 880, 1760)
        if f <= 1.4 * c for a in (1.625, 3.25, 6.5)]
if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "large_targets.txt"
    with ThreadPoolExecutor(os.cpu_count() or 4) as ex: rows = list(ex.map(point, GRID))
    open(out, "w").write("\n".join(rows) + "\n"); print(f"wrote {len(rows)} points to {out}")
