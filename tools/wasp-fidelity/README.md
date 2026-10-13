# EDP Wasp filter fidelity tools

JUCE-free harnesses for the circuit-native EDP Wasp filter (`src/dsp/open303/dfl_WaspFilter.*`, the plugin's
"Wasabi" model; revisions EDP and Doepfer A-124), plus an ngspice model of the circuit to check it against. Run them after
any change to the filter.

## Sources

- L. Koper, M. Holters, F. Esqueda, J. D. Parker, "A Virtual Analog Model of the EDP Wasp VCF", DAFx 2022: the
  schematic (Fig. 2, the Doepfer A-124's values, Table 1), the EDP's differences (R13 100k, +5 V), the CD4069 MOSFET
  model (eq. 33-34, Table 2), the CA3080 model with its rail behaviour (eq. 35-41, Tables 3-4) and D1 / D2.
  https://www.dafx.de/paper-archive/2022/papers/DAFx20in22_paper_34.pdf
- J. Haible / M. Herrmann, "EDP Wasp VCF (+5 V powered)" clone, rev 1.1 (2006): the +5 V topology cross-check
  (33k summing network, 1N4148 limiter, 100k / 100k / 220n resonance network). http://modular.fonik.de/pdf/Wasp%20Filter%20clone.pdf

## The circuit (as modelled)

| Part | Value | Role |
|---|---|---|
| IC1 (CD4069) | R1 27k + C1 0.22u in, R3 27k feedback, R2 27k \|\| C2 100p from LP | summing amplifier; output = HP |
| OTA1 / OTA2 (CA3080) | R5 / R9 47k, R6 / R10 1k, R7 / R8 (R11 / R12) 27k to the supply | voltage-controlled integrator resistors (input x 16.3e-3) |
| IC3 / IC5 (CD4069) | C3 / C4 330p across | integrators; outputs = BP / LP |
| RES (50k lin) | R14 1k from BP, R4 27k to IC1's input, far end on C7 0.22u | BP damping, shunted out above ~40 Hz as the pot rises |
| R13 / R15, D1 / D2 | EDP 100k (A-124 1M) / 100k, 1N4148 | soft limiter on the damping |
| supply | EDP +5 V, A-124 +12 V | single rail; the OTA + inputs at half of it |

Unverified guesses: the device models are the paper's fits at +12 V (a CD4069 and CA3080 of theirs), used unchanged
at +5 V (the CA3080's rail knees placed the same distance from the rails); the CMOS protection diodes as ideal
1e-14 A junctions; the level pot at full (no source resistance); ideal OTA Iabc (the exponential converter is not
modelled); the output coupling C6 into 100k. The input level (volts at R1 for a full-scale signal at 0 dB
drive) is per revision:

- EDP 0.46 V: a 5 V p-p CMOS logic square (+-2.5 V after the input capacitor) through the input network of the EDP
  Gnat, the Wasp's sibling (same designer, oscillators and filter topology): 180k into a 33k summing node, x 0.18
  (T. Stinchcombe's Gnat schematics, sheets 6 and 8, https://www.timstinchcombe.co.uk/index.php?pge=gnat). The Wasp's
  own mixer is not documented; this is the closest drawn circuit. 0.01 % THD at 200 Hz in the passband, the dirt
  from ~1 V (+7 dB drive) up.
- A-124 5 V: Eurorack audio is 10 V p-p (+-5 V, Doepfer's A-100 technical notes), through the Level pot (50k) into
  C1 / R1 (DAFx 2022, Hin); the model has no Level pot, so it is fully up. That is ~3 % THD at 200 Hz in the
  passband on +12 V (~15 % at 1.5 kHz near a 5 kHz cutoff): Doepfer's manual says to turn Level down if the output
  distorts. A lower Level setting is a lower Filter Saturation.

## Behaviour and character controls

The CD4069s are op amps with a gain of only ~22 (+5 V) / ~13 (+12 V) once loaded, so IC1's virtual ground moves and
the loop gain falls: the resonance sits ~10 % (EDP) / ~18 % (A-124) below the nominal cutoff, and the Q at full RES
is higher than the ideal SVF's. Large signals bend early and asymmetrically (mostly H2): at full RES a 0.25 V input
already compresses the peak by 7 dB (EDP). The EDP does not self-oscillate; the A-124 just does, weakly, at low
cutoffs.

Saturation Bias moves OTA1's + input divider up by 0.15 of the supply at 1 (the DC loop then holds HP off IC1's
switching point: earlier, asymmetric clipping); Saturation Rail sags the supply by up to 30 % (Rail 0). Open303
ramps the bias in with Saturation, as on the other models.

The LP / BP / HP morph mixes the three outputs, which share IC1's inversion, so the crossfades have no notches.

## Build

```sh
tools/wasp-fidelity/build.sh      # into tools/wasp-fidelity/build/ (git-ignored); needs a C++17 compiler (CXX, default clang++)
```

## Tools

| Tool | What it does |
|---|---|
| `build/probe <edp\|a124> <cutoff> <res\|pot:x> <mode> <f>...` | small-signal magnitude (dB) by sine probe, 4 x 44.1 kHz |
| `build/render <edp\|a124> <cutoff> <pot> <mode> <amp V> <freq> <secs> [kick V] [node] [bias rail]` | renders in circuit volts (float32 on stdout); node: out, n0..n7 (the circuit nodes); `render dc <rev>` prints the DC operating point |
| `build/levels <edp\|a124> [noteHz]` | saw output level per mode / pot / drive: the BP / HP trims and the drive make-up were fitted with it |
| `build/stress` | 960 cases (both revisions, RES 0..1, every mode, drive -3..+24 dB, filter FM, bias 1, rail 0, 20 Hz..20 kHz cutoff sweeps, 4 x and 1 x rate): finite, bounded, rings down; then Newton convergence over 192 fast sweeps (drive 0..+24 dB, bias 0..1): at most 2e-6 of the samples may miss the tolerance; prints ns / sample. Exit 0 = pass |
| `spice/wasp_vcf.py` | ngspice netlist generator (the same device equations as B-sources) |
| `spice/compare_ac.py [edp\|a124]` | DSP vs SPICE small-signal response, 5 cutoffs x 6 pot positions, LP / BP / HP with their output trims removed (same Iabc, no fitting) |
| `spice/tran_compare.py [edp\|a124]` | kick rings, distortion vs level (gain compression, THD, H2), Saturation Bias 1 and Rail 0 |

Needs ngspice (`brew install ngspice`) and numpy for the `spice/` scripts; run them from `spice/` after `build.sh`.
`spice/` imports `spice_util.py` from `tools/ms20-fidelity/spice`.

## Results (current tree)

- DC operating point within 1 mV of SPICE (inverter switching points 2.21 V at +5 V, 5.60 V at +12 V; the paper's
  measured CD4069 switches near 5.7 V).
- Small signal (`compare_ac`): within 0.01 dB of SPICE up to 1.5 x cutoff at every RES position, LP / BP / HP, peaks
  within 0.07 dB (EDP). Above that at a 10 kHz cutoff the DSP falls faster (up to 1.7 dB at 30 kHz: the trapezoidal
  integrators near Nyquist at 4 x). A-124: the same, except at 400 Hz / full RES, where the peak sits on the edge of
  oscillation (55.1 dB in SPICE vs 51.2 dB, same frequency).
- Large signal (`tran_compare`): gain compression within 0.01 dB and THD / H2 within 0.01 % of SPICE in the passband
  and at the peak from 0.05 to 2 V, also at Bias 1 and Rail 0; the A-124's weak oscillation at 200 Hz within ~2 % in
  amplitude and 0.1 Hz.
- Stress: 0 of 960 failures; ~250 ns / sample (EDP) and ~300 ns / sample (A-124) at 4 x 44.1 kHz (about 2 Newton
  iterations per sample with the predictor; the solve eliminates four nodes in closed form, then a 4 x 4).
- Convergence: the solve is a damped Newton (the step scaled to at most 0.2 of the supply per node, Armijo
  backtracking on the KCL residual, the predictor kept inside the rails, a restart from the last sample at iteration
  30 when that is the better point; up to 50 iterations). Undamped, it cycled between two points near the OTA /
  inverter rail knees above ~3 kHz at +12 dB and more (fixed cutoffs too): 540549 of 10.6 M sweep samples (5 %)
  missed the tolerance; now 16 (at the input levels above), all at 1 x 44.1 kHz, none at 4 x. A +16 dB sweep costs
  ~315 ns / sample (was ~545). Where the old solve converged the output is unchanged to below -145 dB (the A-124 at
  full RES on the edge of oscillation aside, which a 1e-12 input change already moves by -50 dB).
