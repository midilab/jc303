# Diode filter fidelity tools

JUCE-free harnesses for the Diode / BP / HP filters (`src/dsp/open303/dfl_DiodeLadderFilter.*`):
stability, stress, drive and morph levels, the offline solver behind the feedback law, and a SPICE reference of the
real TB-303 filter. Background and results: [`docs/diode-octave-fidelity.md`](../../docs/diode-octave-fidelity.md).

Run these after any change to the diode filter, the cutoff tuning, the feedback highpass, the drive range or
`Open303` routing. Everything below was run clean on the current tree (see "Expected results").

## Build

```sh
tools/diode-fidelity/build.sh                 # builds into tools/diode-fidelity/build/ (git-ignored)
MATH_APPROX=/path/to/include tools/diode-fidelity/build.sh   # if lib/math_approx-src is not in this checkout
```

Needs a C++17 compiler (`CXX`, default `clang++`) and the `math_approx` headers (CPM fetches them to
`lib/math_approx-src` during a normal configure). No JUCE.

## Tools

| Tool | What it checks | Run time | Pass criteria (exit code 0) |
|---|---|---|---|
| `build/stability_sweep` | Impulse-decay test at resonance 0.97/0.99/1.0 over 40 cutoffs (200-20000 Hz), 9 Filter Feedback corners (57.5-268 Hz), 10 drive values (-12..16 dB; the filter clamps to [-12, +16]) = 10800 cases | ~10 min | every case decays below -60 dB, or (a slow subsonic ring) does not grow over 6 s; prints the worst tail |
| `build/stress_test` | Full synth, Diode (Res Trim 0.85 / 1 / 1.15, full bass comp) and pure BP / HP (Res Trim 1.15) x 5 drives x 3 cutoffs x 3 env mods x FM on/off at resonance 100, accent 100: 450 runs | ~10 s | no non-finite sample, tail after gate-off < -100 dB, peak < 20 |
| `build/drive_level [-v]` | Drive knob is not a volume control (48 settings per filter incl. resonance 0..100): clean region flat, level above 0 dB within per-type limits (Diode -2.5..+3.5 dB, BP / HP wider) | ~1 min | every filter type within limits |
| `build/morph_level` | BP / HP morph: at morph 1 the Diode BP and Diode HP types match Diode LP in level (full synth, 4 cutoffs x 4 resonances x 2 env mods x 3 drives); also prints pure BP / HP vs Diode | ~10 s | worst deviation <= 0.5 dB |
| `fit_drive_trim.py` | Fits the drive level tables `DT` (run with `DT` zeroed; paste the printed arrays) | ~1 min | helper |
| `build/render_note` | Renders one note to raw float32 on stdout (`render_note <filterType> <cutoff> <res> <env> <decayMs> <accent> <gateOffSec> <totalSec> <driveDb> <morph> <midiNote> [fbHpHz] [bias] [resTrim]`) | - | helper |
| `gen_native_tables.py` + `spice/native_targets.py` + `build/native_solver` | Re-solves the Diode tables (`KNT`, `KNC`) against SPICE; run after changing the ladder, `CUTOFF_TUNING`, the coupling network or the netlist | ~3 min | prints C++ arrays |
| `build/native_output [-v]` | Diode's output network (`dfl_TB303OutputNetwork.h`) against SPICE, ladder top to jack, 5 resonance-pot positions x 7 frequencies (10 Hz-1 kHz), levels re pot 0 | ~1 s | within 0.5 dB (1 dB below 20 Hz) |
| `spice/` | ngspice reference of the real filter (see below) | seconds each | see the scripts |

### Expected results (current tree, drive -3 dB)

- stability sweep: 0 failures of 10800 (930 slow but decaying)
- stress test: 0 failures of 450, worst tail about -287 dB, worst peak 5.4
- native output: 0 of 35 points out of tolerance, worst 0.54 dB
- drive level: Diode clean region 0.14 dB, +16 dB mean +1.0 dB (worst setting -2.4 / +3.1 dB)
- morph level: morph 1 matches Diode LP exactly; pure BP -0.5 dB, pure HP -0.6 dB vs Diode at resonance 50..70

```sh
cd tools/diode-fidelity && ./build.sh && for t in stability_sweep stress_test native_output drive_level morph_level; do ./build/$t || echo "FAILED: $t"; done
```

## When the tables must be regenerated

The feedback law (`KNT`, `KNC`) depends on `CUTOFF_TUNING`, the coupling network and the ladder; re-solve it with
`gen_native_tables.py` after changing any of those, then re-run the stability sweep. The drive level tables (`DT`)
depend on the drive stage, the output gains and the feedback law: refit them with `fit_drive_trim.py`.

## SPICE reference (`spice/`)

Needs ngspice (`brew install ngspice`) and numpy. Run the scripts from the `spice/` directory.

- `tb303_vcf.py`: netlist generator (also checked in as `docs/tb303-vcf.cir`; its header lists the unverified guesses).
  It sets tight tolerances: with the defaults the operating point is sometimes wrong and AC results jump (see the doc).
- `native_targets.py`: SPICE targets for Diode (response ratio vs pot position and cutoff).
- `run_ac.py`: small-signal AC sweep; `boost_spice.py`: resonant boost versus pot position and cutoff;
  `real303.py`: Stinchcombe's published polynomial for comparison; the netlist matches it to 0.03 dB with resonance off.
- `tran_spice.py`, `ablate.py`: large-signal sine drive and the ablations (linearise Q21 / Q12) behind the finding that
  an input-pair `tanh` explains the real circuit's saturation.
- `mc_native.py`: component-tolerance Monte Carlo mapped onto Diode (cutoff scale, equivalent loop-gain
  scale, boost spread); writes `mc_native.json`. ~5 s for 100 units.
- `mc.py`, `mc_run.py`: component-tolerance Monte Carlo (100 units; ~20 s, run sequentially: a macOS
  `multiprocessing.Pool` without a `__main__` guard fork-bombs).
- `sweep_spice.py`, `tp6.py`: envelope-driven cutoff sweep and the TP6 output-level check against the service manual.
- Pitfalls: SPICE node names are case-insensitive; the feedback polarity needs Q19 driven from `c21a`.
