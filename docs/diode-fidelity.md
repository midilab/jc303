# Diode filter: circuit fidelity notes

> **Naming.** The filter was first called "Diode Octave", after a reading of the half-value bottom
> capacitor (C18, 18 nF against 33 nF) as an octave-up first pole. That reading was wrong: C18's node sees one
> diode pair instead of two, so all four stage poles are equal (see "Equal stage poles, measured corner, prewarp
> at cutoff" below and "The octave-pole myth" in `docs/spicy-filter.md`). The name stays below only where it
> refers to that earlier model.

This documents the investigation behind the Diode Octave filter changes on branch
`fix/diode-resonance-gain-tracking` (commits `f291278` and `1e08509`): why the filter
was changed, what was measured, what was tried and rejected, and what is still open.

All numbers come from offline harnesses (the Open303 core compiled standalone, plus an
ngspice reference of the real circuit). Nothing here has been verified by ear or on a
physical TB-303. See [Not done](#not-done).

> **Diode Octave and plain Diode have been removed.** Diode Native (calibrated to the SPICE model of the real
> circuit, see "Diode Native" below) is the only diode core and is now called just "Diode" (`FILTER_DIODE`). The
> filter model choices are TeeBee and Diode. The separate Diode BP / Diode HP types are replaced by a Filter Mode
> knob (0 = LP, 0.5 = BP, 1 = HP) that mixes the ladder taps into exact p^k numerators and morphs between them
> without notches (`TAP_MIX` and getSample in `dfl_DiodeLadderFilter.h`, checked by `tools/diode-fidelity/mode_level`).
> Bass Comp is a separate knob again. The TeeBee-parity tables (`K100T`, `KCT`, `RHOT`), their solver and the parity
> harnesses are gone, and the drive level tables were refit (LP, BP / HP). The rest of this document is the history
> of the Diode Octave work.

## Summary

**Diode Octave state at the time:** the diode octave filter is retuned for parity with the TeeBee (see
"TeeBee-parity retune" at the end). The sections on the fitted constants (ceiling 1.0, tracking exponent
0.10, knee 17.2, ceiling 17.5/18.1) and on the drive default of 4.5 or 0 dB are the history of how the
design got there and are superseded by that section.

- **Gating, amp envelope, filter envelope, accent, slide and VCA are shared by TeeBee and
  Diode Octave.** The only filter-type branch in the signal path is the one that picks the
  filter core (`rosic_Open303.h`). The audible difference between the two modes comes from
  how the two filter cores respond to the same envelope.
- The diode lacked bass, had a level that wandered with resonance, and had resonant-peak
  errors that depended on cutoff. Fixed by a low-resonance gain trim (`f291278`) and an
  in-loop feedback highpass with a refit resonance mapping (`1e08509`).
- The octave pole (first ladder stage at twice the rate) is correct as a model of the real
  circuit: it is the half-value input-end capacitor.
- A single `tanh` on the loop's differential input is the right nonlinearity. Extra
  per-stage nonlinearity was tested and not adopted.
- Resonant-peak height varies by about +-5 dB between real units from component tolerance
  alone, so remaining 1-3 dB model differences are not worth chasing.

## What changed in the code

| Change | Where | Why |
|---|---|---|
| One-pole highpass in the feedback loop (octave mode), solved exactly inside the zero-delay step; corner defaults to 115 Hz, the Feedback HPF knob is scaled by 115/150 for the diode | `dfl_DiodeLadderFilter.h/.cpp` (`feedbackHp*`), `rosic_Open303.h` | Real coupling network fits a ~110-135 Hz one-pole highpass; restores TeeBee-like bass and low-cutoff resonance |
| **TeeBee-parity feedback law** (octave mode): `K = K100(cutoff, fbHP) * rho(res, cutoff)`, capped at 0.99 of the diode's own self-oscillation threshold. Tables fitted offline so the resonant boost equals TeeBee's | `dfl_DiodeLadderFilter.h` (`calculateCoefficients`) | Replaces the earlier ceiling/knee/tracking constants; see "TeeBee-parity retune" |
| `CUTOFF_TUNING_OCTAVE` 1.33 -> 1.17 | `dfl_DiodeLadderFilter.h` | Resonant peak sat 12-22% above TeeBee's; stopband and peak frequency now align |
| Bass comp and the low-resonance trim computed from the drive-independent feedback (`Kcomp`) | `dfl_DiodeLadderFilter.h` | The bass-comp knob had a drive-dependent strength |
| Octave output trim `OCTAVE_OUTPUT_TRIM` = +4.56 dB, low-res trim `1 - 0.13*exp(-Kcomp/2.5)` | `dfl_DiodeLadderFilter.h` | Level parity with TeeBee at the new default drive |
| Filter Saturation knob (parameter id `filterDrive`, formerly Filter Drive) maps to -3..+16 dB (was -6..+9, originally 0..9), default -3 dB (knob 0.0) | `JC303.cpp` | At -3 dB brightness and shriek match TeeBee; turn up for grit. Below -3 dB the drive was pure gain |
| Drive clamped to [-12, +9] dB | `dfl_DiodeLadderFilter.cpp` (`setInputDrive`) | Stability: the zero-delay solve assumes unit gain; >= 12 dB can oscillate near 20 kHz |
| Loop-highpass state cleared on mode switch; `setFeedbackHighpass` refreshes the feedback law | `dfl_DiodeLadderFilter.h` | Avoids a stale-state step; the law depends on the corner |

Plain Diode, BP and HP modes are untouched.

## Differences found between TeeBee and Diode Octave

Ranked by audibility, from the initial investigation:

1. **Resonance-dependent passband (bass) loss.** TeeBee's output gain rises with resonance;
   the diode's static gain did not. At default settings the diode was +1.5 dB at resonance 0
   and about -2.7 dB at resonance 70 (RMS), with up to ~7 dB less bass at 65 Hz.
2. **Input `tanh` saturation (diode only).** TeeBee is linear. Intended as timbre.
3. **No feedback highpass in the diode loop.** TeeBee bleeds resonance off as cutoff falls
   into the bass; the diode kept full resonance and squelched longer.
4. Minor: effective cutoff floor (about 266 Hz vs 200 Hz), octave-mode resonance ceiling.

## The real circuit

Sources: Tim Stinchcombe's analysis (`https://www.timstinchcombe.co.uk/index.php?pge=diode`
and `?pge=diode2`) and the Roland TB-303 service-manual VCF page.

- It is a genuine four-pole, 24 dB/oct diode ladder with no buffering between stages.
  The "18 dB/oct" figure is marketing, not measured behaviour.
- **The bottom (input-end) ladder capacitor is about half the value of the three above it**
  (.018 uF vs .033 uF). Halving the capacitor doubles that stage's corner, which is exactly
  the "first pole an octave above" that TeeBee and the diode implement. Stinchcombe's
  normalised core is `H = -1 / (s^4 + 6.727 s^3 + 14.142 s^2 + 9.514 s + 1)`, all real poles.
- **Coupling capacitors around the core add about six poles and six zeroes** (his full
  transfer function; poles and zeroes are in rad/s, spanning roughly 0.7 to 92 Hz). The
  network produces a real low resonant peak (about 9 Hz, matching his measured 8 Hz
  oscillation on a TBX-303 clone). That peak sits below the 24 Hz highpass after the filter
  in Open303, so it is not modelled.
- The real feedback network matches a one-pole highpass of about **92 Hz by magnitude,
  about 112 Hz by phase at resonant frequencies, and about 135 Hz as the best joint fit**
  (20 Hz to 3 kHz). TeeBee uses 150 Hz. Phase lead at the resonant frequency is what
  matters for detuning the loop, so the diode uses 115 Hz.

## Method

### Open303 harnesses

The Open303 core is JUCE-free and compiles standalone (see the memory note
"open303 render harness"). Small-signal harnesses drive the raw filter classes with
low-level sines at 4x the base rate and measure magnitude by single-bin DFT. Full-synth
checks render a saw note through `rosic::Open303`.

### SPICE reference

`docs/tb303-vcf.cir` is a nominal ngspice netlist reconstructed from the service manual
and Stinchcombe's analysis. Validation:

- With resonance off it matches his polynomial to **0.03 dB from 300 Hz upward** (his cutoff
  scale is ~1150 Hz at a tail current of 21 uA).
- DC bias at the Q19 emitter is 5.54 V (5.59 V in transients), against the ~5.5 V centre on
  the service manual's TP6 scope trace.
- Simulated output at TP6 for a +-1.5 V square at the filter input is 0.39-0.49 Vpp and
  0.70-0.91 Vpp for +-3 V, against about 0.6 Vpp on the scope trace, which places the real
  oscillator amplitude at the filter in roughly the +-1.5 to +-3 V range.

Guesses that are **not** verified against a board: the C27/C25 tap points, the Q22
base-collector tie, generic transistor parameters, the ideal tail current in place of the
exponential converter (Q9-Q11), and the oscillator waveform shape.

Pitfalls hit while building it: SPICE node names are case-insensitive (a ladder bias node
and an input bias node both named `nb`/`nB` silently merged), and the feedback polarity
must be positive (drive Q19 from `c21a`, not `c21b`).

## Findings

### 1. Feedback highpass: in the loop, not on the output

An output-side bass shelf `y = x + K*lowpass(x)` was tried first. It matched bass in the
1-2.5 kHz cutoff range but over-corrected at low cutoff (by 4-8 dB at 65 Hz for cutoffs of
200-500 Hz) and produced sample steps 5-19x larger on resonance changes. Rejected.

A real highpass in the loop, solved exactly inside the zero-delay step (no one-sample lag),
restores the bass and the low-cutoff resonance reduction. Its cost is resonant-peak height:
the phase lead detunes a loop that sits near self-oscillation. Trade-off at corner
frequency (diode minus TeeBee, averaged over cutoffs 450-2500 Hz):

| Corner | Peak, res 60 | Peak, res 90 | Bass at 65 Hz |
|---|---|---|---|
| none | -0.3 dB | -1.0 dB | -7 dB |
| 50 Hz | -2.0 dB | -2.9 dB | -5 dB |
| 100 Hz | -3.1 dB | -4.4 dB | -2.4 dB |
| 150 Hz | -4.0 dB | -5.5 dB | -0.1 dB |

A one-sample-delay implementation was ruled out as the cause (an exact solve gave the same
peak loss). Raising the resonance ceiling alone recovered only ~1 dB, so the mapping was
refit for the loop highpass (below).

### 2. Resonance mapping

Mapping our resonance knob to the real pot: match TeeBee's resonant boost at 820 Hz (the
real circuit's pot position `a` is then solved from the SPICE netlist). With that mapping
TeeBee's boost-vs-cutoff growth already agrees with the real circuit.

Fitted constants (ceiling 1.0, tracking exponent 0.10, knee 17.2, asymptote 17.5):
RMS error 0.9 dB (worst 2.7 dB, at resonance 100 and 1.5 kHz) against the real circuit's
resonant boost over cutoffs 300 Hz-3 kHz and resonance 30-100. The asymptote stays below
the self-oscillation onset (~17.8); an asymptote of 19 self-oscillated. Both diode modes
decay to silence after gate-off at cutoffs up to 8 kHz, resonance 100, drive 9.

**Correction to the first fit:** the first fits equated our cutoff parameter with the SPICE
cutoff. The two differ by a fixed factor (about 1.32 for the diode, 1.16-1.40 for TeeBee).
With the correct scale, at resonance 100 the diode's boost is 19.8 / 29.5 / 31.2 dB at SPICE
cutoffs of 500 Hz / 1.5 kHz / 3 kHz against a real nominal of 20.2 / 32.3 / 32.7 dB: within
about 1.5 dB. At 3 kHz the diode's resonance 85 and 100 give the same boost (31.2 dB)
because the ceiling flattens the top of the knob; the real circuit still climbs there but
by less than its own unit-to-unit spread.

Against TeeBee (full-synth sweep, unaccented, cutoff 400-1500 Hz): RMS -1.7 to +0.4 dB,
peaks within -1.0 dB at resonance 70-100.

### 3. Nonlinearity

The real circuit was driven with sines at increasing amplitude (input amplitude is at the
resistor in front of the input coupling capacitor; the filter sat at ~1150 Hz):

| Drive | Passband compression | Passband THD | Resonant-peak compression |
|---|---|---|---|
| 0.3 V | -0.01 dB | 0.02% | -0.36 dB |
| 1 V | -0.09 dB | 0.26% | -2.5 dB |
| 3 V | -0.77 dB | 2.05% | -7.5 dB |

Ablations (replace a part with a linear equivalent, repeat at 3 V):

| Variant | Passband comp / THD | Peak comp |
|---|---|---|
| Real circuit | -0.76 dB / 2.05% | -7.47 dB |
| Q21 output pair linear | -0.63 / 1.60% | -9.30 dB |
| Q12 input pair linear | -0.12 / 1.36% | -3.45 dB |
| Q12 and Q21 both linear | +0.04 / 0.89% | -6.04 dB |
| Q12 as an ideal `tanh` | -0.77 / 2.07% | -7.70 dB |

- An ideal `tanh` on the input pair's differential voltage reproduces the real circuit's
  passband behaviour and most of the peak compression. Our single `tanh` on the loop's
  differential input is the correct structure.
- **Q21 is not the limiter**: making it linear increases compression.
- Ladder diodes add a smaller secondary contribution.
- A per-stage semi-implicit conductance factor was tried; it improved the peak-compression
  fit only marginally and was not adopted.
- A Chebyshev (T3) shaper was considered and rejected: it is a cubic once the amplitude
  varies, and a cubic is the lowest-order term of `tanh` anyway. `tanh` has the advantage
  of bounded output inside a feedback loop.

**Drive default.** The drive knob is shared by all diode modes (TeeBee ignores it). Under
the oscillator-level range above, the SPICE-matched drive is lower than the previous 4.5 dB
default (about -3 to -6 dB for +-2 to +-3 V; the knob minimum is 0 dB). The default is now
**0 dB** (`filterDrive` parameter default 0.0 in `JC303.cpp`). The previous 4.5 dB default
matched TeeBee to within about 1 dB in level and 1-2% in brightness; 0 dB is quieter and
brighter, so Diode Octave gets a static +0.9 dB output trim (`OCTAVE_OUTPUT_TRIM`) to keep
its previous loudness. The trim is static, so every other drive setting is also 0.9 dB louder
than before. Saved sessions should keep whatever drive value they stored (the plugin state holds the
parameter value); this was not tested.

Measured, diode octave against TeeBee (before the trim; the trim adds 0.9 dB to the level
rows):

| Drive (diode octave) | Saw note level vs TeeBee | Peak vs TeeBee | Brightness vs TeeBee |
|---|---|---|---|
| 0 dB | -2.0 dB | -1.2 dB | +11% |
| 4.5 dB | -1.1 dB | -0.8 dB | +1% |
| 9 dB | -0.2 dB | -0.5 dB | -9% |

### 4. Dynamic behaviour (cutoff envelope sweep)

A saw at 110 Hz with an instant-attack, exponentially decaying cutoff, three cases
(300 to 3000 Hz / 150 ms / resonance 50; 200 to 4000 Hz / 80 ms / resonance 70; 400 to
2500 Hz / 200 ms / resonance 30, amplitude +-1.5 V). After fitting one cutoff scale per
case the spectral-centroid-versus-time error is 0.15-0.4 dB RMS and the level-shape error
0.2-0.8 dB RMS (static level offset removed) for both TeeBee and the diode. At the default
drive (4.5 dB) the diode needs the same scale (1.32) in all three cases; TeeBee needs 1.16,
1.24 or 1.40, so the diode's sweep tracks the real shape more consistently. At 0 dB drive
the diode's scale varies too (1.24, 1.16, 1.32), so the constancy is a property of the
default setting.

### 5. Component tolerance

Re-run after the SPICE convergence fix (see "Diode Native"), with `spice/mc_native.py`: 100 randomised units
(resistors +-5%, film capacitors +-10%, electrolytics +-20%, transistor IS lognormal sigma 0.2, BF lognormal sigma 0.3,
matched duals share parameters, and the resonance pot's total resistance +-20%, which the first run held fixed).
Cutoffs use Diode Native's convention.

| Quantity | Spread (std, or 5th-95th percentile) |
|---|---|
| Cutoff (-10 dB point) | 4.2% at 500 Hz, 1.5% at 1.5 kHz, 1.2% at 5 kHz |
| Bass (65 vs 500 Hz, pot 0.9) | 1.3 dB at 500 Hz, under 0.1 dB above |
| Equivalent loop gain K (unit / nominal) | x0.85..x1.16 at 500 Hz, x0.90..x1.12 at 1.5-5 kHz |

Resonant boost, 5th-95th percentile [nominal]:

| Cutoff | Pot 0.6 | Pot 0.9 | Pot 1.0 |
|---|---|---|---|
| 500 Hz | 3.7..6.5 [5.0] | 8.2..13.1 [10.4] | 10.3..17.0 [13.2] |
| 1.5 kHz | 6.6..9.0 [7.5] | 13.5..20.1 [15.9] | 17.7..35.3 [22.3] |
| 5 kHz | 7.6..10.1 [8.7] | 15.7..25.6 [19.1] | 21.8..45.5 [31.9] |

A unit's variation is mostly a single loop-gain scale (K inferred from each unit's boost via Diode Native's
boost-vs-K curve): across units the per-unit scale has a std of 6.3%, while within one unit it varies by only 2.5%
across cutoff and pot position (97 units). It is barely correlated with the unit's cutoff scale. The extra spread at
500 Hz is the feedback coupling network's tolerance. At full pot 2-3% of units go past 0.99 x the oscillation
threshold at 1.5 and 5 kHz (none at 500 Hz), i.e. a few real units are at or near self-oscillation there.

**Res Trim** (Diode Native, Modifications menu; parameter `resTrim`, 0.5 = nominal) scales K by x0.85..x1.15, about
the 5th-95th percentile above. Above x1 the oscillation cap rises with it: at x1.15 Native self-oscillates from pot
about 0.93-0.95 at cutoffs of 2 kHz and up (never at 1 kHz and below, where the coupling network raises the
threshold); the sustained sine sits 5-22 dB below a resonance-0 saw depending on how far past the threshold it is.
The Filter Feedback knob is the complementary control: it moves the network's dominant corner, which changes
resonance mainly at low cutoff. The first run's conclusion stands: units differ by roughly 10 dB of peak at high pot positions, more than the
remaining model differences.

## Reproducing the checks

The stability sweep, stress test, parity scorers, plain-mode onset check, the offline solver for the feedback-law
tables and the SPICE scripts are checked in under [`tools/diode-fidelity/`](../tools/diode-fidelity/README.md)
with a build script, pass criteria and the expected results for the current tree. Re-run them (and regenerate the
tables with `gen_k_tables.py`) after any change to the diode filter, tuning, loop highpass or drive range.

## Not done

- No JUCE build and no listening check of the committed changes.
- No accented-note, other-sample-rate, plain-Diode, BP or HP re-checks.
- The oscillator amplitude at the filter is only bounded to about +-1.5 to +-3 V, so the
  absolute saturation scale (and the SPICE-matched drive) cannot be pinned down.
- The exponential converter (Q9-Q11) and the real cutoff-versus-control-voltage law are not
  modelled; our cutoff parameter is a fixed multiple of the SPICE cutoff, not an absolute
  frequency.
- The multi-pole low-frequency peak (~9 Hz) and the stages after the filter (Q19 output,
  VCA) are not modelled.
- The filter/amp envelope, accent and slide circuits were not compared against the real
  circuit (they are shared by both filter types).
- Netlist guesses listed above are unverified against a board.

## Drive default change: verification

Full-synth saw note, new build (drive 0 + 0.9 dB trim) against the previous build at its
4.5 dB default, cutoff 400-1500 Hz: at resonance 70-100 the RMS level is within 0.0 to
+0.1 dB (accented or not), peaks +0.2 to +0.5 dB, brightness +9 to +12%. At low resonance the
new build is a little quieter (-0.4 dB at resonance 30, -0.7 dB at resonance 0), because the
low-resonance trim was fitted at the old default. Not verified by ear.

### Interplay with Bass Comp

`bassComp` defaults to 0.1 and maps directly to the passband-compensation amount. The
compensation term is `passbandCompensation * K`, and `K` includes the drive makeup, so the
knob is not drive-invariant: at 0 dB drive `K` is about 1.3x larger than at 4.5 dB. At the
default 0.1 the +0.9 dB trim gives loudness parity with the previous build (resonance 70-100).
Away from the default the new build differs from the previous one by -1.2 dB (`bassComp` 0)
to +1.1 dB (`bassComp` 1.0), i.e. the knob's span in dB is about 10% larger. A drive-invariant
bass comp (term from `K / driveMakeup`, scaled to the 4.5 dB behaviour) was prototyped and
removes that drift at 0 dB drive, but it changes bass comp at every other drive setting
(+0.9 to +2.6 dB at 9 dB drive) and would need the output trim raised to about 1.8 dB, so it
was not adopted.

### Max cutoff, max resonance

TeeBee has a distinctive sustained "shriek" at maximum cutoff and resonance that the first
version of the diode lacked. At the same cutoff parameter as the plugin uses (drive 0, res 100):

| Cutoff | TeeBee boost | Diode (ceiling 17.5) | Diode (ceiling 18.1) | TeeBee ring to -40 dB |
|---|---|---|---|---|
| 2.4 kHz | 32.6 dB | 31.3 | 34.1 | 20 ms |
| 5 kHz | 39.4 | 34.8 | 37.1 | 26 ms |
| 8 kHz | 43.0 | 35.0 | 39.9 | 18 ms |
| 12 kHz | 46.2 | 35.0 | 44.6 | 14 ms |
| 16 kHz | 48.7 | 34.9 | 36.5 | 14 ms |
| 20 kHz | 50.3 | 33.6 | 34.9 | 14 ms |

With the ceiling at 17.5 the diode's boost plateaued at about 35 dB and resonance 85 and 100
were identical at high cutoff. The real circuit's boost at max pot is flat with cutoff
(SPICE nominal 25-33 dB from 1.5 to 15 kHz, 5th-95th percentile about 20-36 dB, some units
~41 dB), so TeeBee's climb to 50 dB is TeeBee's own character, not circuit behaviour. The
ceiling was raised to **18.1** to recover the TeeBee-like shriek; it sits at the upper edge
of the real tolerance range. With the loop highpass the diode only self-oscillates at about
18.4-18.8 (stable up to 18.4 in the scan), so the margin is roughly 0.3-0.7. A stress run of
216 combinations (four diode types, three drives, three cutoffs, three env-mod settings,
Devilfish FM on/off, resonance 100, accent 100, bass comp 1.0) stayed finite and decayed to
silence.

Full-synth check, res 100, max cutoff, env mod 60, first 80 ms: energy within +-5% of the
dominant peak is 49.7% for TeeBee, 4.4% for the diode at 17.5 and 35.5% at 18.1. Switching
the in-loop tanh off at 18.1 gives 46.9%, so the saturation costs some purity. Remaining
gaps: above about 12 kHz the diode's boost falls away (36.5 dB at 16 kHz) while TeeBee keeps
climbing, and for env-mod 80-100 the shriek sits at 12-17 kHz where the diode is weaker.

An analog-exact octave pole (first pole at exactly twice the others in the analog domain,
`g2 = 2*g`, instead of the current digital-octave tuning) was tried: it removes the collapse
above 12 kHz but gives less boost than the current pole in the 5-12 kHz range (36.0 / 36.2 /
37.5 dB against 37.1 / 39.9 / 44.6 dB, TeeBee 39.4 / 43.0 / 46.2) and overshoots at 20 kHz
(54 dB, 88 ms ring), so it was not adopted.

## TeeBee-parity retune

Goal: make Diode Octave sound like TeeBee wherever TeeBee is a sensible reference, including max
cutoff and max resonance with Filter Feedback at 1.0 (where TeeBee's "shriek" is strongest).

**Root causes found**
- `CUTOFF_TUNING_OCTAVE` (1.33) predated the loop highpass and resonance refit: the resonant peak sat
  at 1.13x the cutoff against TeeBee's 1.01x, making the diode ~10% brighter and its stopband 1-3 dB
  higher at drive 0. Scoring peak frequency and stopband shape over tunings 1.10-1.33 gave the best
  agreement at 1.17.
- The old resonance law saturated the *product* of resonance and cutoff-tracking, so at mid
  resonance and high cutoff the diode was far too resonant (+10 dB at resonance 60 and 10 kHz) while at
  resonance 100 it plateaued near 35 dB. TeeBee's own feedback grows with cutoff along a polynomial
  and sits at a roughly constant fraction of the structure's threshold.
- The diode's self-oscillation threshold depends on cutoff and on the feedback-HP corner: 28.8 at
  300 Hz, 21 at 1 kHz, about 18.4-18.5 near 8-12 kHz, 19 at 20 kHz (drive-0 basis).

**Method.** For each cutoff and each of three feedback-HP settings (default knob, knob 1.0, knob 0) the
feedback K that makes the diode's resonant boost equal TeeBee's was solved by bisection, along with the
diode's own threshold (impulse growth test). Result: K at resonance 100 rises smoothly (about 15.9 at
300 Hz to 18.7 at 20 kHz), and K at lower resonance follows `K100 * rho(res, cutoff)` where rho is close
to the resonance skew curve at low cutoff and flattens at high cutoff. The tables (11 cutoffs x 3 HP
corners, plus a 5-node rho table per cutoff) live in `calculateCoefficients`. K is capped at 0.99 of the
interpolated threshold; when drive coupling is active (drive above the +4.5 dB reference) the cap uses
the lowest-corner threshold column instead, because interpolation error plus the extra push crossed the
true threshold in testing.

**Drive and bass comp.** Bass comp (now a fixed constant, see below) and the low-resonance trim use the drive-independent feedback
`Kcomp`; before, the knob was about 40% stronger at -6 dB drive than at 0. Full-synth parity (mean over
96 settings: cutoff 400-2394 Hz, resonance 0/30/70/100, env mod 0/60/100, accent on/off) by drive, diode
minus TeeBee, with the trim set for parity at -6 dB:

| Drive | Level | Peak | Brightness |
|---|---|---|---|
| -6 dB (default) | +0.00 dB (spread 0.30) | +0.7 dB | -0.1% |
| -3 dB | +1.4 dB | +2.1 dB | -2.0% |
| 0 dB | +2.7 dB | +3.3 dB | -4.8% |

Bass comp at the new default: 0.1 is the flattest point (level offset -0.01 dB, spread 0.28 dB across
resonance and cutoff); 0 gives -3.7 dB with a 2.4 dB spread; 0.2 gives +2.3 dB, spread 1.2 dB.

**Results** (diode minus TeeBee)
- Small-signal resonant boost over cutoff 500 Hz-10 kHz, resonance 30-100, both feedback-knob positions:
  mean error 0.32 dB over 30 cells (worst 3.0 dB, resonance 100 near self-oscillation); the cell-level
  total score fell from 10.8 (before the retune) to 0.84. Resonant-peak frequency within about 5%
  above 1.5 kHz and within 1.5-6% at 500 Hz after the low-cutoff tuning boost (was 6-11% low).
- Full-synth shriek (res 100, max cutoff, first 80 ms; energy within +-5% of the dominant peak):
  env 60: 44.7% vs TeeBee 49.7% (knob 1.0: 48.4% vs 56.9%); env 80: 29.8% vs 32.9% (31.4% vs 34.3%).
  Still not matched: env 100, where the shriek sits near 17 kHz (diode 3.5% vs 41.1%).
- Stability: 8400 impulse-decay cases (7 HP corners x 4 drives x 40 cutoffs 200-20000 Hz x resonance
  0.97/0.99/1.0) all decay (worst tail -132 dB); 216-combination full-synth stress (later 288, with a fourth drive value) with Devilfish FM stays
  finite and silent after gate-off.
- Not matched by design: the real circuit's boost at max pot is flat vs cutoff (SPICE), so TeeBee's climb to
  40-50 dB at high cutoff is TeeBee-specific; the diode now follows TeeBee, not the real circuit, there.

**Caveats.** The K tables are a fit to TeeBee at the current tuning, drive-0 basis and the three
feedback-HP nodes; changing `CUTOFF_TUNING_OCTAVE`, the loop-HP structure or the ladder requires
re-solving them. Nothing here has been verified by ear or in a JUCE build.

## UI: bass comp removed, HP/BP Morph

The Bass Comp knob was an output gain that rises with resonance (volume-like), plus the HP->LP morph in
Diode HP mode. For Diode Octave, Diode and Diode BP the compensation is now a fixed constant
(`FIXED_PASSBAND_COMP` = 0.1, the value at which the diode's level tracks TeeBee's within 0.28 dB across
resonance and cutoff; 0 would give the real circuit's larger resonance-dependent level dip). The existing
`bassComp` parameter (id kept) is now labelled **HP/BP Morph** and acts in the Diode BP and Diode HP
models only: 0 = pure bandpass / highpass (default), 1 = pure lowpass, linear crossfade (BP morph is new;
HP morph already existed with the same direction). It is hidden in the amadeusp theme unless a
BP/HP model is selected; the shared menu page (midilab themes) always lists it as "HP/BP Morph".
`HP/BP Morph` is also an LFO destination (index 3): the LFO sweeps the morph from the knob value
toward lowpass (depth 1 = full sweep).

Verification: standalone harness (HP morph 1.0 gives a true highpass, 0 a lowpass-shaped response; BP
likewise; LFO->morph moves the low/high energy ratio with the LFO); full-synth stress unchanged (0
non-finite, tails below -290 dB); the edited JUCE files (`JC303.cpp`, amadeusp `Gui.cpp/.h`, `MenuPage.h`)
pass a C++20 syntax-only compile against the project's JUCE headers in both the amadeusp and midilabv2
theme configurations. No full plugin build or listening test.

## Plain Diode / BP / HP: self-oscillation, knob law and amplitude

Moving the drive default from 4.5 dB to -6 dB removed the plain Diode's self-oscillation. In these modes
`K = 17 * resonance` was not drive-compensated, so the loop gain was `17 * res * sqrt(drive gain)`: 22 * res
at the old 4.5 dB default (sustained oscillation from resonance ~50 up), 12 * res at -6 dB (never).

**Loop gain.** It is now `17 * rho(s)` at every drive (`K` is scaled by the drive makeup, so drive only changes
saturation), where `s` is the remapped resonance:

- *Knob span.* The top of the old knob all sounded alike once oscillating, so the knob spans only 0..0.8 of the
  old range (`PLAIN_KNOB_SPAN`): `s = skew(0.8 * knob)`, computed from the skewed resonance the filter receives
  as `s = (1 - (1 - c*resonance)^0.8) / c` with `c = 1 - exp(-3)` (`PLAIN_SKEW_NORM`). Knob 100 therefore behaves
  like the old knob 80 (`s` = 0.957 at full knob).
- *Law.* `rho` is linear in `s` up to `PLAIN_RHO_ONSET` (1.01) at `s` = 0.94147 (`PLAIN_ONSET_S`, the old
  resonance 75), then rises quadratically toward `PLAIN_RHO_MAX` (1.2957, the old default's full strength). With
  the knob span the top of the knob only reaches `s` = 0.957, so the loop gain at knob 100 is **1.042** times the
  self-oscillation point (`PLAIN_RHO_MAX` itself is never reached): the oscillation is real but modest.
- *Onset.* The nominal onset is at 93.75% of the knob (old 75 / 0.8); the measured onset (bisection on the knob,
  `tools/diode-fidelity/build/plain_modes`) is **86.9-92.4%** depending on cutoff, the same for plain Diode, BP and
  HP, and at -6 and +4.5 dB drive it differs by under 0.3 points except at 15 kHz (92.4 vs 89.2). It sits below the
  nominal value because the real threshold loop gain is a little under 17. The earlier version without the knob
  span (rho = 1.01 at old resonance 75) had a measured onset of 69.5-74%; a pure rescale could not give both a
  75% onset and a strong oscillation at 100 (skew(1)/skew(0.75) is only 1.06).

**Amplitude trim.** At the -6 dB default these modes were ~4 dB louder than at the old default in the linear
regime and ~4-5 dB quieter (plain: less) when self-oscillating, so no static gain fits. `plainTrimLP` and
`plainTrimBPHP` apply a fitted dB trim as a function of `rho` (nodes from the measured level difference at the
same loop gain over `rho` 0-1.2957, mean over cutoffs 400/800/1500; the range now used is `rho` <= 1.042). Result
at equal loop gain, new minus old default: RMS within +-0.8 dB for plain Diode, BP and HP; peaks within about +-3 dB
(measured up to `rho` 1.2957; the -2.9 dB / -3.1 dB plain / HP peak differences were at that unreachable maximum).
`Kcomp` for these modes is `17 * s`, so the output compensation follows the remapped knob.

An intermediate version that kept the old strength at every resonance was bit-identical to the pre-retune build
at 4.5 dB drive (resonance 0 samples identical; level, peak and centroid differences 0.00 over all settings).

## Low-cutoff tuning boost

With the single octave tuning constant (1.17) the resonant peak sat 6-11% below the TeeBee's at 300-500 Hz
(peak/cutoff 0.80 vs 0.90 at 500 Hz, resonance 30). The tuning is now `1.17 * (1 + 0.08 * w(cutoff))` where
`w` falls log-linearly from 1 at 300 Hz to 0 at 5 kHz. Scoring peak frequency and stopband shape over
strengths 0-0.20: peak-frequency error 0.17 (worst 1.0 dB) at 0, 0.03 (worst 0.5 dB) at 0.08, 0.02 at 0.12;
stopband-shape error 0.48 -> 0.60 at 0.08 but 0.90 at 0.12, so 0.08 was chosen. At 500 Hz the peak is now
-6.1% / -3.1% / -1.5% off the TeeBee's at resonance 30 / 60 / 100 (was -11% / -9% / -7.5%).

Because the diode's self-oscillation threshold at low cutoff moved with the tuning (e.g. 22.02 vs 22.29 at
500 Hz, lowest feedback-HP corner), the K100 / threshold / rho tables were re-solved at the new tuning (the
original table set had left 24 low-cutoff, drive-9 cases marginal). Note for the solver: it must override both
`K` and `Kcomp`; overriding only `K` leaves the resonance-dependent output compensation out of the boost and
inflates the solved K by ~20%. Verification: 8400-case stability sweep 0 failures (worst tail -138 dB),
stress test clean, full-synth parity unchanged (level +0.02 dB, brightness +1.0%).

## Filter Saturation: range, depth, level compensation and bias

The drive knob (parameter id `filterDrive`, shown as Filter Saturation) maps to -3..+16 dB, default -3 dB. It is not a
volume control: output level is compensated across drive and resonance. Before, the level rose +5 to +6 dB from -6 to
+9 dB at every resonance (Diode Octave; plain Diode and BP/HP differed with resonance, BP falling 4 dB at resonance 100).

**Corrected claim.** An earlier version of this work reported level flatness measured with `drive_level` at resonance of
about 0 only (the harness passed 0..1 where `setResonance` takes 0..100). With real resonance values the makeup exponents
alone left Diode Octave rising +2 to +3 dB at high resonance and BP/HP falling up to 10 dB. The tables below fix that;
`drive_level` now covers resonance 0/30/70/100.

- **Piecewise makeup.** `DRIVE_MAKEUP_EXP_CLEAN` = 1.0 below 0 dB (the shaper is nearly linear there, so drive would only
  be gain) and `DRIVE_MAKEUP_EXP_SAT` = 0.65 above. Continuous at 0 dB.
- **Saturation depth.** Above 0 dB the gain into the shaper is `knob dB * (1 + SAT_DEPTH_EXTRA)` (0.5: +16 dB on the knob
  is +24 dB into the shaper), so the top of the knob drives the `tanh` harder. Below 0 dB nothing changes, so the clean
  region and the TeeBee-parity default are untouched. Measured effect on the filter output is modest (crest factor about
  1 dB lower at 0.5 than at 0, about 1.5 dB at 1.0); `stability_sweep` is clean at 0.5 and 1.0.
- **Loop gain.** `K` is divided by `driveLoopGain = driveFactor * driveMakeup`, and the zero-delay solve includes it too
  (it assumed unit gain, exact only at or below 0 dB; above that the under-counted instantaneous feedback made the
  loop oscillate at +16 dB, 20 kHz cutoff, resonance >= 0.97).
- **Saturation bias.** The shaper is `shape(a + b) - shape(b)` divided by its slope at `b` (that normalisation latched
  Hi-Fi; it was removed, then restored by ear, and the latch kept as the crunch, see "Bias shaper latch-up" below), with
  `b = 0.5 * clamp(drive / 16, 0, 1)`: zero at and below 0 dB, so the clean region and the default are untouched. 0.5 by
  default (`SAT_BIAS_DEFAULT`; 0 for a while after the latch-up fix), exposed as the Saturation Bias parameter (0..1,
  `Open303::setFilterBias`; higher = more swell and crunch). The `DT` level tables are fitted at the default. An older
  version with its own level compensation is on the branch `backup/filter-bias-parameter`. On a saw the bias is
  inaudible at 0.3, mostly a level and resonant-peak change at 0.5 and above; no even-harmonic emphasis is measurable
  because the saw already has even partials.
- **Drive level tables (`DT`).** Output gain in dB as a function of drive (nodes -3, 0, 4.5, 9, 12.5, 16) and resonance
  (knob 0/30/70/100) for three tables: Diode Octave LP, plain Diode LP, plain BP/HP. Fitted by
  `tools/diode-fidelity/fit_drive_trim.py` (full-synth renders at the shipped bias and depth, mean over 4 cutoffs x
  3 env mods): correction = target - measured, target = flat, then +1 dB rising from 0 to +16 dB. The default (-3 dB)
  row is zero, so TeeBee parity and the plain-mode trims fitted at the default are untouched. Refit the tables (with `DT`
  zeroed) after changing the makeup, the depth, the bias, the shaper or the feedback law.
- **Stability.** The clamp in `setInputDrive` is +16 dB, the top of `stability_sweep` (0 failures, worst tail -88 dB).
- **Results** (`build/drive_level`, 48 settings per filter incl. resonance 0..100):
  Diode Octave: clean region within 0.10 dB; above 0 dB within -1.2..+2.5 dB (mean +1.0 dB at +16 dB, the fitted target).
  Plain Diode, BP and HP are much closer than before but not tight: worst case -4.8..+5.7 dB, because their level
  jumps with resonance near self-oscillation onset and four resonance nodes cannot follow it (their limits in the test
  are wide).
- **Trims re-fitted** for the new default: `OCTAVE_OUTPUT_TRIM` 1.6904 -> 1.2101 and the plain trim tables lowered by the
  measured level difference. Parity at -3 dB: level -0.01 dB (std 0.38), peak +0.69 dB, centroid -0.8%.
- **Not done.** No listening verdict beyond the author's choice of depth 0.5 and bias 0.5 from WAV renders; no preset
  migration (saved knob positions of the drive parameter changed: old -6 + 15k dB, new -3 + 19k dB). Brightness falls with
  drive (centroid about -5% at 0 dB, -11% at +4.5 dB).

### BP / HP level (merged from `bug/diode-bp-hp-level-match`)

- **Static gains.** `BP_OUTPUT_GAIN` (+9 dB) and `HP_OUTPUT_GAIN` (-3 dB) on the BP and HP terms of the BP / HP -> LP
  morph. They are targeted at a typical resonant 303 setting (resonance 50..70, cutoff 800..1500 Hz, default drive,
  morph 0): BP / HP land at -0.0 / -0.6 dB re plain LP there (range about -3..+2). There is deliberately no correction
  by cutoff: at low resonance and high cutoff BP / HP are 20+ dB below LP because a saw keeps most of its energy at
  the low harmonics they remove (that is the filter's character), and a table that boosted it would raise noise.
- **LP end of the morphs.** The output gains are applied per signal term (`lpGain`, `bandGain`, `hpGain` in
  `getSample`), so at morph 1 Diode BP and Diode HP are exactly plain Diode LP in level (`build/morph_level`: 0.00 dB
  deviation). Before, the HP morph skipped the bass compensation for the whole crossfade (its LP end was 11..12 dB
  quiet, so the morph knob doubled as a volume control) and the BP LP end used the BP/HP trims instead of the LP ones
  (up to 15 dB off at high drive and resonance). Pure LP, BP (morph 0) and HP (morph 0) are unchanged.
- **Peaks.** Worst BP / HP peaks in a grid (cutoff 100..8000, resonance 0/50/100, env 60/100, accent, drive -3/+16) are
  +6.3 / +6.7 dBFS at cutoff 100, resonance 100, accent, +16 dB drive; LP, Octave and TeeBee peak at +10..+12 dBFS at
  cutoff 8000, resonance 0, env 100 with accent (pre-existing; a global accent-peak policy, not a BP / HP issue).

## Diode Native

A sixth filter model, **Diode Native**, is the octave ladder calibrated to the SPICE model of the real VCF
(`docs/tb303-vcf.cir`) instead of to the TeeBee. Diode Octave stays as it is (TeeBee parity).

| Part | Diode Octave | Diode Native |
|---|---|---|
| Ladder core | Pirkle octave ladder | Same; it matches Stinchcombe's core polynomial within 0.4 dB / 3 degrees up to 8x the corner |
| Cutoff | `CUTOFF_TUNING_OCTAVE` 1.17 + low-cutoff boost, fitted to TeeBee's peak | `NATIVE_TUNING` 1.2003, derived: the cutoff parameter is the core's 180-degree point (1.18921 x its corner), so the max-resonance peak lands near the cutoff at high cutoff; at low cutoff the coupling network moves it, as in the circuit |
| Feedback network | One-pole highpass at 115 Hz | Stinchcombe's full coupling network (four highpasses and a shelf, poles 1.2-92 Hz), solved inside the zero-delay loop; the Filter Feedback knob scales its dominant 92 Hz pole |
| Resonance knob | Open303's skewed taper into K tables fitted to TeeBee's boost | The knob is the real pot's rotation (linear 50k, loaded); `KNT` (cutoff x pot) solved so the response matches SPICE |
| Output level | Compensation rising with resonance, fitted to TeeBee | Static gain (res-0 level = TeeBee above the bass); the output network's wiper tap makes up about half the resonance passband loss, as in the circuit |
| Output path | Open303's allpass 14 Hz, highpass 24 Hz, notch 7.5 Hz | `TB303OutputNetwork`: Q21 coupling (the feedback network's dominant pole), C14, the VCA's two-tap input (filter output and resonance-pot wiper), three subsonic VCA / mixer / line-out poles |
| Self-oscillation | No (capped at 0.99 x threshold) | No (the circuit does not; capped at 0.98 x threshold, `KNC` at three Filter Feedback nodes) |

**Calibration** (`tools/diode-fidelity/gen_native_tables.py`): `spice/native_targets.py` runs the netlist for 11
cutoffs x 6 pot positions and records the response ratio |H_pot| / |H_0| over 0.4-2.4x cutoff; `native_solver`
finds the K whose DSP response fits it (least squares in dB, boost weighted 4x). Fit: 0.83 dB rms over all curves.
Held-out check (20 cutoff / pot pairs off the table nodes): boost within 0.42 dB rms (mean +0.13, worst 1.5 dB at
400 Hz / pot 0.3, where there is no peak), peak frequency within about 3 %.

**Output network** (`src/dsp/open303/dfl_TB303OutputNetwork.h`, used by Open303 for Native only, in place of the
allpass / highpass2 / notch chain; the pre-filter highpass stays). From the ladder top to the jack, from the service
notes and `spice/tb303_vca.py`:

- C27/C25 (0.1u) into the Q21 bases. With Q21's input impedance this is Stinchcombe's dominant 578.1 rad/s (92 Hz)
  feedback-network pole: the netlist reproduces his whole F(s) within 0.6 dB (5 Hz-1 kHz) at Q21 beta 200. It sits
  in the output path too, so the network takes it from the Filter Feedback knob. The corner depends on beta (200:
  90 Hz, 300: 66, 500: 46, 800: 35, 1500: 26); the 2SC1583's hFE is at least 500 per its datasheet, so real units are
  probably below the 92 Hz default. In Native the knob's upper half (past the TeeBee 150 Hz point) therefore runs
  exponentially down to a 46 Hz pole (beta about 500) instead of 61 Hz (`Open303::setFeedbackHighpass`,
  `NATIVE_FEEDBACK_MIN`); the lower half and the other filter types are unchanged. It stops there because higher
  betas motorboat (below).
- C14 into the resonance pot, 3.2-4.0 Hz.
- The VCA input (IC15 pin 3) sums R121 220k + C21 10n from the filter output and R122 100k + C22 10n from the pot
  wiper, into 2.2k. The wiper tap's gain follows the loaded divider (50k pot, about 96k on the wiper) and reaches 2.17x
  tap A at full pot, +10 dB; its corner is 140-156 Hz, so the bass corner rises with resonance.
- VCA, mixer and line out: poles at 1.25, 4.75 and 8.25 Hz (fit to SPICE within 0.04 dB / 0.4 degrees, 2 Hz-2 kHz).

**High-beta Q21 and motorboating.** A lower coupling pole strengthens the network's ~7 Hz mode. SPICE transients
(kick, full VCF, If 70 uA) show it growing at full pot for beta 800 and from pot 0.9 for beta 1500 (6-7 Hz;
Stinchcombe saw 8 Hz on a clone); beta 500 rings but decays. Native's network matches SPICE at every beta (the
scaled F(s) is within 0.7 dB of the netlist's loop from 5 Hz up) and so does its growth threshold (pole 25 Hz:
K 13-18 depending on cutoff; SPICE beta 1500 is stable at pot 0.8, K 13.1, and grows at 0.9, K 15.3).

A 25 Hz knob end was tried and dropped. At Res Trim 1 the oscillation cap held but took the shriek with it (boost at
8 kHz 33 dB -> 13 dB, because the ~7 Hz mode's threshold falls below the full-pot K); above trim 1 the unit
motorboated. The output network hides most of that at the jack (at most about 2 dB of slow drift on a 16th-note
line), and with the VCA-input coupling bypassed it became a 7 Hz throb about 24 dB over the line that did not sound
good. At the 46 Hz end (beta 500) there is no growth at trim 1.15 at any cutoff (300 Hz-12 kHz, drive -6 / +6), the
cap stays above the full-pot K (no resonance lost at trim 1), and a hot trim self-oscillates in the audio band as
before.

The threshold table `KNC` is solved with a growth test (tiny kick, late window against an earlier one) at 57.5 /
115 / 268. The old test (-60 dB within 1.2 s) mistook the slow but stable 7 Hz ring for oscillation; at 115 and 268
the two agree within 1 %.

`build/native_output` checks it against SPICE (ladder top to jack, 5 pot positions, 10 Hz-1 kHz, levels re pot 0):
worst 0.54 dB (10 Hz, full pot), within 0.25 dB from 20 Hz.

**Level.** Full synth, saw, C2, vs TeeBee: resonance 0 is 3.9 dB quieter (the network's heavier bass cut; C4 is
within 0.6 dB). With bass comp 0 (the circuit), resonance 60-100 sits 1.4-2.7 dB below TeeBee: the ladder's passband
falls 16-19 dB with resonance as in SPICE, and the wiper tap gives about half of it back. Before the output network
the level fell 14-21 dB and ended 7-9 dB below TeeBee.

**Bass Comp** (the `bassComp` parameter, which is HP/BP Morph in Diode BP / HP; labelled "Bass Comp" in Native and
still an LFO destination): output gain `(1 + K)^amount`, which restores that fraction (in dB) of the passband loss
and is exactly 1 at resonance 0. The knob spans amount 0..0.2 (`NATIVE_BASS_COMP_MAX`; 0.5 before the output
network's wiper tap). Full synth vs TeeBee at resonance 60-100: amount 0 is 1.4-2.7 dB quieter, 0.1 (knob 0.5)
matches within 1.1 dB, 0.2 is 1.7-3.6 dB louder.

**Robustness.** Stability sweep (now Octave and Native, 16800 cases): all decay. A dense 20 x 20 check of the cap
(Filter Feedback corner x cutoff) gave a worst K / threshold of 0.9956 at 0.99 margin (between the 1 and 2 kHz
nodes at knob 1.0), hence 0.98: worst 0.9875. Stress test (450 runs incl. Native, bass comp 1.0): no failures.
The Saturation knob changes Native's level by at most +2.6 dB (Octave +1.5 dB), using Octave's drive tables.

### SPICE convergence fix

With ngspice's default tolerances the netlist's operating point is sometimes wrong (the input pair Q12 reported
more current than its tail source), and the AC boost jumped by about 3 dB between neighbouring tail currents
(22.3 -> 22.8 uA), with spikes near the oscillation edge (53.7 dB at 1.5 kHz). `tb303_vcf.py` / `tb303-vcf.cir`
now set `reltol=1e-6 abstol=1e-15 vntol=1e-9 gmin=1e-15`; removing the `.nodeset` gives the same result.

Earlier SPICE numbers in this document were computed before the fix. `boost_spice.py` before / after, at the pot
position for resonance 100: 11.3 / 9.9 dB (300 Hz), 20.2 / 14.3 (500 Hz), 21.4 / 17.7 (820 Hz), 32.3 / 23.3
(1.5 kHz), 32.7 / 28.5 (3 kHz). So the real circuit resonates less than stated in "Resonance mapping" and "Max
cutoff, max resonance" (where its max-pot boost was quoted as 25-33 dB from 1.5 to 15 kHz; native_targets.py now
gives 23-35 dB from 1.5 to 20 kHz, rising with cutoff). The Monte Carlo spread was not re-run. Diode Octave's
tables are fitted to TeeBee, not to SPICE, so its sound is unaffected.

## SPICE review fixes: input coupling and Q21 beta

A review against the netlist and the service-notes schematic found two inconsistencies around the ladder.

**Input coupling.** The diode path ran through Open303's 44.486 Hz pre-filter highpass (`highpass1`). The board has
no such corner: the saw reaches the VCF through C17 1u into R62 220k + R70 2.2k, 0.72 Hz (the netlist's C15 / R20 /
R19). Against SPICE (a=0, re 1 kHz) it cost 2.0 dB at 65 Hz, 3.9 at 40 and 8.4 at 20. Diode now has its own 0.72 Hz
input coupling (`Open303::diodeInputCoupling`); TeeBee keeps `highpass1`.

**Q21 beta.** The feedback law was fitted against the netlist at Q21 BF=300, but the DSP default coupling pole was
Stinchcombe's 92 Hz (beta about 200). The netlist's C27/C25 corner by beta (`(b21a - b21b) / (L0 - R0)`, -3 dB):
200: 89.2 Hz, 300: 63.2, 400: 52.1, 500: 44.7, 800: 34.2 (and 70: 216, 80: 192). The default corner is now beta 300
(`DiodeLadderFilter::FEEDBACK_DEFAULT` = 79, pole 63.2 Hz); the knob runs exponentially from it to the old ends
(214 Hz at knob 0, beta about 70; 46 Hz at knob 1, beta about 500). Open303's constructor now sets the knob, so the
output network's coupling pole follows the default too (it was a fixed 92 Hz until the knob moved).

`KNT` / `KNC` were re-solved at the new default (the coarse K scan in `native_solver` is now 96 steps: at 300 Hz /
pot 0.4 the error valley is narrower than the old 24-step grid and the fit had landed on K 0.43). Over the solver's
band (0.4-2.4 x cutoff) the fit is 1.10 dB rms (was 0.83 against the same curves at the 92 Hz pole); the boost is
unchanged. Below the band the new pole is much closer: ratio |H_pot| / |H_0| minus SPICE at 20 / 45 / 100 Hz

| cutoff, pot | 92 Hz pole | 63 Hz pole |
|---|---|---|
| 300 Hz, 1.0 | +2.83 / +1.85 / +0.64 | +0.87 / +0.66 / +0.78 |
| 300 Hz, 0.6 | +2.15 / +1.26 / +0.05 | +0.34 / +0.04 / +0.15 |
| 1 kHz, 1.0 | +2.74 / +1.93 / +0.76 | -0.23 / -0.09 / +0.00 |
| 2 kHz, 0.8 | +2.58 / +1.79 / +0.57 | -0.74 / -0.51 / -0.45 |

The worse in-band fit sits just below the peak at low cutoff (300 Hz cutoff, pot 1.0: +1.95 dB at 200 Hz, was
+0.58). The extra loop phase of the 92 Hz pole was likely hiding the octave-up first pole (`alpha2`; with the real
C18 the bottom pole is about 0.92x the others, not 2x).

**Level.** Full synth, saw, vs TeeBee (cutoff 400 / 1000 / 2500, env mod 60): resonance 0 at C2 is now -1.9 dB (was
-4.2), C4 +0.0 (was -0.3), so `OUTPUT_GAIN` is unchanged. Bass Comp `BASS_COMP_MAX` 0.2 -> 0.12 keeps knob 0.5 at
TeeBee level at C2 (resonance 60 / 80 / 100: -0.2 / -0.5 / +0.5 dB). `BP_GAIN` / `HP_GAIN` +0.6 dB each keep the
pure modes at LP level (mode_level: -1.3 / 0.0 / -1.2 / 0.0 dB).

## Equal stage poles, measured corner, prewarp at cutoff

**Equal stage poles.** The ladder's first stage ran an octave up (`alpha2`), read as the effect of the half-value C18.
But the bottom node sits on Q12's collectors (a current source) and sees one diode pair where the others see two, so
its pole is g / C18 = 0.92 x 2g / C: equal to the others, not an octave up. With all four stages equal the ladder is
exactly Stinchcombe's core polynomial: D0 = 8p^4 + 32p^3 + 40p^2 + 16p + 1 with p = 2^(-3/4) x gives x^4 + 6.727x^3 +
14.142x^2 + 9.514x + 1, so `CUTOFF_TUNING` = 2^(3/4) / 2^(1/4) = sqrt(2) (K = 0 response within 0.15 dB of the
polynomial, a constant offset, 0.1-8 x cutoff; the octave-up ladder was 0.45 dB / 3 degrees off). The ~18 dB/oct comes
from the coupled stages spreading the poles (0.13, 1.04, 2.33, 3.24 x wc): -12.5 dB/oct from 1-2 x cutoff, -17.5 from
2-4 x, -21.4 from 4-8 x, reaching -24 only far above. The p^4 (HP) tap mix row changes (passband 1/8, was 1/4); the
others do not.

**Measured corner.** `native_targets.py` set the ladder current from a constant 52.05 Hz/uA. Fitting Stinchcombe's
polynomial to the netlist's resonance-0 response (0.25-4 x wc, shape within 0.14 dB rms) gives 61.4 Hz/uA at 4 uA,
55.6 at 21, 54.5 at 60 and 53.6 at 400, so the targets sat 3-18 % above their nominal cutoffs and the DSP's peak
looked 3-5 % low. `If_for` now interpolates the measured table (`WC_UA`, `WC_PER_UA`).

**Prewarp at cutoff.** The bilinear transform was prewarped at the stage rate (sqrt(2) x cutoff), so the resonant
peak, which sits at the cutoff, moved with the sample rate at high cutoff. It is now prewarped at the cutoff. Full-pot
boost and peak at 176.4 / 192 / 384 / 768 kHz: identical within 0.1 dB and 0.1 % at 1, 8, 12 and 16 kHz (16 kHz was
33.0-41.1 dB before).

**Fit.** `KNT` / `KNC` re-solved: 0.38 dB rms over all 66 curves, worst 1.34 (was 0.83 / 2.94 before this review);
full pot within 0.12-0.76 dB rms at every cutoff. `KNC` tends to 17.0 at high cutoff, the core polynomial's
threshold. Below the band, ratio |H_pot| / |H_0| minus SPICE at pot 1.0: 1 kHz within 0.51 dB from 20 to 600 Hz;
300 Hz +1.78 / +0.66 / -0.11 / -0.60 dB at 20 / 45 / 100 / 200 Hz.

**Levels.** `OUTPUT_GAIN` 2.1056 -> 2.0482 (C4 at resonance 0 equals TeeBee; C2 -2.0 dB), `BP_GAIN` +18.6 dB and
`HP_GAIN` +18.8 dB (mode_level -1.3 / 0.0 / -1.2 / 0.0 dB vs LP). Bass Comp knob 0.5 at C2, resonance 60 / 80 / 100:
+0.1 / -0.2 / +0.7 dB vs TeeBee.

## Q21 loading of the ladder top

C27 / C25 into Q21's bases is a series RC load on the ladder's top rung. At low ladder current the rung's diode
resistance is high and the load pulls the top down (SPICE, ladder top vs an ideal buffer before C27 / C25, resonance 0):
up to 1.9 dB at 300 Hz cutoff, 0.8 at 1 kHz, 0.2 at 5 kHz, a broad dip around 100-200 Hz that vanishes below about
10 Hz where C27 isolates the load. The same top voltage feeds the output and the resonance loop, so with one K the DSP
cut 1.8 dB less sub-bass than SPICE at 300 Hz cutoff / pot 1.0 (20 Hz), and K had to fall with cutoff to absorb the
in-band loss.

**Model.** In the stage equations (p = s / wg) the load adds kappa p / (1 + p / rho) y4 to the top node, kappa =
C27 / (2 C26) = 1.515 (the rung's diode resistance is 1 / (wg C26)) and rho = coupling pole / wg: an extra conductance
kappa rho above the coupling pole, cancelled below it by a one-pole lowpass of y4 at the pole. It matches SPICE's loss
within 0.16 dB (20-640 Hz, 300 Hz-5 kHz cutoff) with no fitted parameter. In the DSP stage 4 runs at g (1 + kappa rho)
with input (a4 lp3 + kappa rho w) / (1 + kappa rho), solved together with the lowpass w; rho = gl / g in the discrete
domain. The ladder's K = 0 response matches the analytic loaded model within 0.02 dB.

**Corner.** The previous fit of wc per uA (61 Hz/uA at 4 uA falling to 54 at 400) was taken at the loaded output, so it
contained the loading. On the buffered (unloaded) ladder it is 54.0 Hz/uA up to 120 uA and 53.6 at 400, shape within
0.013 dB; `native_targets.py` now uses that, since the DSP models the loading itself.

**Mode taps.** The load changes the top stage, so the fixed p^k tap mixes leaked (HP only 17.6 dB down at cutoff / 8,
an 11.7 dB notch in the sweep). `updateTapMix` now writes each tap as P(p) lp4 + c w (p w = rho (lp4 - w)) and solves
the triangular system per coefficient update; at kappa rho = 0 it gives the old table. mode_level: HP 82 dB down at
cutoff / 8, worst dip 0.14 dB.

**Fit.** `KNT` / `KNC` re-solved: 0.26 dB rms over all curves, worst 0.78 (0.38 / 1.34 before). Full-pot K is now
nearly constant across cutoff (16.7-17.0; it was 12.2-16.8). Ratio |H_pot| / |H_0| minus SPICE at pot 1.0, 20-600 Hz:
300 Hz cutoff -0.06 dB at 20 Hz (was +1.78), worst +0.61 at 200 Hz; 500 Hz within 0.54; 1 kHz within 0.39.
`OUTPUT_GAIN` unchanged (the loading takes about 0.5 dB off the in-band level, as in the circuit); `BP_GAIN` / `HP_GAIN`
-0.27 / -0.46 dB keep the pure modes at LP level.

## Level matching moved out of the filter

The diode ladder no longer carries a TeeBee-matching output gain (`OUTPUT_GAIN` is gone): the filter stays at its own
level, and level matching between filter models is a global stage, `Open303::FILTER_LEVEL`, applied after the filter
chain. One static gain per model (TeeBee 1.0, Spicy 2.401 = +7.6 dB) puts the models at equal loudness at resonance 0
(saw, mean over C2 / C3 / C4 and cutoff 400 / 1000 / 2500, env mod 60; `build/model_level`). Above that Spicy is about
2.3 dB louder (resonance 50 and 100). Bass Comp defaults to 0 (the circuit); its range is unchanged.

## Large-signal fidelity: drive scale, Q21, per-stage diodes (Hi-Fi)

**Drive scale.** The VCO schematic shows the saw at Q28's buffer swinging 12 V to 5.5 V (6.5 Vpp). Through R62 220k
into 2.2k (1/101) that puts +-32 mV on Q12, a tanh argument of 0.62 at the peaks. Spicy reaches 0.62 at +0.4 dB
drive, so the Saturation default is now 0 dB (it was -3 dB, about 3.5 dB cleaner than the hardware). The drive now
scales only the VCO input into Q12; the feedback is unscaled, so the loop gain no longer depends on drive, and the
level makeup moved to the output. `DT` refitted.

**SPICE large-signal targets** (`spice/large_targets.py`, 144 points: cutoff 500 / 1350 / 3000 Hz x pot 0 / 0.6 / 0.9
x notes 55 Hz-1.76 kHz x input -6 / 0 / +6 dB around the real level; compression and THD at the ladder top and at
vout). With the old input-only tanh, the ladder top was within 0.17 dB / 0.38 pts rms at the real level but
systematically wrong in shape: low notes relative to the cutoff compress less and distort more in SPICE (1350 Hz
cutoff, 55 Hz: SPICE -0.33 dB / 2.78 %, input tanh -0.78 / 2.02). That is the translinear cancellation (the diode
pairs' artanh undoes Q12's tanh at DC). At vout, Q21 adds another 1-1.6 pts of THD at low notes: it is an
undegenerated differential pair at the same 2Vt scale. The earlier ablation that found Q21 "not the limiter" is right
about compression, but Q21 does add distortion.

**Q21.** A tanh after network section 0 (C27 / C25), in the loop and in the output. The filter now applies the
coupling itself (its output passes Q21), and `TB303OutputNetwork` skips its Q21 stage in Open303
(`setCouplingInFilter`). In the Standard path Q21 is linearised about a first-pass estimate inside the zero-delay
solve, so the small-signal response stays exact.

**Per-stage diodes (Hi-Fi, the default).** Each diode pair passes If tanh(dv / 2Vt): in the filter's units
T(x) = tanh(h x) / h, h = 0.5, the same scale as Q12. T has slope 1 at 0, so the small-signal response and `KNT` /
`KNC` are unchanged. Errors against SPICE's ladder top, rms: 0.04 dB / 0.02 pts at the real level (input tanh 0.17 /
0.38), 0.06 / 0.06 at +6 dB (0.44 / 1.05). Against vout, with Q21: 0.07 / 0.11 and 0.11 / 0.31 (input tanh + Q21:
0.20 / 0.43 and 0.45 / 1.52). Nothing was fitted.

Solver (`stepHiFi`): two fixed Newton steps from a linear extrapolation of the last two solutions, the Jacobian
(tridiagonal plus the Q12 feedback corner) by Cramer's rule, and a rational tanh (x P(x^2) / Q(x^2), 5.8e-8 on
|x| <= 9). The first prototype (full 4x4 elimination with pivoting, std::tanh, three iterations) cost about 4x the
filter; this one is +8 % on the filter and +6 % on the whole voice, and stays within -90 dB of a converged solve on
stress signals (tanh arguments to 50, K 20 % past threshold, audio-rate cutoff sweeps).

The BP / HP morph's tap mixes rely on the linear ladder's relations; at +16 dB drive the diodes' node voltages
swing far past the linear case and HP came out 60 dB too loud. Hi-Fi now runs a linear shadow ladder (Thomas
algorithm) driven open-loop by the same Q12 current for the p^1..p^4 terms, and keeps its own output for the LP term.

The Filter Quality switch (Mods menu, Spicy only) selects Standard. `DT` has one set per quality. At +16 dB Hi-Fi LP's
level spreads with cutoff (low cutoff, no env mod -4.6 dB; high cutoff, full env mod +5.0 dB) because the diodes'
compression depends on cutoff and the tables do not; `drive_level` allows that for Hi-Fi LP only. Up to +9 dB Hi-Fi
tracks Standard within 0.5 dB.

## Bias shaper latch-up (Hi-Fi)

A loud-kick stability sweep (`stability_sweep --kick 3`: a 20 ms sine burst at the cutoff instead of a 1e-3 impulse)
found Hi-Fi latching: 34 of 10800 cases kept a sustained tone after the burst (for example 3.75 kHz at -29 dB, still
there after 8 s, at cutoff 12.5 kHz). All at resonance 1.0, cutoff 5 kHz and up, drive +12 dB and up, Filter Feedback
57.5-65 Hz. The small-impulse sweep never saw it: the loop is stable about rest; this is a large-signal limit cycle.

Cause: the asymmetric shaper `shape(a + b) - shape(b)` was divided by its slope at `b`, so its slope at rest was 1 but
reached cosh^2 b once a signal walked the pair back to the tanh's centre (1.27, +2 dB, at the shipped b = 0.5;
2.38 at bias 1.0). The diode rungs supply that walk: their DC shift under the asymmetric drive moves Q12's operating
point. With the loop gain set 2 % under the threshold, +2 dB is enough to sustain an oscillation (lower than the
cutoff, as the half-saturated rungs slow the stages). A converged Newton solve (8 steps) latches the same way, linear
rungs (or Standard) do not, and bias 0 does not.

Fix: the shipped bias is 0, a symmetric Q12 as in the real circuit, and the shaper is no longer normalised.

- Removing the normalisation alone (an input pair with an offset has less gm at its operating point, never more)
  cleared the latch but cost resonance at high drive: the small-signal resonant boost at +16 dB, pot 1.0 fell from
  24.8 to 14.0 dB at 2 kHz (12.9 to 8.5 dB at 500 Hz, and 2-3 dB even at pot 0.7), and the refitted tables pushed
  self-oscillating HP (Res Trim 1.15) past the stress test's peak limit.
- The bias itself is not part of the circuit, and earlier listening found it mostly a level and resonant-peak change
  (that resonant-peak change is the over-unity gain). At bias 0 the shaper is the plain tanh, resonance is the same as
  before at every drive, and no case latched. The normalisation stays out, so a bias set from the tools cannot latch
  either.
- Nothing changes at or below 0 dB drive (the bias ramps in above it). The drive level tables `DT` were refitted for
  both qualities at bias 0 (`fit_drive_trim.py` now renders at the shipped bias): corrections within -2.3..+0.3 dB.

### Restored by ear, with a latch guard (since removed)

Listening, the symmetric Q12 lost the driven filter's push: at +16 dB the octaves above ~500 Hz came out 4-12 dB down
against the earlier build (038591e), the over-unity swell being the sound. So the normalised bias 0.5 is the default
again, and the latch is prevented instead by capping the loop gain: in Hi-Fi, `K` is held to the oscillation cap
divided by the shaper's largest slope (cosh^2 b at its centre, 1.27 at full drive), so the swell can no longer carry
the loop past its threshold. It only acts near the top of the resonance knob at high drive. Standard (linear rungs,
which never latched) keeps the full swell and is identical to 038591e.

- Sound: within about +-1 dB of 038591e per octave at +16 dB, resonance 80-90; at resonance 90-100 and full drive the
  top octaves are up to 2-4 dB lower (the cap). Unchanged at and below 0 dB.
- Stability (`stability_sweep --kick 3` and the impulse sweep, Tension -1 / 0 / +1, with and without Bite): 0 failing,
  where the uncapped normalised shaper latched in 34 cases. The Tension envelope's stage-rate cap went from 0.4 to
  0.35 fs: with the swell back, stiff Tension with Bite rang on at the top cutoffs at 0.4 fs.
- Levels: `DT` refitted at bias 0.5 (Hi-Fi's resonance-100 row moved by the cap; Standard's tables are 038591e's);
  `drive_level`'s Hi-Fi LP fall / rise limits are 5.1 / 5.8 dB (worst settings 5.0 / 5.7 dB), Standard LP's fall is
  back to 2.5 dB.

### Guard removed: the latch is the crunch

Listening again, the guarded build lost what made 038591e "crunchy, sort of like FM" at high Saturation. Measured on
held, driven resonant notes (full synth, +16 dB, resonance 90-100), 038591e put 10-25 dB more energy between the note's
harmonics than the guarded build in 14 of 64 settings (6 kHz, resonance 100, C3: -19 against -45 dB re the total).
That inharmonic energy is the over-unity loop locking onto its own frequency under the note: the same mechanism the
loud-kick sweep reports as a latch. With real notes it is never a stuck tone: the VCA closes on it (0 of 240 note
renders left a tail). So the latch is the character, and the guard is gone.

- `K` is capped by the plain oscillation cap again (`K_MARGIN * max(trim, 1) * kc`), both qualities. With Tension 0 the
  default sound is 038591e's: Standard is bit-identical, and Hi-Fi matches to rounding (about -170 dB) everywhere except
  where the loop is in the crunch, which is chaotic, so round-off grows into different waveforms with the same level
  (within 0.3 dB) and the same inharmonic energy (median difference -0.03 dB over 64 settings).
- `DT` is 038591e's tables again (`fit_drive_trim.py` residual within 0.02 dB, both qualities); `drive_level`'s Hi-Fi
  LP limits are back to 5.0 / 5.5 dB.
- Saturation Bias (0..1, default 0.5) is a plugin parameter: higher means more swell and more crunch.
- Tension's envelope reads the rungs as if at full Saturation (`tensionSens`, the +16 dB drive over the current one),
  so it is audible at any Saturation: at 0 dB the spectral centroid of a held note moves from 1320 Hz to 370 Hz
  (Tension -1) or 4367 Hz (+1), against about +-7 % with the earlier sensitivity cap of 2. It can lock into the crunch
  too. Below 0 dB drive it stays at its 0 dB value (24 dB): the full drive makeup there let stiff Tension at Res Trim
  1.15 in HP peak at 27 at -3 dB drive (73 at the filter's -12 dB floor). The stage-rate cap stays at 0.35 fs
  (Nyquist safety).
- Policy: the tools still require every sample to be finite, the peak bounded, and an idle synth to decay
  (`stress_test`'s tail after gate-off; its peak limit is 26, stiff Tension's worst case being 24.9). A loud kick that
  is still ringing 20 dB under its peak in `stability_sweep --kick` is reported as a latch, not a failure (`--strict`
  fails it). Results, 10800 cases each, 0 failing in every
  run: the impulse sweep (Hi-Fi at Tension 0 and +1, Standard) latches nowhere; `--kick 3` latches in 18 cases at
  Tension 0 (drive +14 / +16 dB, cutoff 7.8 kHz and up), 7 at Tension -1 (+16 dB, 9.8 kHz and up), 320 at Tension +1 (every drive,
  cutoff 6.9 kHz and up: the stiff rungs' raised stage rates hold the top-octave ring), and 0 in Standard.
- Choke at high bias (found by ear: full Saturation, bias above 0.85 at Tension +1, above 0.5 at Tension -1). Driven
  hard, the biased shaper rectifies; when its mean output passes what stage 1's single rung can carry (saturated, 1 / hs),
  stage 1 integrates the rest without bound, every rung saturates and the note goes silent (not a ring: the output is
  finite and the tail decays). The default bias already runs at that edge (Q12's mean reaches 0.94-1.01 of the rung's
  limit on held notes), so a DC bleed, or a rail on stage 1 (whose rung voltage is past |16| 40 % of the time at the
  default), changed the default sound; neither was kept. Floppy Tension made it worse: its envelope read the rectified
  DC as level and slowed the stages. Floppy now reads the rungs relative to Q12's tracked mean (`unMean`, 3 Hz), as it
  already did for Bite's static offset; Tension 0 and +1 are unchanged (bit-identical). On held notes (+16 dB, 96
  settings) every Tension now holds to bias 0.7 and chokes from 0.75 (floppy chokes in 0 of 96 at 0.7, was 55).

