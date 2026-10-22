// Sequencer tests: pattern generation and gate timing, driven through the real
// AcidSequencer303 against JUCE core (Internal sync, 120 bpm, 48 kHz, 960 PPQN).
// At 120 bpm one 16th-note step is 6000 samples and one 96-PPQN tick is 250.
#include <JuceHeader.h>
#include "AcidSequencer303.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (! (cond)) {                                                      \
            std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);     \
            ++failures;                                                      \
        }                                                                    \
    } while (0)

static constexpr int kStep = 6000;
static constexpr int kTick = 250;

struct Ev { bool on; int note; int vel; long at; bool mute = false; bool hammer = false; bool reverse = false; double gate = 0.0; };

// Runs a sequencer and records the note events it emits.
struct Rig
{
    AcidSequencer303 seq;
    std::vector<Ev>  evs;
    long             pos = 0;
    int              allNotesOffCalls = 0;

    Rig()
    {
        seq.onNoteEvent = [this] (const Acid303Event& e)
        { evs.push_back ({ e.type == Acid303EventType::NoteOn, e.note, e.velocity, pos + e.sampleOffset, e.mute, e.hammer, e.reverse, e.gateSamples }); };
        seq.onAllNotesOff = [this] { ++allNotesOffCalls; };
        seq.prepare (48000.0, 960.0);
        seq.setTempo (120.0f);
        seq.clearStepData (true);
    }

    void step (int i, int note, bool accent = false, bool slide = false)
    {
        seq.setStepData (i, static_cast<uint8_t> (note));
        seq.setRest (i, false);
        seq.setAccent (i, accent);
        seq.setSlide (i, slide);
    }

    void run (long samples, int block = 512)
    {
        juce::MidiBuffer empty;
        for (long done = 0; done < samples; done += block)
        {
            seq.processBlock (empty, block, false, 0.0, 120.0);
            pos += block;
        }
    }

    std::vector<Ev> ons() const
    {
        std::vector<Ev> r;
        for (auto& e : evs) if (e.on) r.push_back (e);
        return r;
    }

    // First NoteOff for a note at or after sample `from`; -1 if none.
    long offAfter (int note, long from) const
    {
        for (auto& e : evs) if (! e.on && e.note == note && e.at >= from) return e.at;
        return -1;
    }
};

static void testStepSpacingAndGate()
{
    Rig r;
    r.step (0, 40); r.step (1, 43); r.step (2, 45); r.step (3, 47);
    r.seq.setTrackLength (4);
    r.seq.start();
    r.run (4 * kStep * 2);

    const auto ons = r.ons();
    CHECK (ons.size() >= 8);
    CHECK (ons[0].note == 40 && ons[1].note == 43 && ons[2].note == 45 && ons[3].note == 47);
    CHECK (ons[4].note == 40);                       // pattern loops
    for (size_t i = 1; i < ons.size(); ++i)
        CHECK (ons[i].at - ons[i - 1].at == kStep);  // exactly one 16th apart

    // Plain note: gate is exactly 50% of a step (12 ticks)
    const long off = r.offAfter (40, ons[0].at);
    CHECK (off == ons[0].at + 12 * kTick);
    CHECK (off < ons[1].at);                         // released before the next note
}

static void testAccentVelocity()
{
    Rig r;
    r.step (0, 40); r.step (1, 43, /*accent=*/true);
    r.seq.setTrackLength (2);
    r.seq.start();
    r.run (2 * kStep);
    const auto ons = r.ons();
    CHECK (ons[0].vel == SEQ303_NOTE_VELOCITY);
    CHECK (ons[1].vel == SEQ303_ACCENT_VELOCITY);
}

static void testSlideOverlapsNextNote()
{
    Rig r;
    r.step (0, 40, false, /*slide=*/true); r.step (1, 43);
    r.seq.setTrackLength (2);
    r.seq.start();
    r.run (2 * kStep);
    const auto ons = r.ons();
    const long off0 = r.offAfter (40, ons[0].at);
    CHECK (off0 > ons[1].at);                        // legato: still sounding when the next note starts
    CHECK (off0 < ons[1].at + kStep);                // but released within the next step
}

static void testMuteChokesGate()
{
    Rig r;
    r.step (0, 40, /*accent=*/true); r.step (1, 43);
    r.seq.setStepMute (0, true);
    r.seq.setTrackLength (2);
    r.seq.start();
    r.run (2 * kStep);
    const auto ons = r.ons();
    CHECK (! r.seq.accentOn (0));                    // mute clears accent
    CHECK (ons[0].mute && ! ons[1].mute);
    CHECK (ons[0].vel == SEQ303_NOTE_VELOCITY);
    CHECK (r.offAfter (40, ons[0].at) == ons[0].at + 6 * kTick);   // half the plain gate
}

static void testMuteKeepsSlideOverlap()
{
    Rig r;
    r.step (0, 40, false, /*slide=*/true); r.step (1, 43);
    r.seq.setStepMute (0, true);
    r.seq.setTrackLength (2);
    r.seq.start();
    r.run (2 * kStep);
    const auto ons = r.ons();
    CHECK (r.offAfter (40, ons[0].at) > ons[1].at);  // the slide extension is not choked
}

static void testHammerOverlapsAndFlagsNextNote()
{
    Rig r;
    r.step (0, 40, false, /*slide=*/true); r.step (1, 43); r.step (2, 45);
    r.seq.setHammer (0, true);
    r.seq.setTrackLength (3);
    r.seq.start();
    r.run (3 * kStep);
    const auto ons = r.ons();
    CHECK (! r.seq.slideOn (0));                     // hammer clears slide
    CHECK (r.offAfter (40, ons[0].at) > ons[1].at);  // legato like a slide
    CHECK (! ons[0].hammer && ons[1].hammer && ! ons[2].hammer);   // flag rides on the receiving note
    CHECK (r.offAfter (43, ons[1].at) < ons[2].at);  // the receiving step's own gate is plain
}

static void testHammerCarriesAcrossTie()
{
    Rig r;
    r.step (0, 40); r.step (2, 43);
    r.seq.setHammer (0, true);
    r.seq.setTie (1, true);
    r.seq.setTrackLength (3);
    r.seq.start();
    r.run (3 * kStep);
    const auto ons = r.ons();
    CHECK (ons.size() >= 2 && ons[1].note == 43 && ons[1].hammer);
    CHECK (r.offAfter (40, ons[0].at) > ons[1].at);
}

static void testRestIsSilent()
{
    Rig r;
    r.step (0, 40);                                   // steps 1..3 stay rests
    r.seq.setTrackLength (4);
    r.seq.start();
    r.run (8 * kStep);
    CHECK (r.ons().size() == 2);                     // one note per pass, none for the rests
}

static void testTieHoldsAcrossRest()
{
    Rig r;
    r.step (0, 40);
    r.seq.setTie (1, true);                          // rest + tie: step 0 keeps sounding
    r.seq.setTrackLength (4);
    r.seq.start();
    r.run (2 * kStep);
    const auto ons = r.ons();
    CHECK (ons.size() == 1);
    const long off = r.offAfter (40, ons[0].at);
    CHECK (off >= ons[0].at + kStep);                // gate spans into the tied step
}

static void testSameNoteSlideIsOneGate()
{
    Rig r;
    r.step (0, 40, false, /*slide=*/true); r.step (1, 40, /*accent=*/true);
    r.seq.setTrackLength (4);
    r.seq.start();
    r.run (2 * kStep);
    const auto ons = r.ons();
    CHECK (ons.size() == 2);
    CHECK (ons[1].vel == SEQ303_ACCENT_VELOCITY);    // the slid-into step keeps its accent

    int offs = 0;
    for (auto& e : r.evs) if (! e.on && e.note == 40) ++offs;
    CHECK (offs == 1);                               // one gate: step 0's release is dropped
    const long off = r.offAfter (40, ons[0].at);
    CHECK (off > ons[1].at + 10 * kTick);            // ...and step 1 owns the release
    CHECK (off <= ons[1].at + 12 * kTick);
}

static void testSlideCarriesAcrossTie()
{
    Rig r;
    r.step (0, 40, false, /*slide=*/true);
    r.seq.setTie (1, true);
    r.step (2, 43);
    r.seq.setTrackLength (4);
    r.seq.start();
    r.run (3 * kStep);
    const auto ons = r.ons();
    CHECK (ons.size() == 2 && ons[1].note == 43);
    const long off0 = r.offAfter (40, ons[0].at);
    CHECK (off0 > ons[1].at);                        // legato into the note after the tie
    CHECK (off0 < ons[1].at + kStep);
}

static void testPlayedStepDropsTie()
{
    AcidSequencer303 seq;
    seq.clearStepData (true);
    seq.setTie (3, true);
    seq.setRest (3, false);
    CHECK (! seq.tieOn (3));                         // as in aciduino: a played step is never tied
}

// CPU timing: events land on a free-running 1.8 ms interrupt grid, NoteOn
// ~1.0 ms and NoteOff ~2.35 ms after the interrupt (Schmidt, uPD650C-133).
static constexpr double kMs = 48.0;                  // samples per ms at 48 kHz

static void testCpuTimingDelaysGate()
{
    Rig plain, cpu;
    cpu.seq.setCpuTiming (true);
    for (Rig* r : { &plain, &cpu })
    {
        r->step (0, 40); r->step (1, 43); r->step (2, 45); r->step (3, 47);
        r->seq.setTrackLength (4);
        r->seq.start();
        r->run (8 * kStep + 1024);
    }

    CHECK (plain.evs.size() == cpu.evs.size());
    CHECK (plain.evs.size() >= 16);
    long firstOnDelay = -1;
    bool varies = false;
    for (size_t i = 0; i < plain.evs.size() && i < cpu.evs.size(); ++i)
    {
        const auto& p = plain.evs[i];
        const auto& c = cpu.evs[i];
        CHECK (p.on == c.on && p.note == c.note);    // same events, same order
        const long delay = c.at - p.at;
        if (c.on)
        {
            CHECK (delay >= 1.0 * kMs && delay <= 2.8 * kMs + 1);
            if (firstOnDelay < 0) firstOnDelay = delay;
            else if (delay != firstOnDelay) varies = true;
        }
        else
        {
            CHECK (delay >= 2.35 * kMs && delay <= 4.15 * kMs + 1);
        }
    }
    CHECK (varies);                                  // the grid beats against the tempo
}

static void testCpuTimingStopFlushesQueue()
{
    Rig r;
    r.seq.setCpuTiming (true);
    r.step (0, 40);
    r.seq.setTrackLength (4);
    r.seq.start();
    r.run (16, 16);                                  // NoteOn is queued, not yet due
    r.seq.stop();
    r.run (kStep);
    CHECK (r.ons().empty());                         // a queued NoteOn is dropped on stop
}

static std::vector<int> playedNotes (Rig& r)
{
    std::vector<int> n;
    for (auto& e : r.ons()) n.push_back (e.note);
    return n;
}

static void testPitchRotateKeepsRhythm()
{
    Rig plain, rot, back;
    for (Rig* r : { &plain, &rot, &back })
    {
        r->step (0, 40); r->step (2, 43, /*accent=*/true); r->step (3, 45);   // step 1 rests
        r->seq.setStepData (1, 50);
        r->seq.setTrackLength (4);
    }
    rot.seq.rotatePitches (1);
    back.seq.rotatePitches (-1);
    CHECK (rot.seq.getRawNote (0) == 43 && rot.seq.getRawNote (2) == 45 && rot.seq.getRawNote (3) == 40);
    CHECK (rot.seq.getRawNote (1) == 50);                // a rest keeps its stored pitch
    for (Rig* r : { &plain, &rot, &back })
    {
        r->seq.start();
        r->run (4 * kStep);
    }
    CHECK ((playedNotes (plain) == std::vector<int> { 40, 43, 45 }));
    CHECK ((playedNotes (rot)   == std::vector<int> { 43, 45, 40 }));
    CHECK ((playedNotes (back)  == std::vector<int> { 45, 40, 43 }));
    for (size_t i = 0; i < 3; ++i)
    {
        CHECK (rot.ons()[i].at == plain.ons()[i].at);    // rhythm unchanged
        CHECK (rot.ons()[i].vel == plain.ons()[i].vel);  // accent stays on its step
    }
}

static void testShiftRotatesPattern()
{
    Rig r;
    r.step (0, 40); r.step (1, 43, /*accent=*/true);
    r.step (5, 60);                                      // outside the length: untouched
    r.seq.setTrackLength (4);
    r.seq.shiftSteps (1);
    CHECK (r.seq.getRawNote (0) == 43 && r.seq.accentOn (0));
    CHECK (r.seq.getRawNote (3) == 40 && ! r.seq.accentOn (3));
    CHECK (r.seq.getRawNote (5) == 60);
    r.seq.start();
    r.run (4 * kStep);
    CHECK ((playedNotes (r) == std::vector<int> { 43, 40 }));   // starts on the old step 1
    CHECK (r.ons()[1].at - r.ons()[0].at == 3 * kStep);
    r.seq.shiftSteps (-1);
    CHECK (r.seq.getRawNote (0) == 40 && r.seq.getRawNote (1) == 43);
}

static void testShortestTurn()
{
    CHECK (AcidSequencer303::shortestTurn (-6, 7) == 1);    // menu wrap 6 -> 0 on 7 steps is one step on
    CHECK (AcidSequencer303::shortestTurn (6, 7) == -1);
    CHECK (AcidSequencer303::shortestTurn (3, 7) == 3);
    CHECK (AcidSequencer303::shortestTurn (-3, 7) == -3);
    CHECK (AcidSequencer303::shortestTurn (-15, 16) == 1);
    CHECK (AcidSequencer303::shortestTurn (8, 16) == 8);
    CHECK (AcidSequencer303::shortestTurn (5, 1) == 0);
}

static void testTransposeRewritesNotes()
{
    Rig r;
    r.step (0, 40); r.step (1, 43);
    r.seq.setStepData (2, 50);                       // a rest's stored pitch moves too
    CHECK (r.seq.transposeNotes (-5));
    CHECK (r.seq.getRawNote (0) == 35 && r.seq.getRawNote (1) == 38 && r.seq.getRawNote (2) == 45);
    r.seq.setTrackLength (4);
    r.seq.start();
    r.run (4 * kStep);
    CHECK ((playedNotes (r) == std::vector<int> { 35, 38 }));
}

static void testTransposeKeepsPatternInRange()
{
    Rig r;
    r.step (0, SEQ303_NOTE_MIN); r.step (1, 120);
    CHECK (r.seq.transposeNotes (-SEQ303_NOTE_MIN));  // below the GUI's C0 floor, down to MIDI 0
    CHECK (r.seq.getRawNote (0) == 0 && r.seq.getRawNote (1) == 120 - SEQ303_NOTE_MIN);
    CHECK (! r.seq.transposeNotes (-1));              // would pass MIDI 0
    r.seq.transposeNotes (SEQ303_NOTE_MIN);
    CHECK (! r.seq.transposeNotes (8));               // would pass MIDI 127
    CHECK (r.seq.getRawNote (0) == SEQ303_NOTE_MIN && r.seq.getRawNote (1) == 120);
}

// Swing, MPC style: the off-beat 16th lands at swing % of the 8th-note pair,
// so 75 % is 12 ticks late. Continuous, not limited to whole ticks.
static void testSwingDelaysOffbeats()
{
    Rig plain, swung;
    swung.seq.setSwingPercent (62.5f);              // 6 ticks
    for (Rig* r : { &plain, &swung })
    {
        r->step (0, 40); r->step (1, 43); r->step (2, 45); r->step (3, 47);
        r->seq.setTrackLength (4);
        r->seq.start();
        r->run (4 * kStep + 1024);
    }
    const auto p = plain.ons(), w = swung.ons();
    CHECK (p.size() == w.size() && w.size() >= 4);
    for (size_t i = 0; i < 4 && i < w.size(); ++i)
    {
        const long late = (i % 2) ? 6 * kTick : 0;
        CHECK (w[i].at - p[i].at == late);           // only off-beats move
        CHECK (w[i].note == p[i].note);
        const long gate = swung.offAfter (w[i].note, w[i].at) - w[i].at;
        CHECK (gate == 12 * kTick);                  // plain notes keep their 12 ticks
    }
}

static void testSwingIsContinuous()
{
    Rig r;
    r.seq.setSwingPercent (60.0f);                   // 4.8 ticks = 1200 samples
    r.step (0, 40); r.step (1, 43);
    r.seq.setTrackLength (2);
    r.seq.start();
    r.run (2 * kStep - 512);
    const auto ons = r.ons();
    CHECK (ons.size() == 2);
    if (ons.size() == 2)
    {
        CHECK (ons[1].at - ons[0].at == kStep + 1200);
        CHECK (r.offAfter (43, ons[1].at) - ons[1].at == 12 * kTick);
    }
}

static void testSwingKeepsSlideLegato()
{
    for (float pct : { 66.0f, 75.0f })
        for (int from : { 0, 1 })                    // slide out of an on-beat and out of an off-beat
        {
            Rig r;
            r.seq.setSwingPercent (pct);
            r.step (from, 40, false, /*slide=*/true); r.step (from + 1, 43);
            r.seq.setTrackLength (4);
            r.seq.start();
            r.run (4 * kStep);
            const auto ons = r.ons();
            CHECK (ons.size() == 2);
            if (ons.size() != 2) continue;
            const long off40 = r.offAfter (40, ons[0].at);
            CHECK (off40 - ons[1].at == 8 * kTick);  // same 8-tick overhang as unswung
            CHECK (off40 < r.offAfter (43, ons[1].at));   // no glide back to the slide source
        }
}

static void testSwingTieEndsInItsStep()
{
    Rig r;
    r.seq.setSwingPercent (62.5f);
    r.step (0, 40);
    r.seq.setTie (1, true);                          // tie lands on the late off-beat
    r.seq.setTrackLength (4);
    r.seq.start();
    r.run (2 * kStep);
    const auto ons = r.ons();
    CHECK (ons.size() == 1);
    CHECK (r.offAfter (40, ons[0].at) - ons[0].at == (36 + 6) * kTick);   // half way through the late tie step
}

// At 75 % an off-beat gate ends on the next onset; CPU Timing's longer NoteOff
// latency must not push the release past the next NoteOn.
static void testSwingReleasesBeforeNextNoteWithCpuTiming()
{
    for (bool tie : { false, true })
    {
        Rig r;
        r.seq.setSwingPercent (75.0f);
        r.seq.setCpuTiming (true);
        r.step (0, 40); r.step (1, 40); r.step (2, 40); r.step (3, 40);
        if (tie) { r.seq.setRest (1, true); r.seq.setTie (1, true); }
        r.seq.setTrackLength (4);
        r.seq.start();
        r.run (8 * kStep);
        const auto ons = r.ons();
        CHECK (ons.size() >= (tie ? 5u : 7u));
        for (size_t i = 1; i < ons.size(); ++i)
        {
            const long off = r.offAfter (40, ons[i - 1].at);
            CHECK (off != -1 && off <= ons[i].at);
        }
    }
}

static void testSwingHostStartMidPair()
{
    Rig r;
    r.seq.setSyncMode (AcidSequencer303::SyncMode::Host);
    r.seq.setSwingPercent (75.0f);
    r.step (0, 40); r.step (1, 43);
    r.seq.setTrackLength (2);
    r.seq.start();
    juce::MidiBuffer empty;
    double ppq = 0.25;                               // starts on the off-beat step
    for (int b = 0; b < 8; ++b)
    {
        r.seq.processBlock (empty, 512, true, ppq, 120.0);
        r.pos += 512;
        ppq += 512.0 / 24000.0;
    }
    const auto ons = r.ons();
    CHECK (! ons.empty());
    if (! ons.empty())
        CHECK (ons[0].note == 43 && ons[0].at == 12 * kTick);
}

static void testSwingMidiClockNotQuantised()
{
    Rig r;
    r.seq.setSyncMode (AcidSequencer303::SyncMode::MidiClock);
    r.seq.setSwingPercent (54.0f);                   // 1.92 ticks = 480 samples
    r.step (0, 40); r.step (1, 43);
    r.seq.setTrackLength (2);
    constexpr int kClock = 1000;                     // 24 PPQN at 120 bpm, 48 kHz
    for (int b = 0; b < 2 * 6 + 2; ++b)
    {
        juce::MidiBuffer mb;
        if (b == 0) mb.addEvent (juce::MidiMessage::midiStart(), 0);
        mb.addEvent (juce::MidiMessage::midiClock(), 0);
        r.seq.processBlock (mb, kClock, false, 0.0, 120.0);
        r.pos += kClock;
    }
    const auto ons = r.ons();
    CHECK (ons.size() >= 2);
    if (ons.size() >= 2)
        CHECK (ons[1].at - ons[0].at == kStep + 480);
}

// ---- re-roll ----------------------------------------------------------------

static void testRerollPitchesKeepsRhythm()
{
    AcidSequencer303 seq;
    for (int run = 0; run < 20; ++run)
    {
        seq.acidRandomize (60, 50, 50, 50, 0, 0, 0, 36, 12);
        bool rest[SEQ303_STEP_MAX], tie[SEQ303_STEP_MAX], acc[SEQ303_STEP_MAX], sld[SEQ303_STEP_MAX];
        for (int i = 0; i < SEQ303_STEP_MAX; ++i)
        {
            rest[i] = ! seq.stepOn (i); tie[i] = seq.tieOn (i);
            acc[i] = seq.accentOn (i);  sld[i] = seq.slideOn (i);
        }
        seq.randomizePitches (0, 0, 0, 60, 12);            // a range the old notes cannot be in
        for (int i = 0; i < SEQ303_STEP_MAX; ++i)
        {
            CHECK (rest[i] == ! seq.stepOn (i) && tie[i] == seq.tieOn (i));
            CHECK (acc[i] == seq.accentOn (i) && sld[i] == seq.slideOn (i));
            if (seq.stepOn (i))
                CHECK (seq.getRawNote (i) >= 60 && seq.getRawNote (i) < 72);
        }
    }
}

static void testRerollRhythmKeepsPitchOrder()
{
    const int pitches[] = { 40, 43, 45, 47 };
    for (int run = 0; run < 20; ++run)
    {
        AcidSequencer303 seq;
        seq.clearStepData (true);
        const int at[] = { 0, 2, 5, 9 };
        for (int k = 0; k < 4; ++k) { seq.setStepData (at[k], (uint8_t) pitches[k]); seq.setRest (at[k], false); }
        seq.setTrackLength (12);

        seq.randomizeRhythm (run < 10 ? 50 : 100, 50, 50, 50);
        int k = 0;
        for (int i = 0; i < SEQ303_STEP_MAX; ++i)
        {
            if (! seq.stepOn (i)) continue;
            CHECK (seq.getRawNote (i) == pitches[k % 4]);   // same pitches, same order, wrapping
            ++k;
        }
        if (run >= 10) CHECK (k == SEQ303_STEP_MAX);       // fill 100: every step plays
    }
}

// ---- pitch length (polymeter) ------------------------------------------------

static void setSevenNotes (Rig& r)                   // 7 note steps in 8, pitches 40..46, step 3 rests
{
    const int at[] = { 0, 1, 2, 4, 5, 6, 7 };
    for (int k = 0; k < 7; ++k) r.step (at[k], 40 + k);
    r.seq.setTrackLength (8);
}

static void testPitchLengthCyclesAcrossBars()
{
    Rig r;
    setSevenNotes (r);
    r.seq.setPitchLength (5);
    r.seq.start();
    r.run (3 * 8 * kStep);
    const auto n = playedNotes (r);
    CHECK (n.size() >= 21);
    for (size_t i = 0; i < 21 && i < n.size(); ++i)
        CHECK (n[i] == 40 + static_cast<int> (i % 5));   // 5-pitch loop over 7 note steps, carried across bars
}

static void testPitchLengthOffAndRotate()
{
    Rig off, rot;
    for (Rig* r : { &off, &rot }) setSevenNotes (*r);
    rot.seq.rotatePitches (2);                         // rewrites all 7 pitches, then loops the first 5
    rot.seq.setPitchLength (5);
    for (Rig* r : { &off, &rot }) { r->seq.start(); r->run (2 * 8 * kStep); }
    const auto a = playedNotes (off), b = playedNotes (rot);
    for (size_t i = 0; i < 14 && i < a.size(); ++i)
        CHECK (a[i] == 40 + static_cast<int> (i % 7));  // off: each step plays its own pitch
    for (size_t i = 0; i < 14 && i < b.size(); ++i)
        CHECK (b[i] == 42 + static_cast<int> (i % 5));
}

static void testPitchLengthFollowsHostPosition()
{
    Rig r;
    setSevenNotes (r);
    r.seq.setPitchLength (5);
    r.seq.setSyncMode (AcidSequencer303::SyncMode::Host);
    juce::MidiBuffer empty;
    r.seq.processBlock (empty, 512, true, 0.0, 120.0);   // enter Host mode while playing
    r.evs.clear();
    // jump to bar 3 (8-step pattern = 2 quarters; step 24 = start of the 4th loop)
    const double ppq = 24.0 / 4.0;
    r.seq.processBlock (empty, 512, true, ppq, 120.0);
    const auto n = playedNotes (r);
    CHECK (n.size() >= 1);
    if (! n.empty())
        CHECK (n[0] == 40 + (3 * 7) % 5);            // 3 loops x 7 notes = note index 21
}

// ---- undo -----------------------------------------------------------------------

static std::vector<int> stepSignature (const AcidSequencer303& seq)
{
    std::vector<int> v;
    for (int i = 0; i < SEQ303_STEP_MAX; ++i)
        v.push_back (seq.getRawNote (i) * 16 + seq.stepOn (i) * 8 + seq.tieOn (i) * 4
                     + seq.accentOn (i) * 2 + seq.slideOn (i));
    return v;
}

static void testUndoRedoPattern()
{
    AcidSequencer303 seq;
    CHECK (! seq.canUndo() && ! seq.canRedo());
    seq.acidRandomize (60, 50, 50, 50, 0, 0, 0, 36, 24);
    const auto a = stepSignature (seq);

    seq.pushUndo();
    seq.acidRandomize (60, 50, 50, 50, 0, 0, 0, 36, 24);
    const auto b = stepSignature (seq);
    CHECK (a != b);

    seq.pushUndo();
    seq.randomizePitches (0, 0, 0, 60, 12);
    const auto c = stepSignature (seq);

    CHECK (seq.undoCount() == 2 && seq.canUndo());
    CHECK (seq.undo() && stepSignature (seq) == b);
    CHECK (seq.undo() && stepSignature (seq) == a);
    CHECK (! seq.undo() && stepSignature (seq) == a);  // nothing left
    CHECK (seq.redoCount() == 2);
    CHECK (seq.redo() && stepSignature (seq) == b);
    CHECK (seq.redo() && stepSignature (seq) == c);
    CHECK (! seq.redo());

    seq.undo();
    seq.pushUndo();                                    // a new edit clears redo
    CHECK (! seq.canRedo());
}

static void testUndoHistoryIsBounded()
{
    AcidSequencer303 seq;
    for (int i = 0; i < 40; ++i)
    {
        seq.pushUndo();
        seq.acidRandomize (60, 50, 50, 50, 0, 0, 0, 36, 24);
    }
    CHECK (seq.undoCount() == AcidSequencer303::kUndoDepth);
    int n = 0;
    while (seq.undo()) ++n;
    CHECK (n == AcidSequencer303::kUndoDepth);
}

static void testPitchPlayhead()
{
    Rig r;
    setSevenNotes (r);                               // note steps 0,1,2,4,5,6,7
    r.seq.setPitchLength (5);
    CHECK (r.seq.getPitchPlayhead() == -1);          // stopped
    r.seq.start();
    r.run (12 * kStep + kStep / 2);                  // second pass, step 4 = 4th note
    CHECK (r.seq.getCurrentStep() == 4);
    CHECK (r.seq.getPitchPlayhead() == 0);           // note index 7 + 3 = 10 -> pitch entry 0 -> step 0

    r.run (kStep);                                   // step 5 = 5th note: index 11 -> entry 1 -> step 1
    CHECK (r.seq.getPitchPlayhead() == 1);

    r.seq.setPitchLength (0);
    r.run (kStep);                                   // off: pitch playhead follows the step
    CHECK (r.seq.getPitchPlayhead() == r.seq.getCurrentStep());

    r.seq.stop();
    CHECK (r.seq.getPitchPlayhead() == -1);
}

static void testStopReleasesNote()
{
    Rig r;
    r.step (0, 40, false, true);                     // slide: note is still held mid-step
    r.seq.setTrackLength (4);
    r.seq.start();
    r.run (kTick * 6);
    CHECK (r.ons().size() == 1);
    CHECK (r.offAfter (40, 0) == -1);                // still sounding
    r.seq.stop();
    CHECK (r.offAfter (40, 0) != -1);                // stop sent the note-off
    CHECK (! r.seq.isRunning());
}

static void testTempoClamp()
{
    AcidSequencer303 seq;
    seq.setTempo (5.0f);   CHECK (seq.getTempo() == 20.0f);
    seq.setTempo (999.0f); CHECK (seq.getTempo() == 300.0f);
    seq.setTempo (133.0f); CHECK (seq.getTempo() == 133.0f);
}

// ---- acidRandomize ----------------------------------------------------------

static void testRandomizeFillAndFlags()
{
    AcidSequencer303 seq;
    seq.acidRandomize (100, 0, 0, 0, 0, 0, 0, 36, 24);
    for (int i = 0; i < SEQ303_STEP_MAX; ++i)
    {
        CHECK (seq.stepOn (i));
        CHECK (! seq.accentOn (i) && ! seq.slideOn (i) && ! seq.tieOn (i));
        CHECK (seq.getRawNote (i) >= 36 && seq.getRawNote (i) < 60);
    }

    seq.acidRandomize (0, 100, 100, 100, 0, 0, 0, 36, 24);
    for (int i = 0; i < SEQ303_STEP_MAX; ++i)
        CHECK (! seq.stepOn (i));                    // fill 0: every step is a rest

    seq.acidRandomize (100, 100, 100, 100, 0, 0, 0, 36, 24);
    for (int i = 0; i < SEQ303_STEP_MAX; ++i)
        CHECK (seq.accentOn (i) && seq.slideOn (i) && ! seq.tieOn (i)); // tie only lands on rests
}

static void testRandomizeTieRule()
{
    AcidSequencer303 seq;
    for (int run = 0; run < 100; ++run)
    {
        seq.acidRandomize (50, 0, 0, 100, 0, 0, 0, 36, 24);
        for (int i = 0; i < SEQ303_STEP_MAX; ++i)
        {
            if (! seq.tieOn (i))
                continue;
            const int prev = (i == 0) ? SEQ303_STEP_MAX - 1 : i - 1;
            CHECK (! seq.stepOn (i));                              // ties live on rests
            CHECK (seq.stepOn (prev) || seq.tieOn (prev));         // ...that follow a note or tie
        }
    }
}

static void testRandomizeSilencesHeldNote()
{
    Rig r;
    r.step (0, 40, false, true);
    r.seq.setTrackLength (4);
    r.seq.start();
    r.run (kTick * 6);
    const int before = r.allNotesOffCalls;
    r.seq.acidRandomize (100, 0, 0, 0, 0, 0, 0, 36, 24);
    CHECK (r.allNotesOffCalls > before);             // host gets told to reset held-note state
    CHECK (r.offAfter (40, 0) != -1);
}

static void testReversePlaybackRunsBackwards()
{
    Rig r;
    r.step (0, 40); r.step (1, 43); r.step (2, 45); r.step (3, 47);
    r.seq.setReverse (2, true);
    r.seq.setTrackLength (4);
    r.seq.setReversePlayback (true);
    r.seq.start();
    r.run (4 * kStep);

    const auto ons = r.ons();
    CHECK (ons.size() >= 4);
    CHECK (ons[0].note == 47 && ons[1].note == 45 && ons[2].note == 43 && ons[3].note == 40);
    CHECK (ons[0].reverse && ! ons[1].reverse);     // a reverse-flagged step plays forward in reverse
}

static void testReversePlaybackStartsTiedRunAtFarEnd()
{
    Rig r;
    r.step (0, 40);
    r.seq.setTie (1, true); r.seq.setTie (2, true);  // step 0 holds through steps 1-2
    r.step (3, 47);
    r.seq.setTrackLength (4);
    r.seq.setReversePlayback (true);
    r.seq.start();
    r.run (4 * kStep);

    const auto ons = r.ons();
    CHECK (ons.size() == 2);
    CHECK (ons[0].note == 47 && ons[1].note == 40);
    CHECK (ons[1].at - ons[0].at == kStep);          // the run starts on its last tied rest
    CHECK (r.offAfter (40, ons[1].at) == ons[1].at + 2 * kStep + 12 * kTick);
}

static void testReversePlaybackSlidesIntoTiedNote()
{
    // forward: 40 slides into 43, which holds through tied step 2. Reverse: the run starts at
    // step 2 (43) and slides into 40, the flag on its forward sender
    Rig r;
    r.step (0, 40, false, /*slide=*/true); r.step (1, 43);
    r.seq.setTie (2, true);
    r.seq.setTrackLength (3);
    r.seq.setReversePlayback (true);
    r.seq.start();
    r.run (3 * kStep);

    const auto ons = r.ons();
    CHECK (ons.size() >= 2 && ons[0].note == 43 && ons[1].note == 40);
    CHECK (ons[1].at - ons[0].at == 2 * kStep);
    CHECK (r.offAfter (43, ons[0].at) > ons[1].at);   // legato: 40 starts before 43 ends
}

static void testReversePlaybackSlidesOutOfTiedNote()
{
    // forward: 40 holds through tied step 1 and slides into 47. Reverse: 47 slides into 40's run,
    // the flag on the run's anchor
    Rig r;
    r.step (0, 40, false, /*slide=*/true);
    r.seq.setTie (1, true);
    r.step (2, 47);
    r.seq.setTrackLength (3);
    r.seq.setReversePlayback (true);
    r.seq.start();
    r.run (3 * kStep);

    const auto ons = r.ons();
    CHECK (ons.size() >= 2 && ons[0].note == 47 && ons[1].note == 40);
    CHECK (ons[1].at - ons[0].at == kStep);
    CHECK (r.offAfter (47, ons[0].at) > ons[1].at);   // legato: 40 starts before 47 ends
}

static void testHammerIntoFlagsTheReceivingNote()
{
    Rig r;
    r.step (0, 40); r.seq.setHammer (0, true); r.step (1, 43);
    r.seq.setTrackLength (2);
    r.seq.start();
    r.run (2 * kStep);

    const auto ons = r.ons();
    CHECK (! ons[0].hammer && ons[1].hammer);
    CHECK (r.offAfter (40, ons[0].at) > ons[1].at);  // hammer overlaps like a slide
}

static void testTiedReversedNoteCarriesGateAndMirrorsIndicator()
{
    Rig r;
    r.step (0, 40); r.seq.setReverse (0, true);
    r.seq.setTie (1, true); r.seq.setTie (2, true);
    r.step (3, 47);
    r.seq.setTrackLength (4);
    r.seq.start();

    r.run (kStep / 2);
    const auto ons = r.ons();
    CHECK (ons.size() == 1 && ons[0].reverse);
    CHECK (ons[0].gate == (2 * 24 + 12) * kTick);   // the whole tied section
    CHECK (r.seq.getCurrentStep() == 2);            // indicator starts at the section's end
    r.run (kStep);
    CHECK (r.seq.getCurrentStep() == 1);
    r.run (kStep);
    CHECK (r.seq.getCurrentStep() == 0);
    r.run (kStep);
    CHECK (r.seq.getCurrentStep() == 3);            // back to normal after the section
}

static void testFlaggedTiedSectionMirrorsIndicatorInReverse()
{
    Rig r;
    r.step (0, 40); r.seq.setReverse (0, true);
    r.seq.setTie (1, true); r.seq.setTie (2, true);
    r.step (3, 47);
    r.seq.setTrackLength (4);
    r.seq.setReversePlayback (true);
    r.seq.start();

    r.run (kStep / 2);
    CHECK (r.seq.getCurrentStep() == 3);
    r.run (kStep);                                   // the run's far end triggers the anchor note
    CHECK (r.ons().size() == 2 && ! r.ons()[1].reverse);
    CHECK (r.seq.getCurrentStep() == 0);            // reverse of reverse: starts at the note
    r.run (kStep);
    CHECK (r.seq.getCurrentStep() == 1);
    r.run (kStep);
    CHECK (r.seq.getCurrentStep() == 2);
}

int main()
{
    testStepSpacingAndGate();
    testReversePlaybackRunsBackwards();
    testReversePlaybackStartsTiedRunAtFarEnd();
    testReversePlaybackSlidesIntoTiedNote();
    testReversePlaybackSlidesOutOfTiedNote();
    testHammerIntoFlagsTheReceivingNote();
    testTiedReversedNoteCarriesGateAndMirrorsIndicator();
    testFlaggedTiedSectionMirrorsIndicatorInReverse();
    testAccentVelocity();
    testSlideOverlapsNextNote();
    testMuteChokesGate();
    testMuteKeepsSlideOverlap();
    testHammerOverlapsAndFlagsNextNote();
    testHammerCarriesAcrossTie();
    testRestIsSilent();
    testTieHoldsAcrossRest();
    testSameNoteSlideIsOneGate();
    testSlideCarriesAcrossTie();
    testPlayedStepDropsTie();
    testCpuTimingDelaysGate();
    testCpuTimingStopFlushesQueue();
    testPitchRotateKeepsRhythm();
    testShiftRotatesPattern();
    testShortestTurn();
    testTransposeRewritesNotes();
    testTransposeKeepsPatternInRange();
    testSwingDelaysOffbeats();
    testSwingIsContinuous();
    testSwingKeepsSlideLegato();
    testSwingTieEndsInItsStep();
    testSwingReleasesBeforeNextNoteWithCpuTiming();
    testSwingHostStartMidPair();
    testSwingMidiClockNotQuantised();
    testRerollPitchesKeepsRhythm();
    testRerollRhythmKeepsPitchOrder();
    testPitchLengthCyclesAcrossBars();
    testPitchLengthOffAndRotate();
    testPitchLengthFollowsHostPosition();
    testPitchPlayhead();
    testUndoRedoPattern();
    testUndoHistoryIsBounded();
    testStopReleasesNote();
    testTempoClamp();
    testRandomizeFillAndFlags();
    testRandomizeTieRule();
    testRandomizeSilencesHeldNote();

    if (failures == 0) std::printf ("all passed\n");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
