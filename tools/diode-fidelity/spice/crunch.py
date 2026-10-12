"""The driven crunch in the circuit: Q12's input pair offset by b (in units of 2 Vt: Q12a's IS x e^(2b), an offset of
2 b Vt, about 26 mV at b = 0.5) with the loop gain raised by cosh^2 b, as DiodeLadderFilter's normalised bias shaper
does (in hardware: resonance turned up until just under oscillation again; the feedback through an ideal gain, as the
Sharpness option). A saw at the VCF input, resonance pot 1.0. Prints the energy between the note's harmonics in the
output (dB re total; higher = more inharmonic), then note-off tails and a 1 mV probe (does the filter rest, or
oscillate by itself?). Without the gain (offset alone) the pair's gain only climbs back to the symmetric pair's, so
it cannot cross the threshold: included for comparison.

  python3 crunch.py      (run from this directory; about a minute on 10 cores)"""
import os, sys, math, subprocess, hashlib, tempfile, numpy as np
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, '.')
from tb303_vcf import netlist
from native_targets import If_for

CACHE = os.path.join(tempfile.gettempdir(), "diode_crunch_cache"); os.makedirs(CACHE, exist_ok=True)
F0 = 65.41   # C2

def build(b, amp, cut, norm=True, tstop=0.8, dt=4e-6, gate=None):
    G = math.cosh(b) ** 2 if norm else 1.0
    n = netlist(1.0, If_for(cut), mode="tran", amp=amp, freq=F0,
                tran=f".tran {dt} {tstop} 0 {dt}\n.option reltol=1e-4 abstol=1e-13\n")
    n = n.replace(".model QN12A NPN (IS=1e-14 ", f".model QN12A NPN (IS={1e-14 * math.exp(2 * b)} ")
    n = n.replace("R15 fbr inR 10k", f"R15d fbr nin 12.2k\nE15 fbx nin fbr nin {G}\nR15 fbx inR 10k")
    T = 1.0 / F0; A = amp * G
    saw = f"PULSE({-A} {A} 0 {T - 1e-6} 1e-6 0 {T})"
    src = f"Vin in 0 DC 0 {saw}" if gate is None else (
        f"Vsaw sw 0 DC 0 {saw}\nSg sw in g 0 SWM\nRgo in 0 1meg\nVg g 0 PULSE(1 0 {gate} 1u 1u 10 20)\n"
        ".model SWM SW(VT=0.5 RON=1 ROFF=1e9)")
    vin = [l for l in n.splitlines() if l.startswith("Vin in 0")][0]
    return n.replace(vin, src)

def run(n):
    key = hashlib.md5(n.encode()).hexdigest()[:14]; dat = f"{CACHE}/{key}.dat"
    if not os.path.exists(dat):
        m = n.replace(".end\n", f".control\nrun\nwrdata {CACHE}/{key}.tmp v(vout)\nquit\n.endc\n.end\n")
        open(f"{CACHE}/{key}.cir", "w").write(m); subprocess.run(["ngspice", "-b", f"{CACHE}/{key}.cir"], capture_output=True)
        os.replace(f"{CACHE}/{key}.tmp", dat)
    d = np.loadtxt(dat); return d[:, 0], d[:, 1]

def window(t, v, t0, t1):
    ts = np.arange(t0, t1, 1 / 44100.0); x = np.interp(ts, t, v); return x - x.mean()

def inharm(t, v):
    x = window(t, v, 0.3, 0.8)
    X = np.abs(np.fft.rfft(x * np.hanning(len(x)))) ** 2; f = np.fft.rfftfreq(len(x), 1 / 44100.0)
    d = np.abs(f / F0 - np.round(f / F0)) * F0; m = f > 20
    return 10 * np.log10(np.sum(X[m & (d > 0.25 * F0)]) / np.sum(X[m]) + 1e-30)

def level(t, v, t0, t1):
    x = window(t, v, t0, t1); X = np.abs(np.fft.rfft(x * np.hanning(len(x)))); f = np.fft.rfftfreq(len(x), 1 / 44100.0)
    return 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-30), f[np.argmax(X * (f > 20))]

def main():
    jobs = [(b, amp, cut, True) for b in (0.0, 0.3, 0.5, 0.71, 0.85) for amp in (3.25, 10.0, 20.0, 40.0) for cut in (1000.0, 3000.0)]
    jobs += [(b, amp, cut, False) for b in (0.5, 0.71) for amp in (3.25, 20.0, 40.0) for cut in (1000.0, 3000.0)]
    with ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        res = list(ex.map(lambda j: inharm(*run(build(*j))), jobs))
    print("b     saw amp V  cutoff  loop gain | between harmonics, dB re total")
    for (b, amp, cut, norm), ih in zip(jobs, res):
        print(f"{b:4.2f}  {amp:6.2f}   {cut:6.0f}  {'cosh^2 b' if norm else 'stock   '}  | {ih:6.1f}")
    print("\nnote off at 0.6 s (amp 40 V, cutoff 1 kHz): output 0.65-0.8 s | 1.0-1.2 s")
    for b in (0.0, 0.5, 0.71):
        t, v = run(build(b, 40.0, 1000.0, tstop=1.2, gate=0.6))
        a, c = level(t, v, 0.65, 0.8), level(t, v, 1.0, 1.2)
        print(f"  b {b:4.2f}: {a[0]:6.1f} dBV @ {a[1]:4.0f} Hz | {c[0]:6.1f} dBV @ {c[1]:4.0f} Hz")
    print("\n1 mV input (does it rest?): output 1.0-1.2 s")
    for b in (0.5, 0.71, 0.85):
        t, v = run(build(b, 1e-3, 1000.0, tstop=1.2)); c = level(t, v, 1.0, 1.2)
        print(f"  b {b:4.2f}: {c[0]:6.1f} dBV @ {c[1]:4.0f} Hz")

if __name__ == "__main__":
    main()
