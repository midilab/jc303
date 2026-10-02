# Diode filter fidelity tools

JUCE-free harnesses for the Diode Octave / Diode / BP / HP filters (`src/dsp/open303/dfl_DiodeLadderFilter.*`):
stability, stress, parity with the TeeBee, the offline solver behind the Diode Octave feedback law, and a SPICE
reference of the real TB-303 filter. Background and results: [`docs/diode-octave-fidelity.md`](../../docs/diode-octave-fidelity.md).

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
| `build/stability_sweep` | Impulse-decay test of Diode Octave at resonance 0.97/0.99/1.0 over 40 cutoffs (200-20000 Hz), 7 feedback-HP corners, 10 drive values (-12..16 dB; the filter clamps to [-12, +9]) = 8400 cases | ~2.5 min | every case decays below -60 dB; prints the worst tail |
| `build/stress_test` | Full synth, all 4 diode types x 4 drives x 3 cutoffs x 3 env mods x FM on/off at resonance 100, accent 100: 288 runs | ~10 s | no non-finite sample, tail after gate-off < -100 dB, peak < 20 |
| `build/plain_modes` | Self-oscillation onset of plain Diode / BP / HP (bisection on the resonance knob) at 5 cutoffs, drive -6 and +4.5 | ~10 s | onset 83-95 % everywhere (design 87-92 %), none at 80 %, sustained at 100 % |
| `build/parity_cells [-v] [drive]` | Small-signal parity vs TeeBee: boost, peak frequency, passband, stopband shape over 30 cells (500 Hz-10 kHz, resonance 30/60/100, both feedback-knob positions) | ~10 s | boost term <= 0.30 (worst <= 4 dB), peak-frequency term <= 0.10, stopband shape <= 1.0 |
| `parity_synth.py [--check]` | Full-synth level / peak / brightness vs TeeBee over 96 settings per drive, plus the max-cutoff "shriek" check; uses `build/render_note` | ~15 s | `--check`: at the default -6 dB drive level within 0.5 dB (std < 0.6), brightness within 3 %, peak < 1.5 dB |
| `build/render_note` | Renders one note to raw float32 on stdout (`render_note <filterType> <cutoff> <res> <env> <decayMs> <accent> <gateOffSec> <totalSec> <driveDb> <morph> <midiNote> [fbHpHz]`) | - | helper |
| `gen_k_tables.py` + `build/k_target_solver` | Re-solves the Diode Octave feedback-law tables (see below) | ~4 min | prints C++ arrays |
| `spice/` | ngspice reference of the real filter (see below) | seconds each | see the scripts |

### Expected results (current tree, drive -6 dB, bass comp fixed)

- stability sweep: 0 failures, worst tail about -138 dB
- stress test: 0 failures, worst tail about -297 dB
- plain modes: onset 86.9-92.4 % (the knob spans 0..0.8 of the old range)
- parity cells: boost term 0.12 (worst 3.0 dB), peak-frequency term 0.04, passband spread 0.09, stopband shape 0.60
- parity synth at -6 dB drive: level +0.02 dB (std 0.32), peak +0.75 dB, brightness +1.0 %

```sh
cd tools/diode-fidelity && ./build.sh && for t in stability_sweep stress_test plain_modes parity_cells; do ./build/$t || echo "FAILED: $t"; done
python3 parity_synth.py --check
```

## When the feedback-law tables must be regenerated

The Diode Octave resonance (`calculateCoefficients`) is `K = K100(cutoff, fbHP) * rho(res, cutoff)`, capped at
0.99 of the measured self-oscillation threshold, from tables fitted so the diode's resonant boost equals the
TeeBee's. The tables depend on `CUTOFF_TUNING_OCTAVE`, the low-cutoff tuning boost, the loop highpass and the
ladder. After changing any of those:

```sh
tools/diode-fidelity/build.sh
python3 tools/diode-fidelity/gen_k_tables.py > /tmp/tables.txt     # ~4 minutes
```

Paste the three arrays (`K100T`, `KCT`, `RHOT`) over the ones in `calculateCoefficients`, then re-run the
stability sweep and the parity tools. The solver overrides `K` *and* `Kcomp` directly (the output compensation
depends on the resonance, so overriding only `K` inflates the solved K by about 20 %). Skipping the
regeneration after a tuning change left 24 low-cutoff, drive-9 stability cases marginal once.

## SPICE reference (`spice/`)

Needs ngspice (`brew install ngspice`) and numpy. Run the scripts from the `spice/` directory.

- `tb303_vcf.py`: netlist generator (also checked in as `docs/tb303-vcf.cir`; its header lists the unverified guesses).
- `run_ac.py`: small-signal AC sweep; `boost_spice.py`: resonant boost versus pot position and cutoff;
  `real303.py`: Stinchcombe's published polynomial for comparison; the netlist matches it to 0.03 dB with resonance off.
- `tran_spice.py`, `ablate.py`: large-signal sine drive and the ablations (linearise Q21 / Q12) behind the finding that
  an input-pair `tanh` explains the real circuit's saturation.
- `mc.py`, `mc_run.py`: component-tolerance Monte Carlo (100 units; ~20 s, run sequentially: a macOS
  `multiprocessing.Pool` without a `__main__` guard fork-bombs).
- `sweep_spice.py`, `tp6.py`: envelope-driven cutoff sweep and the TP6 output-level check against the service manual.
- Pitfalls: SPICE node names are case-insensitive; the feedback polarity needs Q19 driven from `c21a`.
