"""SPICE model of the TB-303 square waveshaper (Q8) fed by an ideal VCO saw; measures square duty vs pitch.

Requires ngspice (brew install ngspice) and numpy. Usage: python3 tb303_vco_q8.py [--bf 205] [--rsrc 500] [--treset 5e-6]

Values from Roland TB-303 Service Notes (1982, main-board schematic, VCO section), cross-checked against
x0xb0x scope captures and JTriggerFish's Tf303Oscillator report.

VERIFIED (read off the service-notes schematic):
  Q8 2SA733 PNP common emitter; R45 22k emitter->12V; C11 1uF (1/50) emitter->GND;
  R35 100k saw->base; C10 10nF + R34 10k in series saw->base; R36 10k collector->5.333V reference.
  Saw taken at Q28 (2SK30 source follower, R105 10k to GND). Saw is a FALLING ramp with fast upward reset.
  Square sketch in the notes: ~5V low / ~8V high, 9 ms period at key A (110 Hz, CV 2.75V); CV C=1.000V, C'=4.000V.
MEASURED (x0xb0x clone, Tek TDS2014B captures on ladyada build page):
  saw 5.60 Vpp, mean 8.49 V @ 139.7 Hz; square 3.80 Vpp, mean 7.14 V @ 134.7 Hz.
GUESSED / ASSUMED (not on the schematic):
  - saw modelled as an ideal piecewise-linear falling ramp 11.25 V -> 5.75 V (8.5 V +- 2.75 V), reset time TRESET
    (default 5 us) and source resistance RSRC (default 500 ohm, a guess for the 2SK30 follower's 1/gm || R105);
    in the real unit the saw amplitude/DC is ~constant over pitch (cap-reset core with comparator thresholds).
  - Q8 model: the widely circulated '2sa733' Gummel-Poon card (KiCad-Spice-Library BJT.lib), BF=205 by default;
    2SA733P rank is BF 200-400, so --bf 300 is also worth running.
  - waveform-switch / filter-input load on the collector (C17 1uF + R62 220k) omitted (>20x R36).
Duty = fraction of the period the collector is above the midpoint of its steady-state min/max ("collector high").
"""
import argparse, os, subprocess, tempfile
import numpy as np

Q2SA733 = (".model Q2SA733 PNP (IS=55.9f BF={bf} NF=1.01201 VAF=135 IKF=462.6m ISE=180f NE=1.65 BR=2.048 NR=1.012 "
           "RB=20 RE=10m RC=1.7 CJE=35p VJE=900m MJE=500m TF=700p XTF=10 VTF=10 ITF=700m CJC=13p VJC=750m MJC=400m "
           "XCJC=500m TR=10n XTB=2.00849 EG=1.11 XTI=3 FC=500m)")


def netlist(freq, bf=205.0, rsrc=500.0, treset=5e-6, vhi=11.25, vlo=5.75, tstop=None, tmax=None):
    T = 1.0 / freq
    tstop = tstop or max(0.6, 25 * T)
    tmax = tmax or min(T / 2000, 2e-6)
    return f"""* TB-303 VCO saw -> Q8 square shaper  f={freq} Hz
{Q2SA733.format(bf=bf)}
V12  vdd 0 DC 12
V5   vref 0 DC 5.3333
* ideal falling saw (Q28 source): vhi -> vlo over T-treset, reset back up in treset
Vsaw sawi 0 PULSE({vhi} {vlo} 0 {T - treset} {treset} 1n {T})
Rsrc sawi saw {rsrc}
R35  saw b 100k
C10  saw x 10n
R34  x b 10k
R45  vdd e 22k
C11  e 0 1u
Q8   c b e Q2SA733
R36  c vref 10k
.tran {tmax} {tstop} 0 {tmax}
.option reltol=1e-4 abstol=1e-12 vntol=1e-6
"""


def run(freq, **kw):
    n = netlist(freq, **kw)
    with tempfile.TemporaryDirectory() as d:
        dat = os.path.join(d, "o.dat")
        n += f".control\nrun\nwrdata {dat} v(c) v(saw) v(e)\nquit\n.endc\n.end\n"
        cir = os.path.join(d, "q8.cir")
        open(cir, "w").write(n)
        r = subprocess.run(["ngspice", "-b", cir], capture_output=True, text=True)
        if not os.path.exists(dat):
            raise RuntimeError(r.stdout[-2000:] + r.stderr[-2000:])
        d_ = np.loadtxt(dat)
    return d_[:, 0], d_[:, 1], d_[:, 3], d_[:, 5]


def measure(t, vc, vsaw, ve, freq, ncyc=4, npts=8192):
    T = 1.0 / freq
    t0 = t[-1] - ncyc * T
    ts = t0 + np.arange(ncyc * npts) * (T / npts)
    c = np.interp(ts, t, vc)
    s = np.interp(ts, t, vsaw)
    e = np.interp(ts, t, ve)
    lo, hi = c.min(), c.max()
    mid = 0.5 * (lo + hi)
    duty = np.mean(c > mid)
    # where in the saw cycle the high pulse sits: mean saw voltage while collector high (low => late in falling ramp)
    saw_when_high = s[c > mid].mean()
    # rise/fall 10-90% times (one cycle)
    cyc = c[-npts:]
    a_, b_ = lo + 0.1 * (hi - lo), lo + 0.9 * (hi - lo)
    band = (cyc > a_) & (cyc < b_)
    dv = np.gradient(np.r_[cyc[-1], cyc, cyc[0]])[1:-1]
    trise = np.sum(band & (dv > 0)) * T / npts
    tfall = np.sum(band & (dv < 0)) * T / npts
    return dict(freq=freq, duty=duty, vpp=hi - lo, vmean=c.mean(), vhi=hi, vlo=lo, ve=e.mean(),
                saw_when_high=saw_when_high, saw_mean=s.mean(), trise=trise, tfall=tfall)


C1 = 32.7032
FREQS = [10.0, 22.0] + [C1 * 2 ** (k / 2) for k in range(0, 9)] + [85.0, 135.0, 785.0, 1000.0]

if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--bf", type=float, default=205.0)
    ap.add_argument("--rsrc", type=float, default=500.0)
    ap.add_argument("--treset", type=float, default=5e-6)
    ap.add_argument("--freqs", type=str, default="")
    a = ap.parse_args()
    freqs = [float(x) for x in a.freqs.split(",")] if a.freqs else sorted(FREQS)
    print(f"# BF={a.bf} Rsrc={a.rsrc} treset={a.treset}")
    print("  freq_Hz  duty_hi%  Vpp    Vmean  Vhi    Vlo    Ve_mean  saw@high  trise_us tfall_us")
    for f in freqs:
        t, vc, vs, ve = run(f, bf=a.bf, rsrc=a.rsrc, treset=a.treset)
        m = measure(t, vc, vs, ve, f)
        print(f"{f:9.2f}  {100*m['duty']:6.1f}  {m['vpp']:5.2f}  {m['vmean']:5.2f}  {m['vhi']:5.2f}  {m['vlo']:5.2f}  "
              f"{m['ve']:6.2f}  {m['saw_when_high']:6.2f}  {m['trise']*1e6:8.1f} {m['tfall']*1e6:8.1f}", flush=True)
