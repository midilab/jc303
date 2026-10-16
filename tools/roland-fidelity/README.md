# Roland IR3109 filter fidelity tools

JUCE-free harnesses for the circuit-native Roland IR3109 filter (`src/dsp/open303/dfl_RolandFilter.*`: the Juno-106's
80017A, IR3109 + BA662 resonance VCA), plus an ngspice model of the circuit to check it against. Run them after any
change to the filter.

## Sources

- AS3109 datasheet (Alfa RPAR, an IR3109 clone): the stage schematic (Fig. 2), the typical low-pass wiring (Fig. 3),
  250 Hz at VC = 0 for 68k / 240 pF. https://electricdruid.net/wp-content/uploads/2025/05/AS3109.pdf
- Electric Druid on the IR3109 resonance circuits (the SH-101's and the Juno's): https://electricdruid.net/?p=97905
- Roland Juno-6 service notes (May 10, 1982): CPU board voice (IC17 IR3109, IC18 BA662) and the VCF adjustment
  (RES VR1 for a 4 V p-p oscillation). https://electricdruid.net/wp-content/uploads/2025/05/Juno-6-Service-Manual.pdf
- AMSynths, "All about the IR3109 chip": the Juno-106's 80017A pots the same IR3109 + BA662 topology as the Juno-6 / 60.
  https://amsynths.co.uk/2022/04/06/all-about-the-ir3109-chip/
- Roland HS-60 / Juno-106S service notes (Jul. 1985): module board (the RES CV, 0..+10 V from the DAC, through R107
  27k + VR26 into Tr18) and the VCF RESONANCE adjustment (VR26 for 4.8 V p-p at the VCF output, C4 key, RESO 10).
- Open80017a (T. Herpoel, tested in a Juno-106), after dksynth's reverse engineering of the 80017A:
  https://github.com/ThomHPL/Open80017a; https://obsoletetechnology.wordpress.com/projects/80017a-vcfvca-teardown/

## The Juno-106 circuit (as modelled)

The 80017A module: the Juno-6 / 60 voice's topology with dksynth's / Open80017a's values.

| Part | Value | Role |
|---|---|---|
| IR3109 | +-15 V, GND pin on ground | four stages: 68k input / local feedback, 560R shunt, 270 pF (Juno-6 / SH-101: 240 pF) |
| input | 4.7k (Juno-6: 10k) | VCF in (after C56) into stage 1: stage gain -14.5 |
| input level | 0.31 V p-p saw at 0 dB (`INPUT_VOLTS`) | 4.5 V p-p at Out4, 0.6 dB under the oscillation, the Juno-6's ratio (the 106 notes give no VCF IN level; see the header) |
| BA662 (OTA) | - input: 100k / 1.5k from Out4; + input: 24k / 1.5k from VCF in (Juno-6: 47k) | resonance VCA; output current into stage 1; its tanh limits the oscillation |
| Tr18, R107 27k, VR26 20kB | RES CV (0..+10 V) to the BA662's Iabc | VR26 trimmed for 4.8 V p-p at full RES: Iabc max = 0.233 mA (`juno_vcf.calibrate()`), VR26 ~13k |

Unverified guesses (also in `spice/juno_vcf.py`): the IR3109's PMOS follower as an ideal 1.6 V level shift with a
ceiling 0.2 V below the supply, the OTA output compliance 0.8 V from either rail, no stage offsets, an ideal Iabc (the
exponential converter is not modelled), the BA662 as an ideal OTA, the RES CV's Tr18 offset (6 % of the slider).

## VCA input and character controls

The VCA is a BA662 OTA whose tanh depends only on the input level (the envelope only scales the output current), so
the filter's output passes through it as a static stage at the circuit's level, AC-coupled as in the circuit (C59);
Open303's own VCA applies the envelope after it: VCF out through C59, VR27 (taken mid-way: the VCA GAIN trim) and R108
82k into the 80017A's 4.7k / 560R pad.

The MS-20's Bite / Clip / Sharpness / Warmth parameters drive this filter too (0 / 0.5 / 0.5 / 0 = the circuit):

| Control | Effect |
|---|---|
| Bite | stage OTA mirror mismatch (up to 10 %, alternating; slope renormalised): H2 ~1-3 % |
| Clip | the BA662's input dividers x 4^(1 - 2 clip), its Iabc inversely: oscillation 0.9 .. 6.1 V p-p, same small-signal resonance |
| Sharpness | stage OTA VT x 2^(1 - 2 s) (the cutoff holds) |
| Warmth | the VCA input driven 4^warmth harder |

Saturation Rail (`setRail`, 1 = the circuit) sags both +-15 V supplies by up to 30 %: the stage OTAs' output
compliance and the followers' ceiling follow them (Out4 swings +-8.5 V instead of +-13 V at rail 0). It is not in the
SPICE model.

Checked against SPICE with the same knobs (`tran_compare.py`'s character section): oscillation amplitude within ~1 % and
THD / H2 within ~0.1 % at every extreme. Sharpness is not in the SPICE model.

`reset()` starts from the DC operating point (the Bite offsets, the VCA's coupling capacitor charged), as SPICE does,
so there is no settling thump.

## Build

```sh
tools/roland-fidelity/build.sh      # into tools/roland-fidelity/build/ (git-ignored); needs a C++17 compiler (CXX, default clang++)
```

## Tools

| Tool | What it does |
|---|---|
| `build/probe <cutoff> <res\|pot:x> <mode> <f>...` | small-signal magnitude (dB) by sine probe, 4 x 44.1 kHz |
| `build/render <cutoff> <pot> <mode> <amp V> <freq> <secs> [kick V] [node] [bite clip warmth sharpness [bias]]` | renders in circuit volts (float32 on stdout); node: out (through the VCA input), o1..o4 |
| `build/levels [noteHz]` | saw output level per mode / pot / drive: the BP / HP trims and the drive make-up were fitted with it |
| `build/stress` | 1920 cases (character extremes, saturation bias 0 / 1, RES 0..1, every mode, drive -3..+24 dB, filter FM, 20 Hz..20 kHz cutoff sweeps, 4 x and 1 x rate): finite, bounded, rings down below the oscillation threshold; prints ns / sample. Exit 0 = pass |
| `spice/juno_vcf.py` | ngspice netlist generator (`juno_vcf.calibrate()` re-derives the RES calibration) |
| `spice/compare_ac.py` | DSP vs SPICE small-signal response, 4 cutoffs x pot positions (same Iabc, no fitting) |
| `spice/tran_compare.py` | self-oscillation (frequency, amplitude, asymmetry), distortion vs level, and the character controls at their extremes |

Needs ngspice (`brew install ngspice`) and numpy for the `spice/` scripts; run them from `spice/` after `build.sh`.
`spice/` imports `spice_util.py` from `tools/ms20-fidelity/spice`.

## Results (current tree)

- Small signal: within 0.25 dB of SPICE from 100 Hz to 10 kHz at every RES position below oscillation, peak included.
  Above ~15 kHz at a 10 kHz cutoff the DSP falls faster (up to 2.9 dB at 30 kHz: the trapezoidal integrators near
  Nyquist at 4 x).
- Self-oscillation: onset near RES 0.89 in both (the Juno oscillates only at the top of the slider, as the trim
  intends); amplitude within ~1 % (4.8 V p-p at full RES), frequency within 0.2 % (it pulls ~12 % flat at full RES).
- Distortion: within 0.01 dB / 0.01 % THD in the passband and at the peak, where the BA662's tanh squashes the
  resonance with level (-12 dB at 0.5 V near the peak at RES 0.85).

Stress: 0 of 1920 failures; ~280 ns / sample at 4 x 44.1 kHz.

## Notes

- Cutoff = the stage pole (Iabc exact, prewarped for the trapezoidal integrators). The 4-pole -3 dB point is
  0.435 x cutoff at RES 0.
- RES costs passband: the feedback is negative at DC; the Juno feeds its input into the BA662's + input too (the
  compensation), so it loses only ~7 dB at full RES (an SH-101, without it, loses ~15 dB).
- BP / HP are additions: binomial mixes of the stage outputs (BP = -(y2 + 2 y3 + y4), HP = u + 4 y1 + 6 y2 +
  4 y3 + y4, u the stage-1 input), signed so the LP -> BP -> HP crossfades have no notches.
