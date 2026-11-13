# ARP 4075 filter fidelity tools

JUCE-free harnesses for the circuit-native ARP 4075 filter (`src/dsp/open303/dfl_ARP4075Filter.*`, in the Odyssey
Mk III's wiring), plus an ngspice model of the circuit with real transistors to check it against. Run them after any
change to the filter.

## Sources

- ARP 4075 Voltage Controlled Low-Pass Filter schematic, rev. E (ARP Instruments / J. Verghese, ARPtech), with the
  Odyssey Mk III VCF and controls page: https://till.com/arptech/pdf/4075.pdf (submodule index:
  https://till.com/arptech/modmain.htm). Its note 7 (R2 / R7 / R12 / R17 to 2k2) is the optional tail mod
  (`setTailMod`; the plugin runs stock); note 8 (C6) is not modelled.
- YuSynth ARP 4072 / 4075 clone (the same stage topology, drawn more legibly):
  https://yusynth.net/Modular/EN/ARPVCF/index.html

## The circuit (as modelled)

| Part | Value | Role |
|---|---|---|
| R123, C24, R35 | 1M, 0.05u, 3k3 | the Odyssey's VCO mix into pin 1 (~16 mV at the limiter for a 10 V p-p VCO) |
| Q9 / Q10, R24 | 2N5087 pair, 56k from +15 V | the "signal limiter": input and resonance into one tanh (~0.26 mA) |
| R144, R142 \|\| C37, R36 | 100k lin, 22k \|\| 220p, 3k3 | RES: the output (after C6 10u) into pin 2 |
| R3 / R4 (x4), R1 / R21 (x4) | 220R, 12k1 1 % | stage base nodes on the -7 V reference, summing the previous / own output |
| Q1..Q8 | 2N5087 pairs | stage transconductors |
| Z1A..D, C1..C4 | LM3900, 4700 pF | Norton integrators on 0 / -14 V |
| Q13 / Q14, R2 / R7 / R12 / R17 | 2N3904 / 2N3906 expo, 4k7 each (2k2 with the mod) | tail currents; Q14 saturates sharply at 1.50 mA a stage (the ~12 kHz limit), 3.2 mA with the mod |

Modelled beyond the ideal OTA cascade (each checked against the transistor-level SPICE): the limiter's base loading
(its differential input resistance, ~103k, in series with the pins' ~3k3 source resistances: -0.5 dB of gain and of
loop gain), the transistors' alpha, and the tail split: the four 4k7s share Q14's current by the stages' base
voltages, so stage 1, whose bases carry the limiter's current in their 220R (~28 mV up), gets less (54 % of the
others' at a 30 Hz cutoff, 94 % at 1 kHz; with the 2k2 mod, through less resistance, 38 % and 87 %), and each pair's
common mode modulates its tail. The split is solved by secants of the pairs' Veb, once per sample at the predicted
point (within ~1 % of SPICE's down to the 30 Hz case).

Unverified guesses (also in `spice/arp4075.py`): the LM3900 as an ideal Norton integrator (no slew or sink-current
limit, a stiff output), its swing at -1 V / -13.9 V; the -7 V reference ideal; generic 2N5087 / 2N3906 models;
the expo converter's input side not modelled (the cutoff sets Q14's emitter current); Q14's saturation knee as a
soft minimum at 7.05 V across the tail resistor (fitted to the SPICE model, stock and mod).

## Behaviour and character controls

Four one-pole stages (-12 dB at the cutoff at RES 0, 24 dB / oct), the resonance's bass loss (the passband falls with
RES, as the Moog's), self-oscillation from RES pot ~0.84 at 1 kHz (0.83 at 300 Hz, 0.85 at 4 kHz). Everything
entering the filter passes the limiter's tanh first, so the stages stay near-linear: the 4075 stays fairly clean
(1-3 % THD at the peak, mostly H2), and its oscillation is a clean ~1.8 V p-p sine.

The 2k2 tail mod (tools only; the plugin runs stock) lifts the cutoff ceiling past 20 kHz. The knob tops out at the
303's 2.4 kHz, so it would only show where the envelope peaks (env mod and accent near their maximum reach 20 kHz): ~+0.3 dB of top octave on
those sweeps, and the resonance follows them above 12 kHz. The resonance map holds with it (within 0.02 at 300 Hz
to 3 kHz).

Saturation Bias offsets the limiter pair (its resting current trimmed out: unequal current limits). Saturation Rail
sags the +15 V and -14 V supplies together by up to 60 % (the -7 V reference follows, half the -14 V). The +15 V
sets the limiter's tail (R24): its current ceiling and its gain fall together, and the pair's threshold (2 VT) does
not move, so a lower rail does not limit earlier; it makes the filter quieter (-1.4 / -3.1 / -5.3 / -8.2 dB at rail
0.75 / 0.5 / 0.25 / 0, RES 0) and less resonant (the loop gain is the limiter's: less peak and bass loss, the
oscillation onset from pot 0.84 to 0.99 at rail 0.3, none below). With less of the resonance's negative feedback,
more of the input reaches the limiter at moderate RES (knob 0.3, 200 Hz, +16 dB: THD 3.5 % at rail 1, 9.4 % at 0).
The stages' swing shrinks with their signals, and Q14's headroom with the reference (the cutoff ceiling falls from
~12 kHz to ~5 kHz at rail 0). SPICE (`tran_compare`, the same sag) agrees: -8.23 vs -8.21 dB at RES 0, -18.2 vs
-18.1 dB at pot 0.75, compression within 0.06 dB at rail 0. Open303 ramps the bias in with Saturation, as on the
other models.

The driven, biased limiter rectifies: its mean current follows the level, so the output carries a DC that moves with
each note's envelope (sine at +16 dB, bias 1, knob 0.3, cutoff 2 kHz: -0.06 at 200 Hz, -0.23 at 1.5 kHz, re ~0.6
peak), and C6 (1 s into the RES pot) passes its steps back to the limiter as an offset that settles over a second.
This is kept as the circuit's own coupling. In Open303, after its 24 Hz output highpass, a gated 303 sequence
(130 BPM sixteenths with accents, +16 dB, bias 1, cutoff 300..1500 Hz, RES 0..80 %) carries it as an even-order
sub-40 Hz part 29..36 dB under the signal (separated by rendering with the oscillator's polarity flipped), a 2..3 %
pulse over a note's first ~20 ms and nothing slower (-36..-44 dB below 8 Hz): no thump or swell, on a par with the
Wasp's (-30..-31 dB) and MS-20's (-34..-37 dB) bias.

The LP / BP / HP morph mixes the stage outputs (BP = -(y2 - 2 y3 + y4), HP = u - 4 y1 + 6 y2 - 4 y3 + y4, u the
stage-1 equivalent input), signed so the crossfades have no notches. The Odyssey's own HPF (a passive one-pole after
the VCF) is not modelled.

## Build

```sh
tools/arp-fidelity/build.sh      # into tools/arp-fidelity/build/ (git-ignored); needs a C++17 compiler (CXX, default clang++)
```

## Tools

| Tool | What it does |
|---|---|
| `build/probe <cutoff> <res\|pot:x> <mode> <f>...` | small-signal magnitude (dB) by sine probe, 4 x 44.1 kHz |
| `build/render <cutoff> <pot> <mode> <amp V> <freq> <secs> [kick V] [node] [bias rail]` | renders in circuit volts (float32 on stdout); node: out, o1..o4 (stage outputs) |
| `build/levels [noteHz]` | saw output level per mode / pot / drive: the BP / HP trims and the drive make-up were fitted with it |
| `build/stress` | 1920 cases (stock and the 2k2 mod, RES 0..1, every mode, drive -3..+24 dB, filter FM, bias 1, rail 0, 20 Hz..20 kHz cutoff sweeps, 4 x and 1 x rate): finite, bounded, rings down below the oscillation threshold; prints ns / sample. Exit 0 = pass |
| `spice/arp4075.py` | ngspice netlist generator (real 2N5087s / 2N3906, behavioural LM3900s) |
| `spice/compare_ac.py [mod]` | DSP vs SPICE small-signal response, 5 cutoffs x 6 pot positions (same tail current, no fitting; mod: the 2k2 mod in both, plus a 20 kHz cutoff) |
| `spice/onset.py [mod] [fc ...]` | self-oscillation onset (RES pot) in both, by bisection on a kick |
| `spice/tran_compare.py` | oscillation amplitude / frequency, distortion vs level (gain compression, THD, H2), Rail 0 |

`probe` and `render` take the 2k2 mod from the environment (`ARP_MOD=1`).

Needs ngspice (`brew install ngspice`) and numpy for the `spice/` scripts; run them from `spice/` after `build.sh`.
`spice/` imports `spice_util.py` from `tools/ms20-fidelity/spice`. Note: ngspice's `wrdata` repeats the scale
column per vector, so several complex vectors come out [f, re, im, f, re, im, ...].

## Results (current tree)

- Small signal (`compare_ac`): passband within 0.01 dB of SPICE (-14.54 vs -14.55 dB per volt at the mixer pot);
  within ~0.3 dB up to 2 x cutoff at every RES position below the onset, except on the flanks of near-oscillation
  peaks; peaks within 0.54 dB and 0.7 % (to 4 kHz; 1.7 % at 10 kHz). At a 10 kHz cutoff the DSP falls faster above
  2 x cutoff (2.6 dB at 30 kHz: the trapezoidal integrators near Nyquist at 4 x).
- Onset (`onset.py`): within 0.003 of SPICE at 300 Hz, 1 kHz and 4 kHz, stock and with the mod.
- The 2k2 mod (`compare_ac mod`): as stock to 10 kHz; at a 20 kHz cutoff within 0.6 dB up to 1.2 x cutoff (the
  trapezoidal integrators fall away above, near Nyquist at 4 x); peaks within 0.7 dB.
- Large signal (`tran_compare`): oscillation amplitude within 1.5 % and frequency within 1.7 % at full RES (~10 % in
  amplitude just past the onset, at pot 0.85); gain compression within 0.45 dB and THD within 0.1 % from 0.5 to 20 V
  at the mixer pot, at Rail 0 too.
- Stress: 0 of 1920 failures; ~290 ns / sample at 4 x 44.1 kHz (a 4 x 4 Newton with a predictor clipped to the
  swing, the tail split once per sample).
