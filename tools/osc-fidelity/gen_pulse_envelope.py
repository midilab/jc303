"""Generate dfl::BlendOscillator's ENV_TABLE (TB-303 pulse envelope) from the ngspice model of the Q8 waveshaper.

For each pitch, the collector-high segment of the SPICE waveform is sampled from where the rise starts (level 0.02)
to the falling 50% crossing, relative to the low level (5.333 V, transistor off) and to the high-pitch reference
height (mean peak above the low level at 261-523 Hz). ENV_LEAD is how far the rise starts before the rising 50%
crossing, as a fraction of the 50%-to-50% segment (the duty); ENV_EDGE_END is the first table point past the end of the
circuit's rising edge, as a position in the extended segment (the band-limited-step mode holds the envelope there over the edge). Table points sit at (i / (ENV_COLS - 1))^2 of the
extended segment, dense at the start so the rising edge keeps its shape at high pitch. The falling edge (the saw
reset, under 1 us) is held at the value where it starts, since the band-limited pulse provides it. Prints C++ to
paste into src/dsp/open303/dfl_BlendOscillator.h. Run from this directory; ~1 minute.
  python3 gen_pulse_envelope.py"""
import sys, numpy as np
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, '.'); import tb303_vco_q8 as v

FREQS = [10, 15, 22, 27.5, 32.7032, 41.2, 49, 55, 65.4064, 82.4, 98, 130.81, 196, 261.63, 523.25]
NT = 48
VLO = 5.3333

def wave(f):
    t, vc, vs, ve = v.run(f); T = 1 / f; n = 16384
    ts = t[-1] - T + np.arange(n) * T / n
    return f, np.interp(ts, t, vc)

def segment(c, ref):
    n = len(c); u = (c - VLO) / ref; hi = u > 0.5 * u.max()
    i0 = np.argmax(np.diff(hi.astype(int)) == 1) + 1; u = np.roll(u, -i0); hi = np.roll(hi, -i0)
    m = np.argmin(hi)                                                   # 50% rise -> 50% fall
    k = n - 1
    while k > n // 2 and u[k] > 0.02: k -= 1                            # walk back to where the rise starts
    pre = n - k
    y = np.r_[u[k:], u[:m]]; x = np.arange(len(y)) / len(y)             # extended segment, 0..1
    dy = np.gradient(y, x)
    k1 = len(y) - 1 - np.argmax(((dy > -2.0) & (x < 0.998))[::-1])      # start of the fast falling edge
    k0 = pre + np.argmax((dy[pre:] < 2.0))                                # end of the circuit's rising edge (past the 50% point)
    th = (np.arange(NT) / (NT - 1)) ** 2
    env = np.interp(th, x, y); env[th > x[k1]] = y[k1]
    # snap the edge end to the first table point past the knee: interpolating across the knee would hold too low a
    # value and leave a steep few-sample climb after it, which aliases
    i0 = int(np.ceil(np.sqrt(x[k0]) * (NT - 1)))
    return pre / m, th[i0], env

if __name__ == "__main__":
    with ThreadPoolExecutor(8) as ex: waves = list(ex.map(wave, FREQS))
    ref = np.mean([c.max() - VLO for f, c in waves if 261 <= f <= 524])
    print(f"    // reference height {ref:.3f} V", file=sys.stderr)
    segs = [segment(c, ref) for f, c in waves]
    rows = [e for _, _, e in segs]
    print(f"    static constexpr int ENV_ROWS = {len(FREQS)}, ENV_COLS = {NT};")
    print("    static constexpr double ENV_FREQ[ENV_ROWS] = { " + ", ".join(f"{f:g}" for f in FREQS) + " };")
    print("    static constexpr double ENV_LEAD[ENV_ROWS] = { " + ", ".join(f"{a:.4f}" for a, _, _ in segs) + " };")
    print("    static constexpr double ENV_EDGE_END[ENV_ROWS] = { " + ", ".join(f"{e:.4f}" for _, e, _ in segs) + " };")
    print("    static constexpr double ENV_TABLE[ENV_ROWS][ENV_COLS] = {")
    print(",\n".join("      { " + ", ".join(f"{x:.3f}" for x in r) + " }" for r in rows) + " };")
