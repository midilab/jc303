# MS-20 filter fidelity tools

JUCE-free harnesses for the circuit-native Korg MS-20 filter (`src/dsp/open303/dfl_MS20Filter.*`, the Korg35
revision of the MS-10 / early MS-20), plus an ngspice model of the circuit to check it against. Run them after any
change to the filter.

## Sources

- T. E. Stinchcombe, *A Study of the Korg MS10 & MS20 Filters* (2006),
  https://www.timstinchcombe.co.uk/synth/MS20_study.pdf: the Korg35's internal circuit (Fig. 1), transfer functions, the reverse-saturated transistor "resistors", the diode limiters, the 6 dB/oct HP wiring.
- Korg MS-20 service manual: KLM-127 (old production) IC9 / IC10 Korg35 HPF / LPF values.

## Build

```sh
tools/ms20-fidelity/build.sh      # into tools/ms20-fidelity/build/ (git-ignored); needs a C++17 compiler (CXX, default clang++)
```

## Tools

| Tool | What it does |
|---|---|
| `build/probe <cutoff> <res\|pot:x> <mode> <f>...` | small-signal magnitude (dB) by sine probe, 4 x 44.1 kHz |
| `build/render <cutoff> <pot> <mode> <amp V> <freq> <secs> [kick V] [bias] [clip]` | renders in circuit volts (float32 on stdout) for SPICE comparisons |
| `build/levels [noteHz]` | saw output level per mode / pot / drive: the BP / HP trims and the drive make-up were fitted with it |
| `build/stress` | 1152 grid cases (every knob at its extremes, drive -3..+16 dB, 20 Hz..20 kHz cutoff sweeps, 4 x and 1 x rate): finite, bounded, rings down below the oscillation threshold; plus 6 Newton lock-up cases (BP, knob 0.95, +16 dB, bias 0.5: sweep and 6 / 10 kHz): peak < 4, < 1 % non-converged; prints the grid's non-converged count and ns / sample. Exit 0 = pass |
| `spice/ms20_k35.py` | ngspice netlist generator (the header lists the unverified guesses) |
| `spice/compare_ac.py` | DSP vs SPICE small-signal response, 4 cutoffs x pot positions, at SPICE's own natural frequency (no fitting) |
| `spice/tran_compare.py` | self-oscillation (onset, frequency, amplitude, asymmetry) and distortion vs level |
| `spice/mode_compare.py` | BP / HP injection points: response shape vs SPICE |

Needs ngspice (`brew install ngspice`) and numpy for the `spice/` scripts; run them from `spice/` after `build.sh`.

## Results (current tree)

- Small signal (`compare_ac`): Korg35 within ~1 dB at 470 Hz and 2 kHz at every pot position below oscillation
  (the very top of a near-oscillating peak can read a few dB off: it is that sharp). At 8 kHz the DSP's corner sits
  ~1 % above SPICE's (+0.4 dB at pot 0, the peak +1.4 / +2.4 dB at pot 0.5 / 0.6): the model's, not the
  discretisation's (the corner is prewarped).
- Distortion (`tran_compare`): passband 0.1..4 V within 0.02 dB / 0.03 % THD, near the peak within 0.15 dB.
- Self-oscillation: onset near pot 0.73, amplitude within ~2 %, the negative half swinging harder as in the circuit;
  frequency exact at 470 Hz, 1 % high at 2 kHz, 5 % high at 8 kHz.
- Modes (`mode_compare`): BP / HP shapes within ~0.7 dB.
- Stress: 0 of 1158 failures, 6685 of 254 M grid samples non-converged; ~450-470 ns / sample at 4 x 44.1 kHz (~8 % of one core).

## Notes

- Cutoff = the core's small-signal natural frequency at resonance 0: the control voltage at Q1's collector source is
  found from a DC operating-point table (rebuilt when Sharpness changes), at the prewarped corner
  (fs / pi) tan(pi fc / fs), so the trapezoidal solve's corner lands on the cutoff at any sample rate.
- The nominal input is 0.22 V for a full-scale signal at 0 dB drive: the VCF's 3 Vp-p maximum input through the
  open HPF (gain 0.148), where the forward-path diodes just bend (~4 % THD on a full-scale sine); +16 dB (1.4 V) is
  the hot end (~28 %). See `K35_INPUT_VOLTS`.
- At PEAK 1 in BP with Clip / Bite at their extremes the Korg35 self-oscillates near its op amp's rails, about +35 dB
  re the passband: bounded, but loud.
- Not modelled: the exponential converters (the control is ideal), component tolerances, temperature drift.
