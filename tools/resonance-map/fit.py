"""Fits dfl_ResonanceMap.h's breakpoints from `measure` output: knob -> pot per model so every model reaches the same
peak height (dB above the passband) at the same knob position and self-oscillates from KNOB_ONSET.

  ./build/measure 1000 > build/measure_1000.txt && python3 fit.py build/measure_1000.txt

Target (knob, peak dB): through ANCHORS by monotone cubic; KNOB_ONSET maps to the model's onset pot and the knob's
top (KNOB_ONSET..1) linearly onto pot onset..1, so each circuit keeps its own behaviour past the threshold. A model
that never self-oscillates (onset 1, the EDP Wasp) follows the targets to KNOB_LAST_TARGET, then runs linearly to
the end of its pot."""
import sys, re, numpy as np

ANCHORS = [(0.0, 0.0), (0.45, 6.0), (0.70, 12.0), (0.82, 20.0), (0.86, 24.0)]
KNOB_ONSET = 0.90
KNOBS = [0.0, 0.15, 0.30, 0.45, 0.58, 0.70, 0.76, 0.82, 0.86]
KNOB_LAST_TARGET = 0.82   # no self-oscillation: the last knob position held to the targets

def pchip(xs, ys, x):
    xs, ys = np.asarray(xs, float), np.asarray(ys, float)
    h = np.diff(xs); d = np.diff(ys) / h
    m = np.zeros_like(ys)
    for i in range(1, len(xs) - 1):
        if d[i - 1] * d[i] > 0:
            w1, w2 = 2 * h[i] + h[i - 1], h[i] + 2 * h[i - 1]
            m[i] = (w1 + w2) / (w1 / d[i - 1] + w2 / d[i])
    m[0], m[-1] = d[0], d[-1]
    i = min(max(np.searchsorted(xs, x) - 1, 0), len(xs) - 2)
    t = (x - xs[i]) / h[i]
    return (ys[i] * (2*t**3 - 3*t**2 + 1) + h[i] * m[i] * (t**3 - 2*t**2 + t)
            + ys[i + 1] * (-2*t**3 + 3*t**2) + h[i] * m[i + 1] * (t**3 - t**2))

def models(path):
    out, name = {}, None
    for line in open(path):
        if not line.startswith(" "):
            name = line.strip(); out[name] = {"p": [], "db": []}
        elif "peak" in line:
            _, p, _, db, _ = line.split(); out[name]["p"].append(float(p)); out[name]["db"].append(float(db))
        elif "onset" in line:
            out[name]["onset"] = float(line.split()[1])
    return out

def invert(p, db, target):
    db = np.maximum.accumulate(db)            # monotone (the measurement's last digits can wobble)
    if target <= db[0]: return p[0]
    i = int(np.argmax(db >= target))
    if db[i] < target: raise ValueError(f"{target} dB not reached before the onset")
    return p[i - 1] + (p[i] - p[i - 1]) * (target - db[i - 1]) / (db[i] - db[i - 1])

ms = models(sys.argv[1])
ak, ad = zip(*ANCHORS)
for name, m in ms.items():
    target = lambda k: invert(np.array(m["p"]), np.array(m["db"]), pchip(ak, ad, k)) if k > 0 else 0.0
    if m["onset"] < 0.999:
        pts = [(k, target(k)) for k in KNOBS] + [(KNOB_ONSET, m["onset"]), (1.0, 1.0)]
    else:
        held = [k for k in KNOBS if k <= KNOB_LAST_TARGET]
        pts = [(k, target(k)) for k in held]
        k0, p0 = pts[-1]; rest = np.linspace(k0, 1.0, 11 - len(pts) + 1)[1:]
        pts += [(k, p0 + (1.0 - p0) * (k - k0) / (1.0 - k0)) for k in rest]
    ks, ps = zip(*pts)
    print(f"  // {name}: " + (f"onset at pot {m['onset']:.4f}" if m["onset"] < 0.999 else "no self-oscillation"))
    print("  { { " + ", ".join(f"{k:.2f}" for k in ks) + " },\n    { " + ", ".join(f"{p:.4f}" for p in ps) + " } }")
