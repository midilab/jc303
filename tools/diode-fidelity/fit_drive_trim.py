#!/usr/bin/env python3
"""Fits the drive level compensation tables (DT in dfl_DiodeLadderFilter.h).

Renders full-synth notes with build/render_note at the shipped bias (0.5) for every filter type, drive node and resonance node
(mean over 4 cutoffs x 3 env mods), and prints the correction in dB = target - measured, where measured is the
level relative to the -3 dB default drive and the target rises 1 dB from 0 to +16 dB drive. Run it with the DT
tables set to all zeros (the printed values are corrections on top of whatever the build already applies), paste
the printed arrays over DT, rebuild and re-run to check the residual is ~0.

  python3 fit_drive_trim.py"""
import itertools, os, subprocess
import numpy as np
from concurrent.futures import ThreadPoolExecutor
HERE = os.path.dirname(os.path.abspath(__file__)); BIN = os.path.join(HERE, "build", "render_note"); SR = 44100
DRIVES = (-3.0, 0.0, 4.5, 9.0, 12.5, 16.0); RES = (0, 30, 70, 100)
SETTINGS = list(itertools.product((400, 800, 1500, 2394), (0, 60, 100)))
def level(job):
    ft, cut, res, env, drive = job
    a = [BIN, str(ft), str(cut), str(res), str(env), "400", "0", "0.4", "0.6", str(drive), "0", "45", "0", "0.5"]
    x = np.frombuffer(subprocess.run(a, capture_output=True).stdout, dtype=np.float32)[:int(.25 * SR)]
    return 20 * np.log10(np.sqrt((x ** 2).mean()) + 1e-12)
def measure(types):
    jobs = [(ft, c, r, e, d) for ft in types for r in RES for (c, e) in SETTINGS for d in DRIVES]
    with ThreadPoolExecutor(8) as ex: v = list(ex.map(level, jobs))
    it = iter(v); out = {}
    for ft in types:
        for r in RES:
            m = np.array([[next(it) for d in DRIVES] for _ in SETTINGS]); out[(ft, r)] = (m - m[:, :1]).mean(axis=0)
    return out
def target(): return np.array([max(0.0, min(d / 16.0, 1.0)) * 1.0 for d in DRIVES])
M = measure((1, 2, 3, 4))
names = ("octave LP", "plain LP", "plain BP / HP")
for i, name in enumerate(names):
    print(f"        {{ // {name}")
    for r in RES:
        meas = M[(1, r)] if i == 0 else M[(2, r)] if i == 1 else (M[(3, r)] + M[(4, r)]) / 2
        print("          { " + ", ".join(f"{x:.2f}" for x in (target() - meas)) + " },   // resonance " + str(r))
    print("        },")
