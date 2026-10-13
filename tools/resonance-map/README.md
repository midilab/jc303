# Resonance map

Makes the resonance knob behave alike on the circuit-native models (MissBehave, JuneBug, Wasabi, BitterSweet, Hornet,
Oddity): the same small-signal peak height above the passband at the same knob position, and self-oscillation from
the same point. Each circuit's own pot law differs widely (raw onsets: Korg35 0.73, ARP 4075 0.84, Juno 0.89), so the
knob reaches each pot through a per-model monotone map (`src/dsp/open303/dfl_ResonanceMap.h`, applied in
`Open303::setResonance`); the filters still receive a plain pot rotation. TeeBee and Spicy are not mapped.

Targets: +6 dB at knob 0.45, +12 dB at 0.70, +20 dB at 0.82, +24 dB at 0.86, self-oscillation from 0.90; the last
tenth of the knob maps linearly onto each pot from its onset to the end, so each circuit keeps its own behaviour past
the threshold.

| tool | what |
|---|---|
| `build/measure [fc] [amp] [wasp]` | peak dB vs pot (small sine swept around the cutoff, lock-in) and the onset (kick, bisection) |
| `fit.py <measure output>` | the map's breakpoints (target curve by monotone cubic through the anchors, inverted per model) |
| `build/check [fc]` | through the map: the +6 / +12 / +20 dB crossings within 0.03 of their knob and onset in 0.88..0.92 |

    ./build.sh && ./build/measure 1000 > build/measure_1000.txt && python3 fit.py build/measure_1000.txt
    ./build/check 300 && ./build/check 1000 && ./build/check 3000

The EDP Wasp (Wasabi) never self-oscillates: it follows the targets to +20 dB at 0.82, then the rest of its pot (to
~+26 dB at the end). Its law moves with the cutoff (the CMOS inverters' loop gain: +6 dB at knob 0.30 at 300 Hz,
0.49 at 3 kHz), the circuit's own character, so its map is exact at 1 kHz only; it is probed at 1e-3 (its peak
compresses early).

Hornet (the A-124) takes the same branch: no self-oscillation at 1 kHz (it oscillates at full RES at low cutoffs:
`check` holds its "no oscillation" at 1 kHz only), ~+38 dB at the end of the pot, and the Wasp's cutoff-dependent
law; its own Q gives ~+2.5 dB at pot 0, so the knob's first sixth stays at the pot's start. Measured at 1e-3 too:
`./build/measure 1000 1e-3 hornet`.

The SEM (BitterSweet; the map measures LP, its MODE pot's start) does not self-oscillate either:
it rings at full RES (~+41 dB of small-signal peak at 1 kHz) and gives almost no peak over its pot's first 60 %; it
is probed at 1e-3 (its D11 / D12 compress the peak from a few volts of BP).

Oddity (the ARP 4075) self-oscillates (onset at pot 0.84 at 1 kHz) and takes the onset branch like the MS-20 and
IR3109 models; it has no peak at pot 0 (its four-stage cascade), and is probed at 1e-3 (its input limiter compresses
from ~10 mV). Its law barely moves with the cutoff: within 0.002 at 300 Hz and 0.018 at 3 kHz, onset 0.898..0.909;
with the 2k2 tail mod the same map holds within 0.019 (onset 0.901..0.909), so it has no map of its own.

Current tree: the MS-20 and IR3109 models within 0.008 of every knob position at 300 Hz, 1 kHz and 3 kHz, onsets
0.898..0.904; the two Wasps exact at 1 kHz, within 0.15 / 0.08 / 0.06 (+6 / +12 / +20 dB) at 300 Hz and 3 kHz. The SEM
exact at 1 kHz, within 0.008 at 300 Hz and 0.022 at 3 kHz (its law barely moves with the cutoff).
Fitted at 1 kHz with the character controls at the circuit (Bias 0, Rail 1); other Bias / Rail settings are not
re-checked.
