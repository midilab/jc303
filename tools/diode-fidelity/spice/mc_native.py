"""Component-tolerance Monte Carlo of the real VCF, mapped onto Diode Native's parameters.

For each randomised unit (mc.perturb: resistors +-5 %, film caps +-10 %, electrolytics +-20 %, transistor IS / BF
lognormal, matched duals shared; plus the resonance pot's total resistance +-20 %) and each cutoff, records:
  * f10: the -10 dB frequency at pot 0 (cutoff scale), relative to the nominal unit;
  * boost and peak / cutoff at pot positions 0.6 / 0.9 / 1.0 (native_targets conventions);
  * bass: 65 Hz level relative to 500 Hz at pot 0.9.
Writes mc_native.json (seed -1 = nominal). Run from this directory; ~1-2 minutes for 100 units.
  python3 mc_native.py [N=100] [--pot-tol 0.2]"""
import sys, json, uuid, os, subprocess, numpy as np
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, '.')
from tb303_vcf import netlist
from mc import perturb
from native_targets import If_for

CUTS = (500, 1500, 5000)
POTS = (0.0, 0.6, 0.9, 1.0)

def unit_netlist(seed, pot, cut, pot_tol):
    n = netlist(pot, If_for(cut), mode="ac").replace(".ac dec 60 1 20000", f".ac dec 400 20 {min(60e3, 3 * cut)}")
    if seed < 0:
        return n
    rng = np.random.default_rng(seed)
    n = perturb(n, rng)
    k = 1.0 + np.random.default_rng(10_000 + seed).uniform(-pot_tol, pot_tol)   # same pot for every pot position
    out = []
    for line in n.split("\n"):
        t = line.split()
        if t and t[0] in ("RVt", "RVb"):
            frac = f"(1-{pot})" if t[0] == "RVt" else f"{pot}"
            line = f"{t[0]} {t[1]} {t[2]} {{{frac}*{50e3 * k:.6g} + 1}}"
        out.append(line)
    return "\n".join(out)

def ac(args):
    seed, pot, cut, pot_tol = args
    base = f"/tmp/_mcn_{uuid.uuid4().hex[:10]}"
    n = unit_netlist(seed, pot, cut, pot_tol).replace(".end\n", f".control\nrun\nwrdata {base}.dat vdb(vout)\nquit\n.endc\n.end\n")
    open(base + ".cir", "w").write(n)
    subprocess.run(["ngspice", "-b", base + ".cir"], capture_output=True)
    try:
        d = np.loadtxt(base + ".dat"); r = (d[:, 0], d[:, 1])
    except Exception:
        r = None
    for ext in (".cir", ".dat"):
        try: os.remove(base + ext)
        except OSError: pass
    return args, r

def main():
    N = int(sys.argv[1]) if len(sys.argv) > 1 and not sys.argv[1].startswith("--") else 100
    pot_tol = float(sys.argv[sys.argv.index("--pot-tol") + 1]) if "--pot-tol" in sys.argv else 0.2
    jobs = [(s, p, c, pot_tol) for s in range(-1, N) for c in CUTS for p in POTS]
    with ThreadPoolExecutor(os.cpu_count() or 8) as ex: res = dict(ex.map(ac, jobs))
    out = {}
    for s in range(-1, N):
        for c in CUTS:
            data = {p: res[(s, p, c, pot_tol)] for p in POTS}
            if any(v is None for v in data.values()): continue
            f, d0 = data[0.0]
            ref = np.interp(0.2 * c, f, d0); f10 = f[np.argmax((f > 0.2 * c) & (d0 < ref - 10))]
            row = {"f10": float(f10), "boost": {}, "fr": {}}
            for p in POTS[1:]:
                fp, dp = data[p]; sel = (fp > 0.4 * c) & (fp < 2.4 * c); i = np.argmax(np.where(sel, dp, -1e9))
                row["boost"][str(p)] = float(dp[i] - np.interp(fp[i], f, d0)); row["fr"][str(p)] = float(fp[i] / c)
            f9, d9 = data[0.9]; row["bass"] = float(np.interp(65, f9, d9) - np.interp(500, f9, d9))
            out[f"{s}|{c}"] = row
    json.dump(out, open("mc_native.json", "w"))
    print(f"{N} units, pot tolerance +-{pot_tol:.0%}: {sum(1 for k in out if not k.startswith('-1'))} unit/cutoff results")

if __name__ == "__main__":
    main()
