"""ngspice runners shared by the MS-20 SPICE scripts (run from this directory)."""
import os, subprocess, uuid
import numpy as np

# Device models shared by the MS-20 netlists. 2SC1623 BF / BR from the PSpice model Stinchcombe quotes; the rest are
# generic stand-ins (1S1588 as 1N4148). OPA: 4558 / uA741 as a one-pole op amp, 3 MHz GBW, +-13.5 V swing.
MODELS = """.model Q2SC1623 NPN (IS=1.8e-14 BF=206.7 BR=4.21 NF=1 NR=1 VAF=100 VAR=20 RB=10 RC=1 RE=0.5 CJE=8p CJC=4p TF=0.4n TR=10n)
.model Q2SA812 PNP (IS=1e-14 BF=200 VAF=50 RB=10 CJE=10p CJC=5p TF=0.5n)
.model J2SK30 NJF (VTO=-1.0 BETA=1.0m LAMBDA=2m CGS=5p CGD=5p)
.model D1S1588 D (IS=2.52n N=1.752 RS=0.568 CJO=4p)
.subckt OPA inp inn out
E1 n1 0 inp inn 1e5
Rp n1 n2 1k
Cp n2 0 5.3u
Bo out 0 V = 13.5*tanh(V(n2)/13.5)
.ends
"""

def run(net, vectors, keep=False):
    """Runs a netlist in batch mode and returns wrdata columns: [x, v1, (v1 imag,) v2, ...] as an array."""
    base = f"/tmp/_ms20_{uuid.uuid4().hex[:10]}"
    ctl = f".control\nrun\nwrdata {base}.dat {' '.join(vectors)}\nquit\n.endc\n"
    open(base + ".cir", "w").write(net.replace(".end\n", ctl + ".end\n"))
    r = subprocess.run(["ngspice", "-b", base + ".cir"], capture_output=True, text=True)
    if not os.path.exists(base + ".dat"):
        raise RuntimeError(f"ngspice failed ({base}.cir):\n" + r.stdout[-2000:] + r.stderr[-2000:])
    d = np.loadtxt(base + ".dat")
    if not keep:
        for ext in (".cir", ".dat"):
            try: os.remove(base + ext)
            except OSError: pass
    return d

def ac_db(net, node):
    """(f, dB) at node from an .ac netlist."""
    d = run(net, [f"vdb({node})"])
    return d[:, 0], d[:, 1]

def tran(net, node):
    """(t, v) at node from a .tran netlist."""
    d = run(net, [f"v({node})"])
    return d[:, 0], d[:, 1]

def harmonics(t, v, freq, tail, nh=6):
    """Amplitudes of the first nh harmonics over the last `tail` seconds (integer cycles, resampled uniformly)."""
    m = t > t[-1] - tail; tt = t[m]; vv = v[m] - v[m].mean()
    nc = int(tail * freq); T = nc / freq
    ts = np.linspace(tt[0], tt[0] + T, 1 << 15, endpoint=False); vs = np.interp(ts, tt, vv)
    X = np.fft.rfft(vs)
    return [abs(X[k * nc]) * 2 / len(vs) for k in range(1, nh + 1)]

def peak(f, db, lo, hi):
    """(peak frequency, peak dB) of db within lo..hi Hz."""
    sel = (f > lo) & (f < hi); i = np.argmax(np.where(sel, db, -1e9))
    return f[i], db[i]

def corner(f, db, drop=3.0):
    """First frequency where db falls `drop` dB below its low-frequency plateau (median of the lowest decade)."""
    ref = np.median(db[f < f[0] * 10]); i = np.argmax(db < ref - drop)
    return f[i]
