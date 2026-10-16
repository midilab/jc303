"""A mutant 303 ladder with per-rung current control: for each of the four rungs above Q12 (Q22 nb->0, Q16/Q23 0->1,
Q14/Q17 1->2, Q13/Q15 2->3), an extra current dI is injected at the rung's upper node(s) and taken out at its lower
nodes (half per side), so only that rung's current changes; dI = 0 at rest, so the quiescent point is the stock one.
Rung j is controlled by the swing of the capacitor below it, s_j = tanh^2((V(Lj) - V(Rj)) / 2Vt):
  A (envelope, the hardware Tension): rung current x exp(T 2.8 env_j), env_j = s_j smoothed over 2 ms (RC)
  B (instantaneous Duffing):          rung current x (1 + k s_j), no smoothing
(ideal behavioural sources: in hardware, matched source / sink transistor pairs). Steady sines at 0.03-6.5 V, pot
0.95, cutoff 1350 Hz: the resonant peak vs level; then rest stability (1 mV input).

  python3 mutant.py      (run from this directory; several minutes on 10 cores)"""
import os, sys, subprocess, hashlib, tempfile, numpy as np
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, '.')
from tb303_vcf import netlist
from native_targets import If_for
C = os.path.join(tempfile.gettempdir(), "diode_mutant_cache"); os.makedirs(C, exist_ok=True)
VS = 2 * 0.025865
UP = {0: ["nb"], 1: ["L0", "R0"], 2: ["L1", "R1"], 3: ["L2", "R2"]}

def mutate(n, kind, amt, If):
    if kind is None: return n
    add = []
    for j in range(4):
        s = f"tanh((V(L{j})-V(R{j}))/{VS})^2"
        if kind == "A":
            add += [f"Bs{j} es{j} 0 V={s}", f"Rs{j} es{j} env{j} 1k", f"Cs{j} env{j} 0 2u"]
            m = f"min(max(exp({amt}*2.8*V(env{j})),0.2),5)"
        else:
            m = f"max(1+({amt})*{s},0.2)"
        add.append(f"Bm{j} m{j} 0 V={m}")
        dI = f"{If}*(V(m{j})-1)"
        if j == 0: add.append(f"Bu{j} 0 nb I={dI}")
        else: add += [f"Bul{j} 0 {UP[j][0]} I=0.5*{dI}", f"Bur{j} 0 {UP[j][1]} I=0.5*{dI}"]
        add += [f"Bdl{j} L{j} 0 I=0.5*{dI}", f"Bdr{j} R{j} 0 I=0.5*{dI}"]
    return n.replace("Itail tail 0", "\n".join(add) + "\nItail tail 0")

def run(n):
    k = hashlib.md5(n.encode()).hexdigest()[:14]; d = f"{C}/{k}.dat"
    if not os.path.exists(d):
        m = n.replace(".end\n", f".control\nrun\nwrdata {C}/{k}.tmp v(vout)\nquit\n.endc\n.end\n"); open(f"{C}/{k}.cir", "w").write(m)
        r = subprocess.run(["ngspice", "-b", f"{C}/{k}.cir"], capture_output=True, text=True)
        if not os.path.exists(f"{C}/{k}.tmp"): print(r.stdout[-600:]); return None
        os.replace(f"{C}/{k}.tmp", d)
    x = np.loadtxt(d); return x[:, 0], x[:, 1]

def gain(kind, amt, f, amp, pot=0.95, cut=1350.0):
    If = If_for(cut); dt = min(4e-6, 1 / f / 200)
    r = run(mutate(netlist(pot, If, mode="tran", amp=amp, freq=f, tran=f".tran {dt} 0.4 0 {dt}\n.option reltol=1e-4 abstol=1e-13\n"), kind, amt, If))
    if r is None: return np.nan
    t, v = r; nc = int(0.1 * f); T = nc / f; ts = np.linspace(0.4 - T, 0.4, 1 << 14, endpoint=False)
    x = np.interp(ts, t, v); x -= x.mean(); return abs(np.fft.rfft(x)[nc]) * 2 / len(x) / amp

CUT = 1350.0; FS = np.geomspace(CUT * 0.5, CUT * 1.6, 49); AMPS = (0.03, 1.0, 3.25, 6.5)
CFGS = [("stock", None, 0), ("A envelope, T = -1 (floppy)", "A", -1.0), ("A envelope, T = +1 (stiff)", "A", 1.0),
        ("B instantaneous, k = -0.8", "B", -0.8), ("B instantaneous, k = +2", "B", 2.0), ("B instantaneous, k = +4", "B", 4.0)]

def peak(row):
    i = int(np.nanargmax(row))
    if 0 < i < len(row) - 1:
        y = 20 * np.log10(row[i - 1:i + 2]); x = np.log(FS[i - 1:i + 2]); d = (y[0] - y[2]) / (2 * (y[0] - 2 * y[1] + y[2]))
        return float(np.exp(x[1] + d * (x[2] - x[1])))
    return FS[i]

def main():
    jobs = [(c[1], c[2], f, a) for c in CFGS for a in AMPS for f in FS]
    with ThreadPoolExecutor(os.cpu_count() or 4) as ex: g = list(ex.map(lambda j: gain(*j), jobs))
    g = np.array(g).reshape(len(CFGS), len(AMPS), len(FS))
    print("pot 0.95, cutoff 1350 Hz: resonant peak (Hz) vs sine amplitude")
    print("config".ljust(30) + "".join(f"{a:>9.2f} V" for a in AMPS) + "   shift 0.03->6.5 V   peak gain at 0.03 V")
    for k, (name, _, _) in enumerate(CFGS):
        pk = [peak(g[k, j]) for j in range(len(AMPS))]
        print(name.ljust(30) + "".join(f"{p:11.0f}" for p in pk) + f"   {1200 * np.log2(pk[-1] / pk[0]):+7.0f} cents      {20*np.log10(np.nanmax(g[k,0])):6.1f} dB")

if __name__ == "__main__":
    main()
