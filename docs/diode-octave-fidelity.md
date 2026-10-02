# Diode Octave filter: circuit fidelity notes

This documents the investigation behind the Diode Octave filter changes on branch
`fix/diode-resonance-gain-tracking` (commits `f291278` and `1e08509`): why the filter
was changed, what was measured, what was tried and rejected, and what is still open.

All numbers come from offline harnesses (the Open303 core compiled standalone, plus an
ngspice reference of the real circuit). Nothing here has been verified by ear or on a
physical TB-303. See [Not done](#not-done).

## Summary

**Current state (read this first):** the diode octave filter is retuned for parity with the TeeBee (see
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
| Filter drive knob maps to -6..+9 dB (was 0..9), default -6 dB (knob 0.0) | `JC303.cpp` | At -6 dB brightness and shriek match TeeBee; turn up for grit |
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

A Monte Carlo over 100 randomised units (resistors +-5%, film capacitors +-10%,
electrolytics +-20%, transistor IS lognormal sigma 0.2, BF lognormal sigma 0.3, matched
duals share parameters):

| Quantity | Spread |
|---|---|
| Cutoff | +-1.6 to +-2.6% |
| Resonant-peak frequency | +-0.05 of the cutoff |
| Bass (65 Hz vs 500 Hz) | +-1 dB |
| Resonant boost | standard deviation 3-7 dB at high pot positions |

Boost range (5th-95th percentile, nominal in brackets) at pot position 1.0:

| SPICE cutoff | Boost |
|---|---|
| ~500 Hz | 12.6 to 28.1 dB [20.2] |
| ~1500 Hz | 20.4 to 36.0 dB [32.3] |
| ~3000 Hz | 18.7 to 36.1 dB [32.7] |

Two real units at identical settings can differ by about 10 dB of resonant peak. This is
why the remaining 1-3 dB model differences are not worth tuning.

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
