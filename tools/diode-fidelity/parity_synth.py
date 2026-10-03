#!/usr/bin/env python3
"""Full-synth parity of Diode Octave against TeeBee, using build/render_note.

Mean over 96 settings (cutoff 400-2394 Hz, resonance 0/30/70/100, env mod 0/60/100, accent on/off) of the
diode-minus-TeeBee level, peak and spectral centroid, per drive setting; then the 'shriek' check (energy
within +-5% of the dominant peak in the first 80 ms at resonance 100, cutoff 2394 Hz, for env mod 60/80/100).

  python3 parity_synth.py [--check]      --check exits 1 unless the -3 dB (default drive) row is within
                                         |level| < 0.5 dB (std < 0.6 dB), |centroid| < 3 %, peak < 1.5 dB."""
import itertools, os, subprocess, sys
import numpy as np
HERE = os.path.dirname(os.path.abspath(__file__))
BIN = os.path.join(HERE, "build", "render_note")
SR = 44100

def render(ft, cut, res, env, acc, drive, fh=None):
    a = [BIN, str(ft), str(cut), str(res), str(env), "400", str(acc), "0.4", "0.6", str(drive), "0", "45"]
    if fh: a.append(str(fh))
    return np.frombuffer(subprocess.run(a, capture_output=True).stdout, dtype=np.float32)

def stats(x, t1=0.25):
    seg = x[:int(t1 * SR)]; sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))); f = np.fft.rfftfreq(len(seg), 1 / SR)
    m = (f > 60) & (f < 12000)
    return 20 * np.log10(np.sqrt((seg ** 2).mean()) + 1e-12), 20 * np.log10(np.abs(seg).max() + 1e-12), (sp[m] * f[m]).sum() / sp[m].sum()

def shriek(x, t1=0.08):
    seg = x[:int(t1 * SR)]; sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))); f = np.fft.rfftfreq(len(seg), 1 / SR)
    m = (f > 800) & (f < 18000); i = np.argmax(np.where(m, sp, 0)); near = abs(f - f[i]) < 0.05 * f[i]
    return 100 * (sp[near] ** 2).sum() / (sp[m] ** 2).sum(), f[i]

def main():
    if not os.path.exists(BIN): sys.exit("build first: tools/diode-fidelity/build.sh")
    settings = list(itertools.product((400, 800, 1500, 2394), (0, 30, 70, 100), (0, 60, 100), (0, 1)))
    tb = {k: stats(render(0, *k, 0)) for k in settings}
    print("Diode Octave minus TeeBee, mean over 96 settings: level dB (std) / peak dB / centroid %")
    check = {}
    for drive in (-6, -3, 0, 4.5, 9):
        d = np.array([[a - b for a, b in zip(stats(render(1, *k, drive))[:2], tb[k][:2])] + [100 * (stats(render(1, *k, drive))[2] / tb[k][2] - 1)] for k in settings])
        print(f"  drive {drive:+5.1f}: level {d[:, 0].mean():+5.2f} ({d[:, 0].std():.2f})  peak {d[:, 1].mean():+5.2f}  centroid {d[:, 2].mean():+5.1f}% (std {d[:, 2].std():.1f})")
        if drive == -3: check = dict(level=d[:, 0].mean(), std=d[:, 0].std(), peak=d[:, 1].mean(), cen=d[:, 2].mean())
    print("\nshriek energy within +-5% of the dominant peak (first 80 ms), res 100, cutoff 2394, no accent")
    for fh, lab in ((150, "knob default"), (100, "knob 1.0")):
        for env in (60, 80, 100):
            t = shriek(render(0, 2394, 100, env, 0, 0, fh)); row = f"  {lab:12s} env {env:3d}: TeeBee {t[0]:4.1f}% @{t[1]:5.0f} Hz |"
            for drive in (-6, -3, 0): s = shriek(render(1, 2394, 100, env, 0, drive, fh)); row += f" drive {drive:+d}: {s[0]:4.1f}% @{s[1]:5.0f}"
            print(row)
    if "--check" in sys.argv:
        ok = abs(check["level"]) < 0.5 and check["std"] < 0.6 and abs(check["cen"]) < 3 and check["peak"] < 1.5
        print("check:", "PASS" if ok else "FAIL", check); sys.exit(0 if ok else 1)

if __name__ == "__main__":
    main()
