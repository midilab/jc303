"""Generate dfl::BlendOscillator's ENV_TABLE (TB-303 pulse envelope) from the ngspice model of the Q8 waveshaper.

For each pitch, the collector-high segment of the SPICE waveform (from the rising to the falling 50% crossing) is
sampled at ENV_COLS points, relative to the low level (5.333 V, transistor off) and to the high-pitch reference
height (mean peak above the low level at 261-523 Hz). The circuit's fast edges are held at the value where they end
(slope below 2 per unit of segment), since the band-limited pulse provides the edges. Prints C++ to paste into
src/dsp/open303/dfl_BlendOscillator.h. Run from this directory; ~1 minute.
  python3 gen_pulse_envelope.py"""
import sys, numpy as np
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, '.'); import tb303_vco_q8 as v

FREQS = [10, 15, 22, 27.5, 32.7032, 41.2, 49, 55, 65.4064, 82.4, 98, 130.81, 196, 261.63, 523.25]
NT = 33
VLO = 5.3333

def wave(f):
    t, vc, vs, ve = v.run(f); T = 1 / f; n = 16384
    ts = t[-1] - T + np.arange(n) * T / n
    return f, np.interp(ts, t, vc)

def segment(c, ref):
    u = (c - VLO) / ref; hi = u > 0.5 * u.max()
    i0 = np.argmax(np.diff(hi.astype(int)) == 1) + 1; u = np.roll(u, -i0); hi = np.roll(hi, -i0)
    m = np.argmin(hi); y = u[:m]; x = np.arange(m) / m; dy = np.gradient(y, x)
    k0 = np.argmax((dy < 2.0) & (x > 0.002))                       # end of the fast rising edge
    k1 = m - 1 - np.argmax(((dy > -2.0) & (x < 0.998))[::-1])       # start of the fast falling edge
    th = np.linspace(0, 1, NT); env = np.interp(th, x, y)
    env[th < x[k0]] = y[k0]; env[th > x[k1]] = y[k1]
    return env

if __name__ == "__main__":
    with ThreadPoolExecutor(8) as ex: waves = list(ex.map(wave, FREQS))
    ref = np.mean([c.max() - VLO for f, c in waves if f >= 261])
    print(f"    // reference height {ref:.3f} V", file=sys.stderr)
    rows = [segment(c, ref) for f, c in waves]
    print(f"    static constexpr int ENV_ROWS = {len(FREQS)}, ENV_COLS = {NT};")
    print("    static constexpr double ENV_FREQ[ENV_ROWS] = { " + ", ".join(f"{f:g}" for f in FREQS) + " };")
    print("    static constexpr double ENV_TABLE[ENV_ROWS][ENV_COLS] = {")
    print(",\n".join("      { " + ", ".join(f"{x:.3f}" for x in r) + " }" for r in rows) + " };")
