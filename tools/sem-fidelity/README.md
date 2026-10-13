# Oberheim SEM filter fidelity tools

JUCE-free harnesses for the circuit-native Oberheim SEM filter (`src/dsp/open303/dfl_SEMFilter.*`, the plugin's
"SemiSweet" model), plus an ngspice model of the circuit to check it against. Run them after any change to the filter.

## Sources

- Oberheim SEM-1A schematic (VCF section), via Synthfool:
  http://www.synthfool.com/docs/Oberheim/Oberheim_SEM1A/oberheim_sem_1a.gif
- KVR, "What makes SEM filter so special" (circuit discussion: the damper-style resonance, the CA3080 and FET buffer
  nonlinearities, the OTA output limits): https://www.kvraudio.com/forum/viewtopic.php?t=497961

## The circuit (as modelled)

| Part | Value | Role |
|---|---|---|
| A12 (LM301A) | R155 220k + C15 0.15u in, R139 100k feedback, R138 100k \|\| C18 10p from LP; 30 pF compensation | summing amplifier; output = HP; slews at 0.5 V / us |
| A13 / A15 (CA3080) | R142 / R148 100k, R144 / R150 220R (+ input), R145 / R151 220R (- input) | integrator transconductors (input x 2.2e-3) |
| C20 / C21 | 300 pF to ground | integrator capacitors |
| Q39 / Q40 (2N4302) | source followers, R147 / R153 15k to -15 V | buffers; outputs = BP / LP |
| PS16 RES (50k) | BP to ground, wiper through R157 33k | damping: RES up = the wiper toward ground = less damping |
| R159 / R158, D11 / D12, R156 | 47k / 10k, 1N4148, 100k | damping that grows with level (holds the resonance) |
| C22 | 0.33u | couples both damping paths into A12's input |

Unverified guesses: the 2N4302 as a square-law JFET with IDSS 2 mA, VTO -1.5 V (its datasheet allows 0.5..5 mA); the
CA3080 as an ideal tanh with its output clamped 1.5 V inside the rails; A12 as a two-stage op amp with the LM301A
datasheet's 0.5 V / us and 1 MHz at 30 pF (its input stage a tanh 80 mV wide into the compensation node), DC gain 1e5,
its output clamped 1.5 V inside the supply; the offset trims T9 / T10 at null; the expo converter splitting its current
equally between the OTAs (the 15k R146 / R152); a linear RES pot. The input level (5 V peak at R155 for a full-scale
signal: the SEM's 10 V p-p oscillators) is a guess.

## Behaviour and character controls

The followers' gain below one puts the resonance ~5 % below the nominal cutoff. Damping is heavy at RES 0 (Q ~1/3)
and vanishes at RES 1, where only C18, the loads and the diodes damp: up to ~40 dB of small-signal peaking at low
cutoffs and a slowly dying ring, but no sustained self-oscillation (A12's 1 MHz gain-bandwidth lags the loop a little
at high cutoffs; at 0.77 MHz full RES would self-oscillate above ~3 kHz). The circuit is clean until A12 meets its
rails (about 0.25 % THD at 20 V into R155 in the passband); near the peak the diodes compress the resonance from a few
volts of BP. Hot HP and BP slew: A12's 0.5 V / us gives ~6 kHz of full-power bandwidth at its 13.5 V swing (8 % THD on
HP at 8 kHz from 30 V at R155).

Saturation Bias puts an input offset of up to 2 VT on OTA1 (its trim off null) with the resting current trimmed out:
unequal current limits, even harmonics once the resonance drives it. Saturation Rail sags both supplies by up to 30 %.

The morph is LP / BP / HP (`setMode`: 0 LP, 0.5 BP, 1 HP), mixing the three outputs (they share A12's inversion, so
the crossfades have no notches); BP and HP are trimmed to sit near LP loudness on a saw. The SEM's own MODE pot (PS19,
LP -> notch -> HP) is not modelled.

## Build

```sh
tools/sem-fidelity/build.sh      # into tools/sem-fidelity/build/ (git-ignored); needs a C++17 compiler (CXX, default clang++)
```

## Tools

| Tool | What it does |
|---|---|
| `build/probe <cutoff> <res\|pot:x> <mode\|lp\|bp\|hp> <f>...` | small-signal magnitude (dB) by sine probe, 4 x 44.1 kHz: the output at a mode position, or a circuit node over the outputs' passband gain |
| `build/render <cutoff> <pot> <mode> <amp V> <freq> <secs> [kick V] [node] [bias rail]` | renders in circuit volts (float32 on stdout); node: out, n0..n8 (the circuit nodes, n8 = HP); `render dc` prints the DC operating point |
| `build/levels [noteHz]` | saw output level per mode / pot / drive: the BP / HP trims and the drive make-up were fitted with it |
| `build/stress` | 400 cases (RES 0..1, mode 0 / 0.25 / 0.5 / 0.75 / 1, drive -3..+24 dB, filter FM, bias 1, rail 0, 20 Hz..20 kHz cutoff sweeps, at Open303's fixed 4 x rate): finite, bounded, rings down, every Newton solve converges; prints ns / sample. Exit 0 = pass |
| `spice/sem_vcf.py` | ngspice netlist generator (the same device equations) |
| `spice/compare_ac.py` | DSP vs SPICE small-signal response, 5 cutoffs x 6 pot positions, LP / BP / HP (same Iabc, no fitting) |
| `spice/tran_compare.py` | kick rings, distortion vs level (gain compression, THD, H2), Saturation Bias 1 and Rail 0, A12's slew on HP |

Needs ngspice (`brew install ngspice`) and numpy for the `spice/` scripts; run them from `spice/` after `build.sh`.
`spice/` imports `spice_util.py` from `tools/ms20-fidelity/spice` (`tran_compare` refuses a run that stops early).

## Results (current tree)

- Small signal (`compare_ac`): within ~0.1 dB of SPICE up to 1.5 x cutoff at every RES position up to a 4 kHz cutoff,
  LP / BP / HP, peaks within 0.1 dB, except at full RES, where the peak sits on the edge of oscillation (40-50 dB of
  peaking; up to 6 dB apart, same frequency within 0.3 %). At a 10 kHz cutoff the DSP falls faster above the peak (0.3
  dB at the cutoff at pot 0.95, 0.7 dB at 1.5 x, 2.4 dB at 30 kHz: the trapezoidal integrators near Nyquist at 4 x
  and A12's backward-Euler step, which a trapezoidal step would avoid at the cost of ringing at Nyquist).
- Large signal (`tran_compare`): gain compression within 0.03 dB and THD / H2 within 0.15 % of SPICE from 0.5 to 20 V
  in the passband and near the peak, A12 clipping included, also at Bias 1 and Rail 0; the full-RES rings at 200 Hz and
  1 kHz within ~2 % in amplitude and 0.1 Hz. A12 slewing on HP (8 / 12 kHz, 5..30 V): gain within 0.13 dB, THD 8.1 /
  10.3 % vs SPICE's 8.8 / 11.7 % at 30 V (the slewing edge resolved per sample).
- Stress: 0 of 400 failures, no unconverged sample; ~185 ns / sample at 4 x 44.1 kHz (the solve eliminates four nodes
  in closed form, then a 5 x 5; A12's input step is limited in units of its input stage's tanh argument, the nodes to
  their physical ranges, and the predictor skips a sample after one that did not converge).
