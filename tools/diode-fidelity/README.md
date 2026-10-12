# Diode filter fidelity tools

JUCE-free harnesses for the diode filter (LP / BP / HP mode morph) (`src/dsp/open303/dfl_DiodeLadderFilter.*`):
stability, stress, drive and mode levels, the offline solver behind the feedback law, and a SPICE reference of the
real TB-303 filter. Background and results: [`docs/diode-fidelity.md`](../../docs/diode-fidelity.md).

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
| `build/stability_sweep [--standard] [--tension T] [--kick A] [--strict] [--bias S] [--bite B] [--clip C] [--sharp S] [--warm W]` | Impulse-decay test (Hi-Fi, or Standard with `--standard`; `--kick 3`: a loud 20 ms burst at the cutoff instead of the impulse, for large-signal latches; `--tension`: Hi-Fi rung tension) at resonance 0.97/0.99/1.0 over 40 cutoffs (200-20000 Hz), 9 Filter Feedback corners (57.5-268 Hz), 10 drive values (-12..16 dB; the filter clamps to [-12, +16]) = 10800 cases | ~10 min | every case decays below -60 dB, or (a slow subsonic ring) does not grow over 6 s; after a loud kick, a ring still within 20 dB of its peak is reported as a latch (the driven crunch, intended: see docs/diode-fidelity.md) and fails only with `--strict`; prints the worst tail |
| `build/stress_test` | Full synth, both qualities, Diode at Res Trim 0.85 / 1 / 1.15 (LP) and 1.15 at mode 0.25 / 0.5 / 0.75 / 1, full bass comp, x 5 drives x 3 cutoffs x 3 env mods x FM on/off at resonance 100, accent 100, Hi-Fi at tension -1 / 0 / 1: 2520 runs | ~2 min | no non-finite sample, tail after gate-off < -100 dB, peak < 26 (stiff tension, HP, Res Trim 1.15, -3 dB: 24.9) |
| `build/drive_level [-v]` | Drive knob is not a volume control (both qualities, 48 settings per filter incl. resonance 0..100): clean region flat, level above 0 dB within per-mode limits (LP -3.0..+3.5 dB, Hi-Fi LP -5.0..+5.5 dB, BP / HP wider) | ~1 min | every filter type within limits |
| `build/mode_level` | Mode morph: level of mode 0.25 / 0.5 / 0.75 / 1 vs LP (full synth, resonance 50..70, cutoff 800..1500); small-signal shape over 21 mode steps (no dip deeper than 3 dB below its 1/3-octave neighbours, i.e. no notch); BP / HP slopes | ~6 s | level within 1.5 dB, no notch, HP >= 30 dB down at cutoff / 8, BP >= 12 dB at cutoff / 8 and x 8 |
| `build/model_level` | Global level between filter models (`Open303::FILTER_LEVEL`): Spicy vs TeeBee at resonance 0 (saw, C2 / C3 / C4 x cutoff 400 / 1000 / 2500) | ~2 s | within 0.25 dB |
| `build/large_signal` | Large-signal response against SPICE (`spice/large_targets.txt`): sine at the VCF input, 144 points (cutoff x pot x note x level -6 / 0 / +6 dB around the real 303's), compression and THD at the output, both qualities | ~10 s | rms within per-quality limits (Hi-Fi 0.15 dB / 0.25 pts at the real level) |
| `fit_drive_trim.py` | Fits the drive level tables `DT` (run with `DT` zeroed; paste the printed arrays) | ~1 min | helper |
| `build/render_note` | Renders one note to raw float32 on stdout (`render_note <filterType> <cutoff> <res> <env> <decayMs> <accent> <gateOffSec> <totalSec> <driveDb> <bassComp> <midiNote> [fbHpHz] [bias] [resTrim] [mode]`) | - | helper |
| `gen_native_tables.py` + `spice/native_targets.py` + `build/native_solver` | Re-solves the Diode tables (`KNT`, `KNC`) against SPICE; run after changing the ladder, `CUTOFF_TUNING`, the coupling network or the netlist | ~3 min | prints C++ arrays |
| `build/native_output [-v]` | Diode's output network (`dfl_TB303OutputNetwork.h`) against SPICE, ladder top to jack, 5 resonance-pot positions x 7 frequencies (10 Hz-1 kHz), levels re pot 0 | ~1 s | within 0.5 dB (1 dB below 20 Hz) |
| `spice/` | ngspice reference of the real filter (see below) | seconds each | see the scripts |

### Expected results (current tree)

- stability sweep: Hi-Fi and Standard 0 failures of 10800 (0 slow), also with `--kick 3` (0 slow, worst tail -62 dB)
- stress test: 0 failures of 2520, worst tail about -274 dB, worst peak 21.6 (HP, Res Trim 1.15, +16 dB)
- native output: 0 of 35 points out of tolerance, worst 0.54 dB
- drive level: Hi-Fi LP +16 dB mean +1.0 dB (worst setting -3.3 / +3.1 dB, cutoff-dependent at the top of the knob); Standard LP +1.0 dB (-2.8 / +3.1); BP / HP within limits
- model level: Spicy vs TeeBee -0.09 dB at resonance 0 (+2.3 dB at 50 and 100; only resonance 0 is matched)
- mode level: 0.25 / 0.5 / 0.75 / 1 at -1.3 / 0.0 / -1.2 / 0.0 dB vs LP; worst dip 0.08 dB
- large signal: Hi-Fi 0.07 dB / 0.11 pts rms at the real level (0.11 / 0.31 at +6 dB); Standard 0.20 / 0.43 (0.45 / 1.52)

```sh
cd tools/diode-fidelity && ./build.sh && for t in stability_sweep stress_test native_output drive_level mode_level model_level large_signal; do ./build/$t || echo "FAILED: $t"; done
```

## Character controls

`DiodeLadderFilter::setBite` / `setClipThreshold` / `setSharpness` / `setWarmth` (0 / 0.5 / 0.5 / 0 = the circuit), each
a part of the circuit and realised the same way in the netlist (`spice/tb303_vcf.py`'s `bite` / `sharp` / `warm`). Only
Filter Bite is in the Mods menu: by ear the others did little or duplicated existing controls (Clip is Saturation in
disguise, the model being homogeneous; Sharpness acts much like Res Trim; Warmth only once Saturation drives Q21).

| Control | The filter | The netlist |
|---|---|---|
| Bite | Q12's and Q21's saturation-current mismatch. Rung and Q22 mismatches self-centre at DC (every rung carries the same current), so the ladder's asymmetry comes from Q12, whose offset pushes a DC current through every rung (a lopsided ladder and resonance), and from Q21, whose offset its base currents through R113 / R114 mostly cancel (X0 + 5.8 tanh X0 = offset) | Q12a / Q21a IS x e^(0.4 bite) / e^(-0.5 bite) |
| Clip | every junction's Vt x 0.5..2 (the rungs through Tension's scale too), currents scaled to hold the cutoff and gains: the model is homogeneous, so y(x; clip) = s y(x / s), the whole ladder's saturation against the clean passband | none needed: `build/character identity` checks the identity (Tension -1 / 0 / 1, both qualities) |
| Sharpness | Q12's emitters degenerated (RE If / 2 Vt up to 4, the gain renormalised): linear, then a hard knee (a bipolar pair cannot get softer: raising its Vt and renormalising at its dividers cancels) | Q12 emitter resistors, the input and the feedback raised by 1 + rho |
| Warmth | Q21 driven up to 4 x harder, its output scaled back | Q21 NF = 1 / w, BF x w (input impedance kept), R1 / R2 / w |

None raises any element's slope above its rest value, so none can push the loop gain past the symmetric circuit's
(the latch-up mechanism, see docs/diode-fidelity.md). `reset()` settles onto Bite's DC operating point (no step).

- `spice/crunch.py`: the driven crunch in the circuit (Q12 offset + loop gain raised by cosh^2 b, a saw, pot 1.0):
  energy between the note's harmonics, note-off tails and a 1 mV rest probe. See docs/diode-fidelity.md.
- `spice/character.py` (`build/character` renders): at cutoff 1350, pot 0 / 0.6 / 0.9, 110 / 440 Hz, 3.25 / 6.5 V, each
  control at its extremes is within 0.56 dB of compression, 0.7 THD points and 0.3 H2 points of SPICE (the circuit
  itself: 0.26 dB / 0.58 / 0.08); all four at once within 0.49 dB / 0.49 / 2.2.
- Stability (`stability_sweep --kick 3`, both qualities, Tension -1 / 0 / 1, each control at its extremes and all at
  once): see the expected results above.
- Also: Standard quality's filter FM scaled alpha = g / (1 + g) by up to 1.5, which exceeds 1 near Nyquist and ran
  away (found at 1 x); it now scales g, as Hi-Fi does (without FM the path is unchanged, bit for bit).

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
- `large_targets.py`: large-signal targets for `build/large_signal` (writes `large_targets.txt`, ~1 min).
- `tran_spice.py`, `ablate.py`: large-signal sine drive and the ablations (linearise Q21 / Q12) behind the finding that
  an input-pair `tanh` explains the real circuit's saturation.
- `mc_native.py`: component-tolerance Monte Carlo mapped onto Diode (cutoff scale, equivalent loop-gain
  scale, boost spread); writes `mc_native.json`. ~5 s for 100 units.
- `mc.py`, `mc_run.py`: component-tolerance Monte Carlo (100 units; ~20 s, run sequentially: a macOS
  `multiprocessing.Pool` without a `__main__` guard fork-bombs).
- `sweep_spice.py`, `tp6.py`: envelope-driven cutoff sweep and the TP6 output-level check against the service manual.
- Pitfalls: SPICE node names are case-insensitive; the feedback polarity needs Q19 driven from `c21a`.
