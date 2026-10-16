# Spicy: a circuit-faithful TB-303 diode ladder

Spicy is the diode-ladder filter in db303 (JC303). It models the TB-303 VCF from a SPICE netlist that was checked
value by value against the Roland service-notes schematic. Against that netlist, its resonant response matches
within 0.26 dB rms (worst 0.78 dB) over 11 cutoffs from 300 Hz to 20 kHz and 6 resonance-pot positions. This page
explains how it works, what is coupled to the resonance pot, and how it differs from Open303's TeeBee filter.

Much of what follows rests on Tim Stinchcombe's analysis of the TB-303 filter (see Credits): his core polynomial, his
model of the feedback coupling network and his reading of the circuit are the foundation the SPICE work builds on.

Code: `src/dsp/open303/dfl_DiodeLadderFilter.h`, `dfl_TB303OutputNetwork.h`, `rosic_Open303.h`. Tools and the
SPICE reference: `tools/diode-fidelity/`. Netlist: `docs/tb303-vcf.cir`. Development log:
`docs/diode-fidelity.md`.

## The circuit

```
             C17 1u   R62 220k
 VCO saw ----||-------/\/\/\---+---- Q12a base        (R70 2.2k to the 5.3 V bias)
                               |
          feedback in -------- Q12b base  <-- R97 10k <-- C23 1u <-- Q18 follower <-- R67 / C15 <-- pot wiper
                                                                                                      |
 Q12 input pair (tail current If from the expo converter = cutoff)                                    |
   |                                                                                                  |
 bottom rung  C18 18n   (Q12 collectors: current sources)                                             |
 rung 3       C19 33n   diode-connected pairs between rungs                                           |
 rung 2       C24 33n                                                                                 |
 top rung     C26 33n   Q22 pair above it to the bias                                                 |
   |                                                                                                  |
   +-- C27 / C25 0.1u --> Q21 diff pair --> Q19 follower --> C14 1u --> resonance pot VR 50k (lin) ---+
                                                                              |
                                                                              +--> VCA input: R121 220k + C21 10n
                                                                              |    (filter output)
                                                         pot wiper -----------+--> VCA input: R122 100k + C22 10n
                                                                                   (wiper tap)
```

One signal leaves the ladder: the differential voltage across the top rung, tapped by Q21 through C27/C25. It feeds
both the output and the resonance loop. The loop runs Q21 -> Q19 -> C14 -> the pot -> C6 / R11 -> Q18 -> C8 / R15 ->
the second input of Q12. The pot's wiper also feeds the VCA directly. That is why the pot reaches so much of the
sound.

## TeeBee and Spicy side by side

| Part | TeeBee (Open303, TB_303 mode) | Spicy |
|---|---|---|
| Origin | mystran and kunn's diode-ladder model from the KVR thread, with fitted coefficient curves | SPICE netlist of the service-notes VCF plus Stinchcombe's analysis; every constant traced to a part or a SPICE fit |
| Ladder | 4 coupled stages, all at rate 2 b0 (diode-ladder topology) | 4 coupled stages, all at the same rate (Pirkle's diode ladder) |
| Solver | Semi-implicit update with a one-sample delay in the feedback | Implicit: the loop, the coupling network and the Q21 load are solved inside each sample (Newton in Hi-Fi) |
| Cutoff mapping | b0 from a rational fit in cutoff | Stage rate = sqrt(2) x cutoff, exactly Stinchcombe's core polynomial; prewarped at the cutoff |
| Nonlinearity | None in TB_303 mode (the drive control has no effect) | Every diode pair, Q12 and Q21 as tanh at the 2Vt scale, solved together per sample (Hi-Fi) |
| Feedback network | One one-pole highpass at 150 Hz | Stinchcombe's full coupling network: 4 highpasses and a shelf, poles from 1.2 to 92 Hz |
| Resonance law | Open303's skew, k = r x polynomial(cutoff), threshold k about 17 | The pot's rotation (skew undone); loop gain K(pot, cutoff) fitted to SPICE, threshold 17.0 |
| Passband with resonance | Output gain g rises with resonance (level compensated) | Falls 16-19 dB as in the circuit; the wiper tap restores about half; Bass Comp is optional |
| Q21 loading of the ladder | Not modelled | Modelled from component values (see below) |
| Input highpass | 44.486 Hz one-pole | 0.72 Hz, the real C17 / R62 coupling |
| Output path | Fitted allpass 14.0 Hz, highpass 24.2 Hz, notch 7.5 Hz | The real chain: Q21 coupling, C14, the VCA's two-tap input, VCA and line-out coupling |
| Filter Feedback knob | Feedback highpass corner | Q21 beta: moves the C27/C25 coupling pole from 214 Hz to 46 Hz |
| Modes | Lowpass | LP -> BP -> HP morph from the ladder's taps, no notches |

TeeBee is a good empirical model: its stage structure and its threshold of about 17 match the circuit. It fits
everything around the ladder with generic filters. Spicy keeps the ladder and replaces those fits with the
circuit's actual parts.

## The octave-pole myth

A common story says the TB-303 is an "18 dB/octave" filter because its bottom capacitor is half the size of the
others (C18 = 18 nF against 33 nF), which puts that stage's pole an octave up. An earlier version of this filter was
built that way. The circuit says otherwise.

**The half-size capacitor does not move its pole.** A node's pole is roughly its conductance divided by its
capacitance:

- Each middle rung sits between two diode pairs, one above and one below, so it sees conductance 2g.
- The bottom rung sits on the Q12 collectors. Q12 is a current source, which is an open circuit at AC, so the
  bottom rung sees only the one diode pair above it: conductance g.
- So the middle rungs sit at 2g / C and the bottom rung at g / (0.545 C) = 0.92 x 2g / C.

The half-size capacitor almost exactly cancels the missing diode path, and all four poles sit at the same rate.
With equal stages, the ladder's transfer function is exactly Stinchcombe's polynomial for the real circuit:

```latex
D(x) = x^4 + 6.727 x^3 + 14.142 x^2 + 9.514 x + 1
```

Stinchcombe's polynomial (in units of his corner wc) has its threshold loop gain at 17.0. The octave-up ladder gave
18.04 and was off by up to 0.45 dB and 3 degrees. Open303's TeeBee already has equal stages, which is why its
threshold is also about 17.

**The 18 dB/octave comes from the coupling between stages.** In a diode ladder each stage loads its neighbours, so
the four poles spread far apart instead of sitting together as in a Moog ladder: 0.13, 1.04, 2.33 and 3.24 x wc. The
slope therefore steepens gradually:

| Range (x wc) | Slope |
|---|---|
| 1 -> 2x | -12.5 dB/oct |
| 2 -> 4x | -17.5 dB/oct |
| 4 -> 8x | -21.4 dB/oct |
| 8 -> 16x | -23.3 dB/oct |
| 16 -> 32x | -23.9 dB/oct |

From 2 to 4 x wc, the octaves you hear above the cutoff, it is about 18 dB/octave. It only reaches the 24 dB/octave of a 4-pole
filter far above cutoff. The low first pole, near wc / 7.8, is also why the 303's passband droops well below the
cutoff even with no resonance.

## The ladder core

The ladder follows Will Pirkle's diode-ladder analysis. In normalised form (p = s / wg, wg = the stage rate):

```
(p + 1) y1 = un + y2
(p + 1) y2 = 0.5 (y1 + y3)
(p + 1) y3 = 0.5 (y2 + y4)
(p + 1) y4 = 0.5 y3                (before the Q21 load, below)
```

From the input `un`, the top tap is `y4 = 1 / D0(p)` with `D0 = 8p^4 + 32p^3 + 40p^2 + 16p + 1`. Substituting
p = 2^(-3/4) x turns D0 into Stinchcombe's D(x) exactly.

**Cutoff convention.** The cutoff parameter is the ladder core's 180-degree frequency, 2^(1/4) x wc. That is the
frequency the resonant peak approaches at high cutoff. It makes the stage rate wg = 2^(3/4) wc = sqrt(2) x cutoff,
so `CUTOFF_TUNING` is exactly sqrt(2). In the netlist, wc is 54.0 Hz per uA of ladder current, constant to within
1 % from 4 to 400 uA. As a cross-check, a top-rung diode resistance of Vt / (If / 2) with C26 gives a stage rate
of 93.3 Hz/uA, against 2^(3/4) x 54.0 = 90.8 measured (within 3 %; the transistor models add a little series
resistance).

**Solver.** The stages are TPT one-poles solved with zero-delay feedback: Pirkle's G / S coefficients give the
ladder output as a linear function of its input within the sample, and the loop is solved for `un` exactly.

**Prewarp at the cutoff.** The bilinear transform is prewarped at the cutoff, where the resonant peak sits, rather
than at the stage rate. Prewarping at the stage rate made the peak move with the host sample rate: at 16 kHz cutoff
and full resonance it ranged from 33.0 to 41.1 dB across 176k to 768k. It is now 34.5 dB at every rate.

## Resonance and everything coupled to the pot

The resonance pot does far more in the 303 than set a feedback amount. It is part of the loop's coupling network,
part of the output mix, and it changes the filter's bass. Each of these is modelled.

### The pot itself

VR4 is a linear 50k pot. Its wiper is loaded by about 96k: C6 / R11 into the Q18 follower, R12 / R13 biasing. So the
wiper voltage is a loaded divider, not a linear function of rotation. Open303 skews its resonance parameter
(`(1 - e^(-3r)) / (1 - e^(-3))`); Spicy undoes that skew (`getResonancePot`), so the knob is the pot's rotation.

### The feedback network

Feedback goes from Q21's collector through Q19, C14, the pot, C6 / R11, Q18 and C8 / R15 into Q12's second input.
Those coupling capacitors make a network with four highpasses and a shelf. In Stinchcombe's analysis (rad/s):

```latex
F(s) = \frac{s^4 (s + 46.5)(s + 4.40)}{(s + 578.1)(s + 97.5)(s + 38.5)(s + 20.0)(s + 7.41)(s + 4.45)}
```

Spicy runs it inside the zero-delay loop solve, dropping (s + 4.40) / (s + 4.45), which is unity within 0.1 dB.
Measured against SPICE (ladder top to Q18's emitter, plus the C8 / R15 injection), it matches within 0.2 dB and
2 degrees down to 3 Hz at pot 0.5, and within 0.4 dB and 4 degrees at 20 Hz at full pot. TeeBee uses a single
150 Hz highpass instead.

The network's slowest mode, near 7 Hz, matters. A lower Q21 coupling pole strengthens it, and in SPICE it starts to
grow (motorboat) at full pot for Q21 beta 800. That is why the Filter Feedback knob stops at beta 500.

### Q21 beta and the Filter Feedback knob

The C27 / C25 coupling pole is set by Q21's input impedance, so by its beta. Measured in the netlist:

| Q21 beta | Coupling corner |
|---|---|
| 70 | 216 Hz |
| 200 | 89 Hz (Stinchcombe's 92 Hz) |
| 300 | 63 Hz (the default) |
| 500 | 45 Hz (the 2SC1583's datasheet minimum hFE) |
| 800 | 34 Hz (motorboats at full pot) |

Spicy's Filter Feedback knob runs exponentially from about 214 Hz at knob 0, through 63 Hz at the default, to 46 Hz
at knob 1. The pole moves in the feedback network and in the output path together, as it does in the circuit.

### Loop gain

The loop gain K(pot, cutoff) is solved offline so the DSP's resonant response matches SPICE. For each cutoff and pot
position, `native_solver` fits K to the ratio |H_pot| / |H_0| over 0.4 to 2.4 x cutoff, with the resonant boost
weighted 4x. The threshold KNC is a growth test at three Filter Feedback settings. It tends to 17.0 at high cutoff,
the core polynomial's value. K is capped at 0.98 of the threshold.

With the Q21 load modelled, full-pot K is nearly constant across cutoff, 16.7 to 17.0. Without it, K had to fall to
12.2 at 300 Hz to absorb the load's in-band loss.

**Res Trim** scales the loop gain by 0.85 to 1.15. That range is the 5th to 95th percentile of a component-tolerance
Monte Carlo of the netlist. A hot unit (above 1) can self-oscillate at full pot, as some real ones do.

### What the pot does to the bass and the level

- **Passband loss.** As resonance rises, the ladder's passband falls 16 to 19 dB, as in the real circuit. This is
  the well-known 303 bass thinning at high resonance.
- **The wiper tap.** The VCA's input sums two taps: R121 220k + C21 10n from the filter output, and R122 100k + C22
  10n from the pot's wiper. As the pot turns up, the wiper tap adds up to +10 dB at full pot (2.17x tap A), giving
  back about half the passband loss. Its corner, 140 to 156 Hz depending on the pot, moves the bass corner up with
  resonance. Tap A's corner is 71.6 Hz.
- **C14.** C14 couples Q19 into the pot. The corner it makes moves from 3.2 to 4.0 Hz across the pot.
- **Bass Comp** (optional, default 0 = the circuit) adds an output gain of (1 + K)^amount. That restores that
  fraction, in dB, of the resonance passband loss. TeeBee instead raises its output gain with resonance.

## Q21 loading of the ladder top

C27 / C25 into Q21's bases is a series RC load on the ladder's top rung. At low ladder current the rung's diode
resistance is high, and the load pulls the top voltage down. Against an ideally buffered top in SPICE (resonance 0):

| Cutoff | 20 Hz | 40 Hz | 80 Hz | 160 Hz | 320 Hz | 640 Hz |
|---|---|---|---|---|---|---|
| 300 Hz | -0.67 | -1.35 | -1.87 | -1.85 | -1.31 | -0.65 |
| 1 kHz | -0.14 | -0.42 | -0.75 | -0.79 | -0.67 | -0.51 |
| 5 kHz | -0.02 | -0.07 | -0.15 | -0.21 | -0.21 | -0.18 |

It is a broad dip of up to 1.9 dB, centred around 100 to 200 Hz. It vanishes below about 10 Hz, where C27 blocks the
load. The same loaded voltage feeds the output and the loop. With one K, a model without the load cut 1.8 dB less
sub-bass than the circuit at 20 Hz (300 Hz cutoff, full pot).

**The model.** In the stage equations the load adds one term to the top node:

```
(p + 1) y4 = 0.5 y3 - kappa p / (1 + p / rho) y4
```

Here kappa = C27 / (2 C26) = 1.515, since the top rung's diode resistance is 1 / (wg C26), and rho is the coupling
pole over the stage rate. Above the coupling pole that is an extra conductance kappa x rho. Below it, a one-pole
lowpass of y4 at the pole cancels it. There is no fitted parameter, and it matches SPICE's loss within 0.16 dB from
20 to 640 Hz at cutoffs from 300 Hz to 5 kHz.

In the DSP, stage 4 runs at rate g (1 + kappa rho), and the load lowpass is solved implicitly with it. rho is taken
as g_load / g, so the relations hold exactly in the discrete domain, and the K = 0 response matches the analytic
loaded model within 0.02 dB.

Two consequences:

- The BP / HP morph mixes the ladder's taps into exact p^k x lowpass numerators. The load changes the top stage, so
  the mix weights are solved again at each coefficient update (`updateTapMix`), with a correction on the load
  lowpass.
- The netlist's corner per uA must be measured on the unloaded ladder. Measured at the loaded output it reads 61 Hz/uA
  at 4 uA, falling to 54 at 400. That rise is the loading itself.

## Around the ladder

- **Input coupling.** The board's saw reaches the VCF through C17 1u into R62 220k + R70 2.2k: a 0.72 Hz corner. Open303
  puts a 44.486 Hz highpass in front of its filter, which costs 2.0 dB at 65 Hz and 8.4 dB at 20 Hz against the
  circuit. Spicy uses the 0.72 Hz coupling; TeeBee keeps its highpass.
- **Saturation.** See Nonlinearities, next.
- **Output network** (`dfl_TB303OutputNetwork.h`). From the ladder top to the jack: the Q21 coupling pole (shared with
  the loop), C14, the two-tap VCA input with the pot's wiper, and three subsonic poles for the VCA, mixer and line out
  (1.25, 4.75 and 8.25 Hz). It matches SPICE within 0.54 dB from 10 Hz to 1 kHz at 5 pot positions, and within 0.25 dB
  from 20 Hz. TeeBee uses a fitted allpass, highpass and notch here.
- **Level.** The filter runs at its own level. Matching between filter models is a separate global stage in Open303
  (`FILTER_LEVEL`): one static gain per model, set so the two match at resonance 0.

## Nonlinearities

Three parts of the TB-303 VCF saturate: the Q12 input pair, every diode pair in the ladder, and the Q21 output pair.
Spicy's Hi-Fi quality (Standard is the default) models all three and solves them together every sample. It matches SPICE's
large-signal response within 0.07 dB of compression and 0.11 points of THD (rms) at the real 303's level. TeeBee's
TB_303 mode is linear: Open303 has a cubic soft clipper (x - x^3/6, clipped at +-sqrt(2)), but that mode never calls
it, so its drive control does nothing.

### The real operating level

The VCO's saw at Q28's buffer swings from 12 V down to 5.5 V: 6.5 Vpp, or +-3.25 V. It reaches Q12 through R62 220k
into 2.2k, a 1/101 divider, so Q12 sees about +-32 mV. A differential pair's tanh argument is v / 2Vt (2Vt = 51.7 mV),
so the saw's peaks sit at 0.62: a moderate bite into the curve, not hard clipping. On Spicy's Saturation knob that is
0 dB, the default.

### Every diode is a tanh

All three nonlinearities have the same form at the same scale. A differential pair splits its tail current as
If x tanh(v / 2Vt). A diode pair carrying the ladder current does the inverse: its voltage is
Vt x ln(I_left / I_right) = 2Vt x artanh(dI / If). In the ladder's normalised units, each diode pair's current is
T(dy) = tanh(h dy) / h with h = 0.5, and the stage equations become

```
p y1 = un - T(y1 - y2)
p y2 = 0.5 (T(y1 - y2) - T(y2 - y3))
p y3 = 0.5 (T(y2 - y3) - T(y3 - y4))
p y4 = 0.5 (T(y3 - y4) - T(y4)) - (the Q21 load)        (T(y4): the Q22 pair above the top rung)
un   = T(vIn - K x feedback)                              (Q12; the feedback passes Q21's T after C27 / C25)
```

T has slope 1 at 0, so for small signals these are exactly the linear ladder's equations. The small-signal response
and the SPICE-fitted loop-gain tables do not change.

### Why one input tanh is not enough

At DC no current flows in the capacitors, so the diode pairs' artanh exactly undoes the input pair's tanh: the ladder
is translinear, and the top rung's voltage equals Q12's input voltage. As the note approaches the cutoff, the
capacitors carry more of the current and the cancellation fails. A single tanh at the input compresses every note
alike; the circuit compresses low notes less and distorts them in a different balance.

Q21 adds its own distortion on the way out. At low notes the ladder's output is large, and Q21, an undegenerated
differential pair at the same 2Vt scale, adds 1 to 1.6 points of THD at the output. (Q21 is not what limits the
level: linearising it in SPICE raises the compression slightly.)

At 1350 Hz cutoff, resonance 0, the real 303's level (compression dB / THD %):

| Note | SPICE ladder top | SPICE output | Hi-Fi | Standard |
|---|---|---|---|---|
| 55 Hz | -0.33 / 2.78 | -0.44 / 4.16 | -0.58 / 4.41 | -1.02 / 2.69 |
| 110 Hz | -0.58 / 2.54 | -0.77 / 3.68 | -0.88 / 3.95 | -1.06 / 2.57 |
| 220 Hz | -0.77 / 1.73 | -0.89 / 2.12 | -0.94 / 2.20 | -0.94 / 1.48 |
| 440 Hz | -0.83 / 0.96 | -0.87 / 0.90 | -0.89 / 0.89 | -0.83 / 0.56 |

Over the whole grid (144 points: cutoffs 500 / 1350 / 3000 Hz, pot 0 / 0.6 / 0.9, notes 55 Hz to 1.76 kHz, input
level -6 / 0 / +6 dB around the real one), errors against SPICE's output, rms:

| Quality | -6 dB | Real level | +6 dB |
|---|---|---|---|
| Hi-Fi | 0.03 dB / 0.03 pts | 0.07 dB / 0.11 pts | 0.11 dB / 0.31 pts |
| Standard | 0.07 dB / 0.12 pts | 0.20 dB / 0.43 pts | 0.45 dB / 1.52 pts |

Against SPICE's ladder top alone, without Q21, the per-stage diodes are within 0.04 dB and 0.02 points at the real
level, with nothing fitted. `build/large_signal` checks this grid (`spice/large_targets.py` regenerates it).

### How Hi-Fi solves it

The four stages, the Q12 and Q21 tanh, the coupling network and the Q21 load form one implicit system per sample.
Hi-Fi solves it with Newton's method (`stepHiFi`):

- Two fixed Newton steps, starting from a linear extrapolation of the last two solutions. This is within -90 dB of a
  fully converged solve on stress tests (tanh arguments of 50, feedback 20 % past the oscillation threshold, audio-rate
  cutoff sweeps).
- The 4x4 Jacobian is tridiagonal plus one corner (Q12's feedback from y4), solved by Cramer's rule with a single
  division.
- tanh is a rational fit, x P(x^2) / Q(x^2), within 5.8e-8 for |x| <= 9, with one division.

It costs about 8 % more than the Standard filter, and about 6 % more for the whole voice: around 2.3 % of one core at
44.1 kHz with 4x oversampling (Apple M3 Max).

The BP / HP morph mixes the ladder's taps with weights that rely on the linear ladder's relations. At high drive the
diodes' node voltages swing far beyond the linear case and those mixes stop cancelling. So Hi-Fi runs a cheap linear
ladder alongside, driven open-loop by the same Q12 current, for the morph's p to p^4 terms. The LP term stays the
Hi-Fi ladder's own output.

### Standard quality

The Filter Quality switch turns Hi-Fi off. Standard keeps the linear ladder's zero-delay solve, with Q12's tanh at the
input and Q21's tanh after the coupling capacitor. Q21 is linearised about a first-pass estimate inside the solve, so
the small-signal response stays exact. It is the cheaper fallback, and it does not follow the circuit's
note-dependent distortion.

### Controls that are not circuit behaviour

- **Saturation (drive).** 0 to +16 dB, default 0 dB (the real 303's level). It scales only the VCO signal into Q12,
  like a hotter oscillator. The feedback is unaffected, so resonance does not change with drive. Above 0 dB the gain
  grows 1.5x faster than the knob's dB value. A level makeup at the output (full below 0 dB, exponent 0.65 above) and
  fitted drive tables (`DT`, one set per quality) keep the knob from acting as a volume control. At +16 dB (6.3x the
  real level), Hi-Fi's level depends on cutoff: low cutoff with no envelope comes out about 4.6 dB quieter, and high
  cutoff with full envelope about 5 dB louder. The tables do not resolve cutoff.
- **Asymmetry.** Q12's curve runs as tanh(a + b) - tanh(b) scaled to slope 1 at rest, an input pair with an offset,
  which adds even harmonics; as a loud signal walks the pair back to its centre the slope rises to cosh^2 b, so the
  driven loop swells with level (the push of the driven filter). b ramps in with drive, from 0 at 0 dB to 0.5 at
  +16 dB, times the Saturation Bias knob (0..1, default 0.5). Near full resonance the swell carries the loop past its
  threshold and it locks onto its own frequency under the note: an inharmonic, FM-like crunch, kept on purpose (it
  reads as a "latch" in a loud-kick sweep, but in the synth the VCA closes on it; see the development log). The real
  Q12 pair is symmetric.

## Calibration against SPICE

The reference is the ngspice netlist in `tools/diode-fidelity/spice/tb303_vcf.py` (also `docs/tb303-vcf.cir`). Its
values were checked against the service-notes schematic: the ladder capacitors, the input coupling, the feedback
entry, the output stage and the VCA input. `native_targets.py` sweeps it at 11 cutoffs x 6 pot positions, and
`gen_native_tables.py` re-solves the loop-gain tables.

How the fit to SPICE improved during this work (resonance-curve rms over all 66 curves):

| Model state | rms | worst |
|---|---|---|
| Octave-up first pole, 92 Hz coupling pole, constant 52.05 Hz/uA | 0.83 dB | 2.94 dB |
| Equal stages, beta-300 default, measured corner, prewarp at cutoff | 0.38 dB | 1.34 dB |
| Plus the Q21 load and the unloaded-ladder corner | 0.26 dB | 0.78 dB |

At full pot every cutoff is within 0.03 to 0.79 dB rms. From 20 to 600 Hz, the resonance ratio at full pot is within
0.61 dB of SPICE at 300 Hz, 500 Hz and 1 kHz cutoff.

## Known limits and open questions

- **Q21 slightly strong.** At low notes Hi-Fi's output THD runs about 0.3 points above SPICE's (4.41 % against
  4.16 % at 55 Hz). The cause is not yet identified.
- **Drive above the real level.** Spicy's Saturation knob goes to +16 dB, far beyond anything the 303 does. Hi-Fi
  extrapolates the circuit's physics there, but SPICE was only checked up to +6 dB.
- **In-loop pot dependence.** In the circuit, the C14 and C6 corners in the feedback loop move a little with the pot.
  The output network tracks C14; the in-loop network does not. The measured effect is within 0.4 dB.
- **What the netlist guesses.** Generic NPN models, an ideal tail current in place of the exponential converter, and
  Q22 drawn as diodes, where the board has a common-base pair (the same at small signal). The real Q21 beta is known
  only from its datasheet.
- **The exponential converter** and the absolute cutoff-vs-CV law are not modelled.

## Credits

- **Tim Stinchcombe** worked out the TB-303 VCF in detail: the small-signal analysis of the diode ladder and its core
  polynomial (the one Spicy's ladder reproduces exactly), the full feedback coupling network F(s) with its poles and
  zeros, the role of Q21's input impedance in the dominant 92 Hz pole, and the low-frequency mode that motorboats on
  some units (he observed it near 8 Hz on a clone). Spicy's ladder tuning, its feedback network and the netlist's
  structure all come from his work, and the SPICE model was first validated by reproducing his polynomial.
- **Will Pirkle** for the diode-ladder analysis and zero-delay structure the ladder core is built on.
- **Robin Schmidt** for Open303, and **mystran** and **kunn** for the diode-ladder model behind its TeeBee filter.

## Sources

- Tim Stinchcombe, analysis of the TB-303 diode ladder filter:
  [part 1](http://www.timstinchcombe.co.uk/index.php?pge=diode),
  [part 2](http://www.timstinchcombe.co.uk/index.php?pge=diode2)
- Will Pirkle, [Modeling the Diode Ladder Filter (AN-6)](http://www.willpirkle.com/Downloads/AN-6DiodeLadderFilter.pdf)
- Roland TB-303 service notes (VCF, VCA and VCO schematic)
- Robin Schmidt, Open303 (`rosic_TeeBeeFilter.h`), and the KVR Audio forum TB-303 filter thread by mystran and kunn
  that its TB_303 mode is based on
- This repository: `docs/tb303-vcf.cir`, `tools/diode-fidelity/`, `docs/diode-fidelity.md`
