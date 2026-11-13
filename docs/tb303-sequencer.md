# TB-303 Sequencer: Behavioural Specification

How the Roland TB-303 sequencer behaves: step data, timing, gate, slide, tie and accent, and how editing changes the stored pattern. Written for implementers of TB-303 emulations and replacement sequencers (JC303, aciduino, OS-303).

This document describes observable behaviour only. It is based on:

- J. Schmidt, [Analysis of the µPD650C-133 CPU timing](https://sonic-potions.com/Documentation/Analysis_of_the_D650C-133_CPU_timing.pdf) (logic-analyzer measurements on an original CPU, 2017).
- R. Whittle, [Investigating the Slide function of the TB-303](https://www.firstpr.com.au/rwi/dfish/303-slide.html) (1999).
- Analysis of the original firmware (`d650c-133.ic8`), used to settle cases the measurements do not cover. Marked **[firmware]** below.

Timing is given in DIN sync clocks (24 per quarter note, 6 per 16th step) and, for convenience, in ticks at 96 PPQN (24 per step, 4 per clock).

---

## 1. Clock and timing

| Item | 4/4 mode | Triplet mode |
|---|---|---|
| Clocks per step | 6 | 8 |
| Gate on | clock 0 | clock 0 |
| Gate off (unheld note) | clock 3 (50 %, 12 ticks) | clock 5 (62.5 %) |

- The CPU does not react to tempo-clock edges directly. It polls them from an interrupt that runs about every **1.8 ms**, free-running and not synchronised to the tempo clock.
- Gate edges therefore land late and jitter (Schmidt):
  - gate on: about 1.0 ms after the interrupt that sees the clock edge (measured 0.8 to 3.3 ms after the clock edge, mean 2.06 ms);
  - gate off: about 2.3 to 2.4 ms after its interrupt (2.25 to 5.45 ms after the clock edge, mean 3.51 ms).
- Net effect: gates run slightly longer than 50 % (measured 49.96 % to 55.8 %), and the error beats against the tempo. Most audible at fast tempos.

## 2. Step data

Each step stores:

| Field | Values |
|---|---|
| Pitch | C, C#, D ... B, C' (13 values), or **rest** |
| Octave | down, normal, up |
| Accent | on / off |
| Hold | on / off: keep the gate high into the next step |

Pattern-level markers: a pattern shorter than 16 steps ends early. Triplet mode (8th-note triplets, 8 clocks per step) is marked in the last step slot, so a triplet pattern has at most 15 steps; 12 steps make one bar. **[firmware]**

**Hold is the only legato mechanism.** Slide and tie are both the hold flag:

- **Slide:** hold into a step with a different pitch.
- **Tie:** hold into a step with the same pitch.

A tied group of *k* steps is stored as *k* steps with the same pitch, octave and accent. Steps 1 to *k*-1 have hold set. The last step carries the note's own slide flag. **[firmware]**

Pitch output = note + octave offset + transpose (one octave per octave setting; transpose from the keyboard applies at the next pattern start).

## 3. Playback rules

### Step start (clock 0)

1. If the step is a rest: output nothing. The gate stays as it is (it is already low; see below).
2. Otherwise: output the pitch and this step's accent, then latch them.
   - If the gate is already high (the previous step held into this one), the gate stays high and the **slide line** stays high: the pitch glides to the new value with no envelope retrigger.
   - Otherwise the gate rises (new note, envelopes retrigger) and the slide line goes low.

### Gate-off decision (clock 3; clock 5 in triplet mode)

The gate stays high past this point **only if both** are true:

- this step has **hold** set, and
- the **next step plays a pitch** (it is not a rest).

Otherwise the gate falls here, exactly as for an unheld note. **[firmware]**

The next step is the one that will actually play: at the end of the pattern this is the first step of the same (or next chained) pattern, so slides and ties work across the loop point.

### Slide line

- Goes high at the start of a slid-into step. Stays high until the start of the next note that is not slid into, including through ties and through rests that follow them (Schmidt). With the gate low it has no audible effect.
- Pulses briefly whenever a pitch is latched: about 8.75 µs low during a slide, about 44 µs high at the start of an unslid note (Schmidt). Inaudible.
- The slide circuit acts during the slid-into step, from its clock 0 (Whittle: "the slide circuit is actually activated on the next note").

### Accent

- Taken from each step's own accent flag when its pitch is output, including slid-into notes and tie steps (tie steps repeat the note's accent). **[firmware]**
- The accent output is latched per step and stays active through ties and rests until the next unaccented note starts (Schmidt). It has no effect while the gate is low.

## 4. Behaviour table

Lengths are gate-high time from the first note's clock 0, in 4/4 mode.

| Sequence | Gate | Glide | Notes |
|---|---|---|---|
| Note | 3 clocks (12 ticks) | - | |
| Note (slide) → note | held into the second note, released at its clock 3 | yes | second note uses its own accent |
| Note (slide) → same pitch | as above | (no pitch change) | indistinguishable from a tie |
| Note → tie | 9 clocks (36 ticks) | - | +6 clocks per extra tie |
| Note → tie → note | released at the tie's clock 3; next note retriggers | no | |
| Note (slide) → tie → note | held through the tie into the next note | yes, at the end of the tied group | |
| Note (slide) → rest | **3 clocks (12 ticks)**, as an unslid note | - | **[firmware]** |
| Note (slide) → tie → rest | released at the tie's clock 3 (36 ticks) | - | **[firmware]** |
| Note (slide) → rest → note | released at clock 3; note after the rest retriggers | no | |
| Tie entered right after a rest | see 5.3: the rest becomes a note | - | **[firmware]** |

## 5. Editing (Pitch Mode and Time Mode)

### 5.1 Pitch list and time list are views

The machine stores only the per-step data above. The pitch list (notes with octave, accent, slide) and the time list (note / tie / rest) shown while editing are derived from it **[firmware]**:

- **Entering an edit mode:** steps are scanned in order. A step starts a new pitch-list entry unless the previous step has hold set **and** this step has the same pitch and octave; such a step is a tie. Rests add no entry.
- **Leaving an edit mode:** the pitch list is applied to the time structure: each note step takes the next pitch-list entry, and each tie step copies the note it continues.

### 5.2 Consequences

- **Re-flow:** changing the time list (e.g. turning a rest into a note) makes every later note take a different pitch-list entry. The melody shifts onto different steps. This is the familiar "edit the rhythm, get a new line" behaviour.
- **A slide into a rest is dropped.** When the pitch list is rebuilt, a note with slide followed by a rest loses its slide flag. (It has no audible effect at playback either: see the gate-off rule.) **[firmware]**
- **Same-pitch slide collapses into a tie.** After an edit pass, a slide into the same pitch is read back as a tie: the second step becomes a copy of the first note (its own accent and slide flags are replaced by the first note's), and the pitch-list entry it used is passed on to the following notes.

### 5.3 Tie after a rest

Entering a tie in Time Mode marks the **previous** step as holding and rewrites it as a note, without checking whether it was a rest. So a tie entered straight after a rest turns that rest into a note: the result is a two-step tied note starting on the former rest step, which takes the next pitch-list entry (later pitches shift by one). **[firmware]**

A tie entered on the very first step is written as a plain note. **[firmware]**

The rest-to-note rewrite looks like an unintended side effect rather than a designed feature. Implementations may reasonably keep a tie after a rest silent instead.

### 5.4 Tap write while running

When notes are recorded in real time, a step whose gate is still high at its clock 3 is recorded as a tie; otherwise as a rest. **[firmware]**

## 6. Notes for per-step sequencers

For sequencers that store accent / slide / tie per step (aciduino, JC303) at 96 PPQN:

- Normal gate: 12 ticks. Release a note after its full length; do not count it down on the tick it starts (an off-by-one here gives 11 ticks).
- Tie (rest + tie flag on the following step): +24 ticks per tied step.
- Slide: keep the gate high until after the next note starts (any overlap works; aciduino uses 32 ticks = 8 ticks past the next step). A slide carries across ties to the next played note.
- A slide followed by a rest is released like an unslid note.
- A slide into the same pitch is one gate: do not send the earlier note's release after the next note-on.
- Optional: model the interrupt timing (1.8 ms free-running grid, gate on about 1.0 ms and off about 2.35 ms after an interrupt) for the slightly long, jittery gates of the original.

## 7. Open questions

- The exact difference between the two end-of-pattern markers (one also selects triplet mode).
- Whether the slide line is purely the slide control or also the pitch-latch strobe.
- The firmware re-latches pitch and accent at clock 2 of a sounding note and appears to drop the slide line there, which would end a glide after about a third of the step. Schmidt measured the slide line high for the whole slid step, and this spec follows the measurement; a scope check of the slide line and the slide CV would settle it.
