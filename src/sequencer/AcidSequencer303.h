#pragma once

/*!
 * AcidSequencer303.h
 *
 * Full JUCE port of midilab/aciduino v2 engine_303.cpp + harmonizer.cpp
 * Original: (c) 2022 Romulo Silva <contact@midilab.co> - MIT License
 * JUCE port: lives inside JC303, ticked sample-accurately from processBlock()
 *
 * ── Clock model ──────────────────────────────────────────────────────────────
 * The sequencer is PPQN-agnostic.  It is initialised with the host (or
 * internal) PPQN resolution via prepare() and derives every timing
 * quantity from that single number:
 *
 *   pulsesPerStep     = ppqn / 4          (one 16th-note)
 *   noteLengthPulses  = pulsesPerStep * (12/24)   original 50% gate ratio
 *   slideExtraPulses  = pulsesPerStep * (20/24)   slide overhang ratio
 *   tie extension     = i * pulsesPerStep          (one step per tied rest)
 *
 * ── Sync modes ────────────────────────────────────────────────────────────────
 *   SyncMode::Internal   – free-running BPM accumulator (setTempo())
 *   SyncMode::Host       – AudioPlayHead ppqPosition + bpm
 *   SyncMode::MidiClock  – MIDI Start / Stop / Clock from the MidiBuffer
 *
 * ── Start modes ───────────────────────────────────────────────────────────────
 *   StartMode::TransportStart  – starts on host Play / MIDI Start message
 *   StartMode::NoteTriggered   – starts on the first incoming MIDI NoteOn
 *
 * ── Thread safety ─────────────────────────────────────────────────────────────
 * All UI-thread setters that touch pattern data acquire _dataLock (SpinLock).
 * The note stack and step-location are audio-thread-only.
 * _mute and _running are std::atomic.
 */

#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <functional>
#include <random>

#include "Harmonizer.h"

// =============================================================================
// Constants  — exact values from engine_303.h
// =============================================================================
static constexpr int      SEQ303_STEP_MAX         = 16;   // STEP_MAX_SIZE_303
static constexpr int      SEQ303_NOTE_STACK_SIZE   = 3;    // NOTE_STACK_SIZE_303
static constexpr uint8_t  SEQ303_NOTE_VELOCITY     = 70;   // NOTE_VELOCITY_303
static constexpr uint8_t  SEQ303_ACCENT_VELOCITY   = 127;  // ACCENT_VELOCITY_303
static constexpr uint8_t  SEQ303_DEFAULT_NOTE      = 36;   // C1 (DAW numbering)

// Lowest note the sequencer GUI enters: TB-303 owner's manual p.88, TRANSPOSE
// DOWN starts at 1V = 32.7 Hz (MIDI 24, C0 in DAW numbering). The engine itself
// accepts any MIDI note.
static constexpr uint8_t  SEQ303_NOTE_MIN          = 24;

// MIDI velocity at/above which a recorded step is marked accented — matches
// Open303::noteOn() (`velocity >= 100` triggers accent on the synth side).
static constexpr uint8_t  SEQ303_ACCENT_VELOCITY_THRESHOLD = 100;

// NOTE_LENGTH_303 = 12 pulses at 96 PPQN  →  12/24 = 50% gate (original TB-303).
// The comment in engine_303.h says explicitly: "12 = 50% on 96ppqn, same as original tb303".
// Slide extra = 20 pulses at 96 PPQN (extends the gate slightly past the next note-on).
// Both are stored as ratios so they scale correctly at any host PPQN.
static constexpr double SEQ303_NOTE_LENGTH_RATIO = 12.0 / 24.0;  // 50% gate
static constexpr double SEQ303_SLIDE_EXTRA_RATIO = 20.0 / 24.0;  // slide overhang

// Muted (choked) note: gate length multiplier, matches Open303 muteGateFactor.
static constexpr double SEQ303_MUTE_GATE_FACTOR  = 0.5;

// =============================================================================
// Data structures
// =============================================================================
struct StepData303
{
    uint8_t note   { SEQ303_DEFAULT_NOTE };
    bool    rest   { false  };
    bool    accent { false };
    bool    slide  { false };
    bool    tie    { false };
    bool    mute   { false };   // choked note; excludes accent
    bool    hammer { false };   // legato with instant pitch change; excludes slide
    bool    reverse { false };  // note plays time-reversed (swell into the gate end)
};

struct NoteStackEntry303
{
    uint8_t  note   { 0  };
    int32_t  length { -1 };   // -1 = free slot; positive = pulse countdown
    double   offFrac { 0.0 }; // release lands this fraction of a tick after the countdown ends
};

/** Full pattern data for one track (serialisable, UI-readable). */
struct TrackData303
{
    StepData303 step[SEQ303_STEP_MAX];
    uint8_t     stepLength { SEQ303_STEP_MAX };
    uint8_t     tune       { 0 };  // 0=off; 1+N = harmonize + N semitone offset
    uint8_t     pitchLength { 0 }; // 0 = off; N = loop the first N pitches across bars
};

// =============================================================================
// Event types emitted to the caller (JC303::processBlock)
// =============================================================================
enum class Acid303EventType { NoteOn, NoteOff };

struct Acid303Event
{
    Acid303EventType type;
    uint8_t          note;
    uint8_t          velocity;
    int              sampleOffset;   // sample position within the current buffer
    bool             mute       { false };
    bool             reverse    { false };   // effective: step flag XOR reverse playback
    bool             hammerInto { false };   // note arrives by legato from a hammer step: snap pitch
    double           gateSamples { 0.0 };    // gate length in samples (0 = unknown)
};

// =============================================================================
// AcidSequencer303
// =============================================================================
class AcidSequencer303
{
public:
    // ── Sync / Start enums ───────────────────────────────────────────────────
    enum class SyncMode  { Internal, Host, MidiClock };
    enum class StartMode { TransportStart, NoteTriggered };

    // ── Event callback ───────────────────────────────────────────────────────
    /** Install before the first processBlock call. Always called on the audio thread. */
    std::function<void(const Acid303Event&)> onNoteEvent;

    /** Called whenever the sequencer clears its note stack (stop, mute, clearTrack,
     *  acidRandomize), after the NoteOffs for held notes have been emitted. */
    std::function<void()> onAllNotesOff;

    // ── Constructor ──────────────────────────────────────────────────────────
    AcidSequencer303()
    {
        _rng.seed (static_cast<uint32_t>(juce::Time::currentTimeMillis()));
        init();
    }

    ~AcidSequencer303() = default;

    // =========================================================================
    // Initialisation — mirrors Engine303::init()
    // =========================================================================

    void init()
    {
        {
            juce::SpinLock::ScopedLockType lk (_dataLock);
            _data.stepLength = SEQ303_STEP_MAX;
            _data.tune       = 0;
            _data.pitchLength = 0;
            clearStepDataInternal (/*fillWithRests=*/false);
        }
        _stepLocation = 0;
        clearStackInternal (/*sendNoteOffs=*/false);
        _mute.store (false);
    }

    /** Call once from AudioProcessor::prepareToPlay.
     *  @param sampleRate  Current audio sample rate.
     *  @param ppqn        Host PPQN resolution (e.g. 960).
     */
    void prepare (double sampleRate, double ppqn)
    {
        _sampleRate    = sampleRate;
        _ppqn          = ppqn;
        _pulsesPerStep = ppqn / 4.0;    // one 16th-note = ppqn/4 pulses

        // ── 96-PPQN-equivalent tick ───────────────────────────────────────────
        // The original engine uses 96 PPQN internally:
        //   - steps fire every 24 pulses  (= one 16th-note at 96 PPQN)
        //   - note-off stack ticks every 1 pulse
        // We keep that granularity by defining one "tick" as ppqn/96 pulses.
        // At 960 PPQN: 1 tick = 10 pulses. Step = every 240 pulses = 24 ticks.
        // All note lengths remain expressed in 96-PPQN ticks so the numbers
        // (NOTE_LENGTH_303=12, etc.) are identical to the original.
        _ticksPerPulse     = _ppqn / 96.0;           // pulses per 96-PPQN tick
        _samplesPerTick    = (_sampleRate * 60.0)     // updated by updateSamplesPerTick()
                             / (_internalBpm * 96.0); // = samplesPerPulse * ticksPerPulse^-1
        _ticksPerStep      = 24;                      // always 24 at 96-PPQN resolution

        _noteLengthTicks = std::max (1, static_cast<int>(_ticksPerStep * SEQ303_NOTE_LENGTH_RATIO));
        _slideExtraTicks = std::max (1, static_cast<int>(_ticksPerStep * SEQ303_SLIDE_EXTRA_RATIO));

        updateSamplesPerTick();
        _sampleAccum     = 0.0;
        _tickCounter     = 0;
        _lastHostTick    = -1;
        _lastMode        = _syncMode.load (std::memory_order_relaxed);
        _blockStart      = 0;
        _queueCount      = 0;
        _lastMidiClock   = -1;
        _tickSamples     = _samplesPerTick;
    }

    // =========================================================================
    // Sync / Start modes
    // =========================================================================

    void      setSyncMode  (SyncMode  m) { _syncMode.store  (m, std::memory_order_relaxed); }
    void      setStartMode (StartMode m) { _startMode.store (m, std::memory_order_relaxed); }
    SyncMode  getSyncMode()  const       { return _syncMode.load  (std::memory_order_relaxed); }
    StartMode getStartMode() const       { return _startMode.load (std::memory_order_relaxed); }

    // =========================================================================
    // Internal tempo (SyncMode::Internal)
    // =========================================================================

    void  setTempo (float bpm)
    {
        // Stored atomically; _samplesPerTick is recomputed on the audio thread
        // at the top of processBlock() so a UI-thread change can't tear the
        // double the audio thread reads while clocking.
        _internalBpm.store (juce::jlimit (20.f, 300.f, bpm), std::memory_order_relaxed);
    }
    float getTempo() const { return _internalBpm.load (std::memory_order_relaxed); }

    // =========================================================================
    // CPU timing (TB-303 uPD650C-133 interrupt latency)
    // =========================================================================
    /** When on, every gate edge lands on a free-running 1.8 ms interrupt grid
     *  that is not synced to the tempo, NoteOn ~1.0 ms and NoteOff ~2.35 ms
     *  after the interrupt (J. Schmidt, "Analysis of the uPD650C-133 CPU
     *  timing").  Gates run slightly long and beat against the tempo. */
    void setCpuTiming (bool on) { _cpuTiming.store (on, std::memory_order_relaxed); }
    bool getCpuTiming() const   { return _cpuTiming.load (std::memory_order_relaxed); }

    // =========================================================================
    // Swing (MPC style)
    // =========================================================================
    /** Off-beat 16ths land at `percent` of the 8th-note pair: 50 % is straight,
     *  75 % is 12 ticks late.  Continuous; the fraction of a tick is applied as
     *  a sample offset. */
    void  setSwingPercent (float percent) { _swingPercent.store (juce::jlimit (50.f, 75.f, percent), std::memory_order_relaxed); }
    float getSwingPercent() const         { return _swingPercent.load (std::memory_order_relaxed); }

    // =========================================================================
    // Transport control
    // =========================================================================

    void start()
    {
        if (_running) return;
        clearStackInternal (true);
        _tickCounter  = 0;
        _sampleAccum  = 0.0;
        _lastHostTick = -1;
        _lastMidiClock = -1;
        _swingRelatch = true;
        _pitchPlayhead = -1;
        _mirrorLeft   = 0;
        _running      = true;
    }

    void stop()
    {
        if (! _running) return;
        _running = false;
        _pitchPlayhead = -1;
        clearStackInternal (true);
    }

    bool isRunning() const { return _running.load(); }

    /** Reverse playback: steps run last to first and every note's envelope is
     *  reversed, except steps flagged reverse (reverse of reverse = forward). */
    void setReversePlayback (bool v) { _reversePlay.store (v); }
    bool getReversePlayback() const  { return _reversePlay.load(); }

    // =========================================================================
    // processBlock — called every buffer from JC303::processBlock
    // =========================================================================
    /**
     * @param midiIn            Incoming MidiBuffer (MIDI sync + note-trigger).
     * @param numSamples        Buffer length in samples.
     * @param hostIsPlaying     AudioPlayHead::isPlaying.
     * @param ppqAtBufferStart  ppqPosition at sample 0 (Host sync).
     * @param bpm               Current host BPM (Host sync; ignored in Internal).
     */
    void processBlock (const juce::MidiBuffer& midiIn,
                       int    numSamples,
                       bool   hostIsPlaying,
                       double ppqAtBufferStart,
                       double bpm)
    {
        _blockLength = numSamples;

        // Apply any tempo queued from the UI thread before clocking.
        // _samplesPerTick is only ever written on the audio thread from here.
        updateSamplesPerTick();

        // Read the sync mode once per buffer.  It is written from the UI
        // thread, so the change is observed here — all state mutation below
        // stays on the audio thread.
        const SyncMode mode = _syncMode.load (std::memory_order_relaxed);

        // ── Sync-mode change: land in a clean, predictable state ──────────────
        // Switching modes mid-playback must stop the sequencer and reset the
        // clock bookkeeping that belongs to the previous mode (_lastHostTick /
        // _tickCounter / _sampleAccum).  Without this, a stale host tick can
        // trigger a pathological catch-up run and cross-mode garbage.
        if (mode != _lastMode)
        {
            _lastMode = mode;
            stop();
            _lastHostTick  = -1;
            _tickCounter   = 0;
            _sampleAccum   = 0.0;

            // Entering Host while the host is already playing produces no play
            // edge, so re-arm immediately; the host ppq re-anchors the position.
            if (mode == SyncMode::Host
                && hostIsPlaying
                && _startMode == StartMode::TransportStart)
                start();
        }

        // ── MIDI transport + note-triggered start ─────────────────────────────
        for (const auto meta : midiIn)
        {
            const auto msg       = meta.getMessage();
            const int  samplePos = meta.samplePosition;

            if (mode == SyncMode::MidiClock)
            {
                if (msg.isMidiStart())
                {
                    _tickCounter   = 0;
                    _sampleAccum   = 0.0;
                    _lastHostTick  = -1;
                    _lastMidiClock = -1;
                    _swingRelatch  = true;
                    if (_startMode == StartMode::TransportStart)
                        start();
                }
                else if (msg.isMidiStop())
                {
                    stop();
                }
                else if (msg.isMidiClock() && _running)
                {
                    tickMidiClock (samplePos);
                }
            }

            if (_startMode == StartMode::NoteTriggered && ! _running && msg.isNoteOn())
                start();
        }

        // ── Clock advancement ─────────────────────────────────────────────────
        switch (mode)
        {
            case SyncMode::Internal:
                processInternalClock (numSamples);
                break;

            case SyncMode::Host:
                processHostClock (numSamples, hostIsPlaying, ppqAtBufferStart, bpm);
                break;

            case SyncMode::MidiClock:
                break; // handled above per-message
        }

        flushQueue (numSamples);
        _blockStart += numSamples;
    }

    // =========================================================================
    // ── Per-step setters / getters — UI-thread-safe ───────────────────────────
    // =========================================================================

    /** rest(step, state) — mirrors Engine303::rest().  A played step drops its
     *  tie, as in the aciduino step editor. */
    void setRest (int step, bool v)
    {
        juce::SpinLock::ScopedLockType lk(_dataLock);
        _data.step[step].rest = v;
        if (! v)
            _data.step[step].tie = false;
    }
    /** setAccent(step, state) */
    void setAccent (int step, bool v) { juce::SpinLock::ScopedLockType lk(_dataLock); _data.step[step].accent = v; if (v) _data.step[step].mute = false; }
    /** setSlide(step, state) */
    void setSlide  (int step, bool v) { juce::SpinLock::ScopedLockType lk(_dataLock); _data.step[step].slide  = v; if (v) _data.step[step].hammer = false; }
    /** setTie(step, state) */
    void setTie    (int step, bool v) { juce::SpinLock::ScopedLockType lk(_dataLock); _data.step[step].tie    = v; }
    /** setMute(step, state) -- choked note; clears accent */
    void setMute   (int step, bool v) { juce::SpinLock::ScopedLockType lk(_dataLock); _data.step[step].mute   = v; if (v) _data.step[step].accent = false; }
    /** setReverse(step, state) -- reversed-envelope note */
    void setReverse(int step, bool v) { juce::SpinLock::ScopedLockType lk(_dataLock); _data.step[step].reverse = v; }
    /** setHammer(step, state) -- instant-pitch legato; clears slide */
    void setHammer (int step, bool v) { juce::SpinLock::ScopedLockType lk(_dataLock); _data.step[step].hammer = v; if (v) _data.step[step].slide  = false; }

    /** setStepData(step, note) — mirrors Engine303::setStepData() */
    void setStepData (int step, uint8_t note)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        _data.step[step].note = note;
    }

    /** getStepData(step) — mirrors Engine303::getStepData(): returns the
     *  harmonized note if tune > 0, raw MIDI note otherwise. */
    uint8_t getStepData (int step) const
    {
        const uint8_t raw = _data.step[step].note;
        if (_data.tune > 0)
        {
            juce::SpinLock::ScopedLockType lk (_dataLock);
            return _harmonizer.harmonize (raw) + static_cast<uint8_t>(_data.tune - 1);
        }
        return raw;
    }

    /** Raw MIDI note, bypassing harmonizer. */
    uint8_t getRawNote (int step) const { return _data.step[step].note; }

    /** stepOn(step) — mirrors Engine303::stepOn() */
    bool stepOn  (int step) const { return ! _data.step[step].rest;   }
    /** accentOn(step) */
    bool accentOn (int step) const { return  _data.step[step].accent; }
    /** slideOn(step) */
    bool slideOn  (int step) const { return  _data.step[step].slide;  }
    /** tieOn(step) */
    bool tieOn    (int step) const { return  _data.step[step].tie;    }
    /** muteOn(step) */
    bool muteOn   (int step) const { return  _data.step[step].mute;   }
    /** hammerOn(step) */
    bool hammerOn (int step) const { return  _data.step[step].hammer; }
    /** reverseOn(step) */
    bool reverseOn(int step) const { return  _data.step[step].reverse; }

    // =========================================================================
    // ── Track-level getters / setters — mirrors engine_303 API ───────────────
    // =========================================================================

    /** `delta` taken the shorter way round a cycle of `len` values, in
     *  (-len/2, len/2]: a menu value wrapping at len reads as one step. */
    static int shortestTurn (int delta, int len)
    {
        int d = ((delta % len) + len) % len;
        if (d > len / 2) d -= len;
        return d;
    }

    /** Rotate the steps in the active length `n` places earlier: step n
     *  becomes step 0.  Steps past the length are untouched. */
    void shiftSteps (int n)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        const int len = _data.stepLength;
        StepData303 old[SEQ303_STEP_MAX];
        std::copy (_data.step, _data.step + len, old);
        for (int i = 0; i < len; ++i)
            _data.step[i] = old[((i + n) % len + len) % len];
    }

    /** Rotate the pitches through the note steps in the active length: each
     *  note step takes the pitch `n` note steps later.  Rests, ties, accents
     *  and slides stay on their steps. */
    void rotatePitches (int n)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        int     noteSteps[SEQ303_STEP_MAX];
        uint8_t pitches[SEQ303_STEP_MAX];
        int count = 0;
        for (int i = 0; i < _data.stepLength; ++i)
            if (! _data.step[i].rest)
            {
                noteSteps[count] = i;
                pitches[count++] = _data.step[i].note;
            }
        for (int k = 0; k < count; ++k)
            _data.step[noteSteps[k]].note = pitches[((k + n) % count + count) % count];
    }

    /** setPitchLength / getPitchLength — 0 = off; N = the note steps cycle
     *  through the first N pitches, carried across bars (polymeter).  The
     *  pitch is derived from the play position, so host seeks stay in phase. */
    void    setPitchLength (uint8_t length)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        _data.pitchLength = static_cast<uint8_t>(juce::jmin<int> (length, SEQ303_STEP_MAX));
    }
    uint8_t getPitchLength() const { return _data.pitchLength; }

    /** Pitch playhead: the step whose stored pitch the last started note
     *  used (differs from the step position with Pitch Rotate / Pitch
     *  Length).  Holds through ties and rests; -1 while stopped. */
    int getPitchPlayhead() const { return _pitchPlayhead.load (std::memory_order_relaxed); }

    /** setTune / getTune
     *  0   = harmonizer off (raw MIDI note)
     *  1+N = harmonize note then add N semitones of offset */
    void    setTune (uint8_t tune)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        _data.tune = tune;
    }
    uint8_t getTune() const { return _data.tune; }

    /** True if every stored step note (rests included) stays in 0..127
     *  after shifting by `semitones`. */
    bool canTransposeNotes (int semitones) const
    {
        for (const auto& s : _data.step)
            if (s.note + semitones < 0 || s.note + semitones > 127)
                return false;
        return true;
    }

    /** Shift every stored step note by `semitones`; refused, leaving the pattern
     *  untouched, unless canTransposeNotes(). */
    bool transposeNotes (int semitones)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        if (! canTransposeNotes (semitones)) return false;
        for (auto& s : _data.step)
            s.note = static_cast<uint8_t>(s.note + semitones);
        return true;
    }

    /** getCurrentStep — last-played step index.
     *  Written on the audio thread; UI reads are an acceptable approximation. */
    uint8_t getCurrentStep() const { return _stepLocation.load(); }

    /** setTrackLength / getTrackLength — active number of steps (1…SEQ303_STEP_MAX) */
    void    setTrackLength (uint8_t length)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        _data.stepLength = juce::jlimit<uint8_t> (1, SEQ303_STEP_MAX, length);
    }
    uint8_t getTrackLength() const { return _data.stepLength; }

    /** setMute / getMute — silences note-on output; clears stack when muting */
    void setMute (bool m)
    {
        _mute.store (m);
        if (m) clearStackInternal (true);
    }
    bool getMute() const { return _mute.load(); }

    // =========================================================================
    // ── Pattern utilities — complete port of engine_303 ───────────────────────
    // =========================================================================

    /** clearStepData(rest) — mirrors Engine303::clearStepData(track, rest).
     *  Resets every step to the default note; optionally fills all as rests. */
    void clearStepData (bool fillWithRests)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        clearStepDataInternal (fillWithRests);
    }

    /** clearTrack — mirrors Engine303::clearTrack().
     *  Mutes the track, clears the note stack, resets all steps to rest,
     *  then unmutes. */
    void clearTrack()
    {
        setMute (true);
        clearStackInternal (true);
        {
            juce::SpinLock::ScopedLockType lk (_dataLock);
            clearStepDataInternal (true);
        }
        setMute (false);
    }

    // =========================================================================
    // ── Rec mode — serial note/rest entry from UI + MIDI ──────────────────────
    // =========================================================================

    void setRecState (bool on) { _recOn.store (on); }
    bool isRecOn() const       { return _recOn.load (std::memory_order_relaxed); }

    void    setRecStep (uint8_t step)                  { _recStep.store (step, std::memory_order_relaxed); }
    uint8_t getRecStep() const                         { return _recStep.load (std::memory_order_relaxed); }

    /** Record a note at the current rec cursor and advance it (wraps at track
     *  length). Legato (slide) is used when a note is struck while the previous
     *  recorded note was still held down. */
    void recNote (uint8_t note, bool accent, bool legato)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        const uint8_t st = _recStep.load (std::memory_order_relaxed);
        auto& s = _data.step[st];
        s.note   = note;
        s.rest   = false;
        s.accent = accent;
        s.slide  = legato;
        s.tie    = false;
        s.mute   = false;
        s.hammer = false;
        s.reverse = false;
        _recStep.store ((st + 1) % _data.stepLength, std::memory_order_relaxed);
    }

    /** Record a note at the current rec cursor and advance it (wraps at track
     *  length), keeping the step's existing accent/slide/tie flags. Used by the
     *  on-screen keyboard, where the note is the whole input. */
    void recNoteKeepFlags (uint8_t note)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        const uint8_t st = _recStep.load (std::memory_order_relaxed);
        auto& s = _data.step[st];
        s.note = note;
        s.rest = false;
        _recStep.store ((st + 1) % _data.stepLength, std::memory_order_relaxed);
    }

    /** Record a rest at the current rec cursor and advance it (wraps at track
     *  length). Same effect as the REST button / sustain-pedal tap. */
    void recRest()
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        const uint8_t st = _recStep.load (std::memory_order_relaxed);
        auto& s = _data.step[st];
        s.rest   = true;
        s.accent = false;
        s.slide  = false;
        s.tie    = false;
        s.mute   = false;
        s.hammer = false;
        s.reverse = false;
        _recStep.store ((st + 1) % _data.stepLength, std::memory_order_relaxed);
    }

    /** clearStackNote — mirrors Engine303::clearStackNote(track).
     *  Sends Note Off for all active notes and frees the stack. */
    void clearStackNote()
    {
        clearStackInternal (true);
    }

    /** setLongTie — mirrors Engine303::setLongTie(track, startStep, stepEnd).
     *
     *  Marks steps from (startStep+1) up to stepEnd as rest+tie, carrying the
     *  note of startStep.  Handles wrap-around within the active step length.
     */
    void setLongTie (uint8_t startStep, uint8_t stepEnd)
    {
        const uint8_t stepSize   = _data.stepLength;
        // getStepData applies harmonizer if tune>0 — same as original
        const uint8_t anchorNote = getStepData (static_cast<int>(startStep));
        const uint8_t firstTied  = static_cast<uint8_t>((startStep + 1) % stepSize);

        uint8_t stepsHolded = 0;
        if (stepEnd < firstTied)
            stepsHolded = stepSize - (firstTied - stepEnd);
        else if (stepEnd > firstTied)
            stepsHolded = stepEnd - firstTied;

        if (stepsHolded == 0) return;

        juce::SpinLock::ScopedLockType lk (_dataLock);
        for (uint16_t i = firstTied; i < static_cast<uint16_t>(firstTied + stepsHolded); ++i)
        {
            const uint8_t idx = static_cast<uint8_t>(i % stepSize);
            _data.step[idx].rest   = true;
            _data.step[idx].tie    = true;
            _data.step[idx].slide  = false;
            _data.step[idx].mute   = false;
            _data.step[idx].hammer = false;
            _data.step[idx].reverse = false;
            _data.step[idx].note   = anchorNote;
        }
    }

    /** acidRandomize — complete port of Engine303::acidRandomize().
     *
     *  Generates a random pattern across all SEQ303_STEP_MAX steps.
     *
     *  @param fill               0–100  % chance each step is ON (not rest)
     *  @param accentProbability  0–100  % chance of accent on an ON step
     *  @param slideProbability   0–100  % chance of slide on an ON step
     *  @param tieProbability     0–100  % chance of tie on a REST step
     *                            (only considered when the previous step had
     *                             a note on it or was itself a tie)
     *  @param numberOfTones      0 = full chromatic; 1–12 = snap to N equally-
     *                            spaced semitone intervals (same algorithm as
     *                            the original fix_tones[] table)
     *  @param lowerNote          lowest MIDI note in the random pitch range
     *  @param rangeNote          range above lowerNote (exclusive upper bound)
     */
    void acidRandomize (uint8_t fill,
                        uint8_t accentProbability,
                        uint8_t slideProbability,
                        uint8_t tieProbability,
                        uint8_t numberOfTones,
                        uint8_t lowerNote,
                        uint8_t rangeNote)
    {
        int8_t fixTones[12];
        makeFixTones (numberOfTones, fixTones);

        // Clear the track: mutes, sends NoteOff for any held note (fires
        // onAllNotesOff), clears all steps, then unmutes.
        clearTrack();

        {
            juce::SpinLock::ScopedLockType lk (_dataLock);
            for (int i = 0; i < SEQ303_STEP_MAX; ++i)
                if (rollRhythmStep (i, fill, accentProbability, slideProbability, tieProbability))
                    _data.step[i].note = randomNote (fixTones, numberOfTones, lowerNote, rangeNote);
        }

        // clearTrack() already called setMute(false) — no need to unmute again.
    }

    // =========================================================================
    // ── Pattern undo / redo (step data only; settings live in their params) ──
    // =========================================================================

    static constexpr int kUndoDepth = 16;

    /** Snapshot the steps before a destructive edit; clears the redo history. */
    void pushUndo()
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        pushSnapshot (_undo, _undoCount);
        _redoCount = 0;
    }

    bool undo() { return swapHistory (_undo, _undoCount, _redo, _redoCount); }
    bool redo() { return swapHistory (_redo, _redoCount, _undo, _undoCount); }

    int  undoCount() const { return _undoCount.load (std::memory_order_relaxed); }
    int  redoCount() const { return _redoCount.load (std::memory_order_relaxed); }
    bool canUndo()   const { return undoCount() > 0; }
    bool canRedo()   const { return redoCount() > 0; }

    /** New random pitches on the note steps; rests, ties, accents and slides stay. */
    void randomizePitches (uint8_t numberOfTones, uint8_t lowerNote, uint8_t rangeNote)
    {
        int8_t fixTones[12];
        makeFixTones (numberOfTones, fixTones);

        setMute (true);
        clearStackInternal (true);
        {
            juce::SpinLock::ScopedLockType lk (_dataLock);
            for (auto& st : _data.step)
                if (! st.rest)
                    st.note = randomNote (fixTones, numberOfTones, lowerNote, rangeNote);
        }
        setMute (false);
    }

    /** New random rests, ties, accents and slides; the pattern's pitches are
     *  kept in order and laid onto the new note steps (wrapping), like
     *  editing the time list on a TB-303. */
    void randomizeRhythm (uint8_t fill,
                          uint8_t accentProbability,
                          uint8_t slideProbability,
                          uint8_t tieProbability)
    {
        setMute (true);
        clearStackInternal (true);
        {
            juce::SpinLock::ScopedLockType lk (_dataLock);

            uint8_t pitches[SEQ303_STEP_MAX];
            int n = 0;
            for (int i = 0; i < _data.stepLength; ++i)
                if (! _data.step[i].rest)
                    pitches[n++] = _data.step[i].note;

            int k = 0;
            for (int i = 0; i < SEQ303_STEP_MAX; ++i)
                if (rollRhythmStep (i, fill, accentProbability, slideProbability, tieProbability) && n > 0)
                    _data.step[i].note = pitches[k++ % n];
        }
        setMute (false);
    }

    // =========================================================================
    // ── Harmonizer passthrough ────────────────────────────────────────────────
    // =========================================================================

    void        setTemperament   (uint8_t id)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        _harmonizer.setTemperament (id);
    }
    uint8_t     getTemperamentId()              const { return _harmonizer.getTemperamentId();      }
    const char* getTemperamentName (uint8_t id) const { return _harmonizer.getTemperamentName (id); }
    int         getNumTemperaments()            const { return _harmonizer.getNumTemperaments();    }

    // =========================================================================
    // ── Pattern data access ───────────────────────────────────────────────────
    // =========================================================================

    /** Read-only reference to the full pattern data for UI paint / display.
     *  Individual fields are written atomically field-by-field under the lock,
     *  so a UI read may see a one-step stale value — acceptable for display. */
    const TrackData303& getTrackData() const { return _data; }

    /** Thread-safe snapshot copy of the full pattern (for serialisation). */
    void copyTrackData (TrackData303& dest) const
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        std::memcpy (&dest, &_data, sizeof (TrackData303));
    }

    /** Thread-safe full replace of the pattern (for deserialisation / preset load). */
    void loadTrackData (const TrackData303& src)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        std::memcpy (&_data, &src, sizeof (TrackData303));
    }

    static constexpr size_t trackDataSize() { return sizeof (TrackData303); }

    // =========================================================================
private:
    // =========================================================================

    // Samples per 96-PPQN tick.  Called from setTempo() and prepare().
    // This is the only place timing arithmetic lives; everything else
    // uses _samplesPerTick and _ticksPerStep.
    void updateSamplesPerTick()
    {
        const float bpm = _internalBpm.load (std::memory_order_relaxed);
        if (bpm > 0.f && _sampleRate > 0.0)
            _samplesPerTick = (_sampleRate * 60.0)
                              / (static_cast<double>(bpm) * 96.0);
    }

    // ── Internal clock ────────────────────────────────────────────────────────
    // Event-driven: skip directly to the next tick boundary instead of
    // iterating every sample.  The inner body runs once per 96-PPQN tick
    // (~1920 times/sec at 120 BPM) rather than once per sample (~44100/sec).
    void processInternalClock (int numSamples)
    {
        if (! _running) return;

        _tickSamples = _samplesPerTick;

        int sampleCursor = 0;

        while (sampleCursor < numSamples)
        {
            // Samples remaining until the next tick boundary
            const double samplesUntilTick = _samplesPerTick - _sampleAccum;
            const int    samplesUntilTickI = static_cast<int>(samplesUntilTick);

            if (sampleCursor + samplesUntilTickI >= numSamples)
            {
                // Next tick falls outside this buffer — advance accumulator and stop
                _sampleAccum += static_cast<double>(numSamples - sampleCursor);
                break;
            }

            // Advance to the tick boundary
            sampleCursor  += samplesUntilTickI;
            _sampleAccum   = _sampleAccum + samplesUntilTickI - _samplesPerTick;

            fireTick (_tickCounter, sampleCursor);
            ++_tickCounter;
        }
    }

    // ── Host transport clock ──────────────────────────────────────────────────
    // Event-driven at 96-PPQN-tick resolution: convert ppqPosition to a
    // 96-PPQN tick counter, then jump to each new tick boundary within the
    // buffer.  No per-sample fp math — only one division per tick fired.
    void processHostClock (int    numSamples,
                           bool   hostIsPlaying,
                           double ppqStart,
                           double bpm)
    {
        if (! hostIsPlaying)
        {
            if (_running) stop();
            return;
        }

        if (! _running) return;

        if (bpm > 0.0 && std::isfinite (bpm))
        {
            _internalBpm.store (static_cast<float>(bpm), std::memory_order_relaxed);
            updateSamplesPerTick();
        }
        else
        {
            // Host handed us NaN / 0 / negative tempo — fall back to our cached
            // tempo so the division below can never produce NaN/inf.
            bpm = _internalBpm.load (std::memory_order_relaxed);
            updateSamplesPerTick();
        }

        // Convert buffer start position to 96-PPQN ticks
        // tick = ppq * 96  (96 ticks per quarter note).  Tick boundaries are
        // placed from the exact position, not the floored tick, so gate edges
        // don't jitter with where the buffer starts inside a tick.
        const double  tickPosAtStart = ppqStart * 96.0;

        // Samples per quarter note at current BPM
        const double samplesPerQuarter = (_sampleRate * 60.0) / bpm;
        // Samples per 96-PPQN tick (floored at 1.0 so a degenerate host tempo can
        // never yield sampPerTick = NaN/inf/0 and wedge the loop below)
        const double sampPerTick = std::max (1.0, samplesPerQuarter / 96.0);
        _tickSamples = sampPerTick;

        int sampleCursor = 0;

        while (sampleCursor < numSamples)
        {
            // Which tick does sampleCursor land on?
            const double  offsetInBuffer = static_cast<double>(sampleCursor);
            const int64_t tickNow = static_cast<int64_t>(
                std::floor (tickPosAtStart + offsetInBuffer / sampPerTick));

            if (tickNow <= _lastHostTick)
            {
                // Host jumped BACKWARDS (DAW loop-wrap or seek) while playing:
                // _lastHostTick is frozen at the pre-loop position, so the plain
                // "advance to the next tick" path below would scan for the rest
                // of the buffer, find nothing, and go permanently mute until the
                // transport is stopped and re-started.  Re-anchor to the loop
                // position instead so the pattern continues from there.
                if (tickNow < _lastHostTick)
                {
                    _lastHostTick = tickNow - 1;
                    _tickCounter  = static_cast<uint32_t>(tickNow);
                    _swingRelatch = true;
                    continue; // re-evaluate the same sampleCursor against the new anchor
                }

                // Still inside the same tick — jump to where the next one starts
                const double sampUntilNext = (static_cast<double>(tickNow + 1) - tickPosAtStart) * sampPerTick
                                             - offsetInBuffer;
                const int jump = std::max (1, static_cast<int>(sampUntilNext));

                if (sampleCursor + jump >= numSamples)
                    break;

                sampleCursor += jump;
                continue;
            }

            // New tick(s) have started — fire each one
            int64_t firstNew = _lastHostTick < 0 ? tickNow : _lastHostTick + 1;

            // Runaway position jump (stale _lastHostTick from a previous sync
            // mode, DAW loop-wrap or seek): iterating tens of thousands of stale
            // ticks inside one audio callback would hammer the CPU and flood the
            // note queue.  Re-anchor to the current position instead; the single
            // current tick below keeps us in time.
            const int64_t ticksPerBuffer = std::max<int64_t> (1,
                static_cast<int64_t>(numSamples / sampPerTick) + 2);
            if (tickNow - firstNew >= ticksPerBuffer)
            {
                _lastHostTick = tickNow - 1;
                _tickCounter  = static_cast<uint32_t>(tickNow);
                _swingRelatch = true;
                firstNew      = tickNow;
            }

            for (int64_t t = firstNew; t <= tickNow; ++t)
            {
                // Sample offset of tick t within this buffer
                const double tickSampleF = (static_cast<double>(t) - tickPosAtStart) * sampPerTick;
                fireTick (static_cast<uint32_t>(t),
                          juce::jlimit (0.0, static_cast<double>(numSamples - 1), tickSampleF));
            }

            _lastHostTick  = tickNow;
            _tickCounter   = static_cast<uint32_t>(tickNow);

            // Advance to where the next tick begins
            const double nextTickSampleF = (static_cast<double>(tickNow + 1) - tickPosAtStart) * sampPerTick;
            const int    nextTickSample  = static_cast<int>(std::ceil (nextTickSampleF));
            if (nextTickSample >= numSamples) break;
            sampleCursor = nextTickSample;
        }
    }

    // ── MIDI clock ────────────────────────────────────────────────────────────
    // MIDI clock = 24 PPQN.  Our internal resolution is 96 PPQN ticks, so
    // each incoming MIDI clock message = 4 ticks, spread over the measured
    // clock period so gates and swing keep 96-PPQN resolution.  The first
    // clock after a start has no period yet and fires its 4 ticks at once.
    void tickMidiClock (int samplePos)
    {
        static constexpr int kTicksPerMidiClock = 4;

        const int64_t now      = _blockStart + samplePos;
        const int64_t interval = now - _lastMidiClock;
        _tickSamples   = (_lastMidiClock >= 0 && interval > 0 && static_cast<double>(interval) < _sampleRate)
                             ? static_cast<double>(interval) / kTicksPerMidiClock : 0.0;
        _lastMidiClock = now;

        for (int i = 0; i < kTicksPerMidiClock; ++i)
        {
            fireTick (_tickCounter, samplePos + i * _tickSamples);
            ++_tickCounter;
        }
    }

    // =========================================================================
    // ── Core tick dispatcher (96-PPQN resolution) ────────────────────────────
    // =========================================================================
    //
    // All timing is expressed in 96-PPQN ticks — matching the original engine.
    //
    //   _ticksPerStep = 24   (one 16th-note = 24 ticks at 96 PPQN, always)
    //
    // fireTick() is called once per 96-PPQN tick boundary (not per sample).
    // At 120 BPM / 44100 Hz that is ~1920 calls/sec instead of 44100/sec.
    // =========================================================================

    inline void fireTick (uint32_t tick, double sampleOffset)
    {
        // Note-off stack: check every tick (1-pulse resolution, same as original).
        // Count down before the step fires, so a note started on this tick
        // keeps its full length (12 ticks = 50%) and a release on this tick
        // is sent before the new NoteOn.
        onTick (sampleOffset);

        // Step boundary: every _ticksPerStep (= 24) ticks; swung steps fire late,
        // the whole ticks on the clock and the fraction as a sample offset.
        // Swing is latched per pair of 16ths so a change can't skip or repeat a
        // step, and on (re)start so a start mid-pair uses the current value.
        const uint32_t step = tick / static_cast<uint32_t>(_ticksPerStep);
        if (_swingRelatch || tick % (2u * static_cast<uint32_t>(_ticksPerStep)) == 0)
        {
            _swingLatched = (_swingPercent.load (std::memory_order_relaxed) - 50.0) / 100.0
                            * 2.0 * _ticksPerStep;
            _swingRelatch = false;
        }
        const double delay = swingDelay (step);
        if (static_cast<int>(tick % static_cast<uint32_t>(_ticksPerStep)) == static_cast<int>(std::floor (delay)))
            onStep (step, sampleOffset, delay - std::floor (delay));
    }

    // ── onStep — fires once per 16th-note step (every 24 ticks) ──────────────
    void onStep (uint32_t stepTick, double sampleOffset, double onsetFrac)
    {
        const uint8_t stepLen = _data.stepLength;

        // Reverse playback runs last step to first
        const bool    rev     = _reversePlay.load (std::memory_order_relaxed);
        const uint8_t fwdPos  = static_cast<uint8_t>(stepTick % stepLen);
        const uint8_t stepPos = rev ? static_cast<uint8_t>(stepLen - 1 - fwdPos) : fwdPos;
        const auto    isTied  = [this] (uint8_t p) { return _data.step[p].rest && _data.step[p].tie; };

        // A tied section whose effective direction is the opposite of the pattern's (a reversed
        // note played forward, or a flagged note under reverse playback) mirrors the step
        // indicator across the section (see the trigger below); the rest of its steps continue it.
        // Section steps are N..N+K; the pattern visits them N+J (forward) or N+K-J (reverse).
        uint8_t shownPos = stepPos;
        if (_mirrorLeft > 0)
        {
            const uint8_t expected = static_cast<uint8_t>(
                (rev ? _mirrorBase + _mirrorK - _mirrorJ : _mirrorBase + _mirrorJ) % stepLen);
            if (rev == _mirrorRev && stepPos == expected)
            {
                shownPos = static_cast<uint8_t>(
                    (rev ? _mirrorBase + _mirrorJ : _mirrorBase + _mirrorK - _mirrorJ) % stepLen);
                ++_mirrorJ;
                --_mirrorLeft;
            }
            else
            {
                _mirrorLeft = 0;
            }
        }
        _stepLocation.store (shownPos, std::memory_order_relaxed);

        if (_mute) return;

        // ── Gate end in 96-PPQN ticks: slide / tie lookahead ─────────────────
        // A gate ends _noteLengthTicks (12 = 50%) after the onset of the step
        // it ends in; each tied rest moves that step on by one.  A slide
        // belongs to the note, as on the TB-303, so it carries across tied
        // rests and ends a fixed overhang (8 ticks) after the next played
        // note's onset.  Measured from real (swung) onsets, so a swung gate
        // never overruns the next note or a slide target.
        //
        // The step whose note is played is srcPos; in reverse a tied run is
        // played from its far end, starting the anchor note that holds through
        // the run.
        const uint8_t lookaheadBound = static_cast<uint8_t>(stepLen);
        const int32_t slideTail      = _slideExtraTicks - (_ticksPerStep - _noteLengthTicks);
        uint8_t       srcPos         = stepPos;
        uint32_t      endStep        = stepTick;   // clock step the gate ends in
        int32_t       tail           = _noteLengthTicks;
        uint8_t       tiedRests      = 0;   // forward: tied rests held by this note

        if (! rev)
        {
            const StepData303& at = _data.step[stepPos];
            if (at.rest) return;

            uint8_t nextStep = stepPos;
            for (uint8_t i = 1; i <= lookaheadBound; ++i)
            {
                nextStep = static_cast<uint8_t>((nextStep + 1) % lookaheadBound);
                const StepData303& next = _data.step[nextStep];

                if (next.rest && next.tie)
                {
                    endStep   = stepTick + i;
                    tiedRests = i;
                    continue;
                }

                if ((at.slide || at.hammer) && ! next.rest)
                {
                    endStep = stepTick + i;
                    tail    = slideTail;
                }
                break;
            }
        }
        else
        {
            const uint8_t after = static_cast<uint8_t>((stepPos + 1) % lookaheadBound);
            const uint8_t prior = static_cast<uint8_t>((stepPos + lookaheadBound - 1) % lookaheadBound);

            if (isTied (stepPos) && ! isTied (after))
            {
                // Played first in reverse: the far end of a tied run starts the anchor note,
                // held through every tied rest.
                uint8_t  p = stepPos;
                uint32_t k = 0;
                while (isTied (p) && k < lookaheadBound)
                {
                    p = static_cast<uint8_t>((p + lookaheadBound - 1) % lookaheadBound);
                    ++k;
                }
                if (_data.step[p].rest) return;
                srcPos    = p;
                endStep   = stepTick + k;
                tiedRests = static_cast<uint8_t>(k);
            }
            else if (_data.step[stepPos].rest || isTied (after))
            {
                return;   // plain rest, mid-run tied rest, or a note already played by its run
            }
            else if (! _data.step[prior].rest
                     && (_data.step[prior].slide || _data.step[prior].hammer))
            {
                // legato flags sit on the forward sender, which is the next note played
                endStep = stepTick + 1;
                tail    = slideTail;
            }
        }

        const StepData303& cur = _data.step[srcPos];

        // A muted note is choked: shorten the plain gate (legato/tie extensions stay intact)
        if (cur.mute && endStep == stepTick)
            tail = std::max (1, static_cast<int32_t>(tail * SEQ303_MUTE_GATE_FACTOR));

        // Ticks from the tick this step fires on (whole part of its swing) to the release
        const double gateTicks = static_cast<double>(endStep - stepTick) * _ticksPerStep
                                 + swingDelay (endStep) - std::floor (swingDelay (stepTick)) + tail;
        const int32_t gateLength = std::max (1, static_cast<int32_t>(std::floor (gateTicks)));
        const double  offFrac    = std::max (0.0, gateTicks - gateLength);

        // A flagged note held by tied rests plays its section the opposite way round: forward
        // playback shows the indicator running from the section's end back to the note, reverse
        // playback (reverse of reverse) from the note forward to the section's end.
        if (cur.reverse && tiedRests > 0)
        {
            _mirrorBase = srcPos;
            _mirrorK    = tiedRests;
            _mirrorJ    = 1;
            _mirrorLeft = tiedRests;   // the K steps that follow the trigger step
            _mirrorRev  = rev;
            _stepLocation.store (rev ? srcPos
                                     : static_cast<uint8_t>((srcPos + tiedRests) % stepLen),
                                 std::memory_order_relaxed);
        }

        // ── Note resolution ───────────────────────────────────────────────────
        const int pitchStep = _data.pitchLength != 0 ? resolvePitchStep (stepTick - fwdPos + srcPos, stepLen) : srcPos;
        _pitchPlayhead.store (pitchStep, std::memory_order_relaxed);
        uint8_t note = _data.step[pitchStep].note;
        if (_data.tune > 0)
            note = _harmonizer.harmonize (note) + static_cast<uint8_t>(_data.tune - 1);

        // ── Same-pitch handover ──────────────────────────────────────────────
        // A slide or wrap-around overlap into the same pitch is one gate, as on
        // the TB-303.  Drop the earlier entry without a NoteOff; otherwise its
        // release would land after this NoteOn and end the new note early.
        for (int i = 0; i < SEQ303_NOTE_STACK_SIZE; ++i)
            if (_stack[i].length > 0 && _stack[i].note == note)
                _stack[i].length = -1;

        // ── Place in note stack (gate length in 96-PPQN ticks) ───────────────
        for (int i = 0; i < SEQ303_NOTE_STACK_SIZE; ++i)
        {
            if (_stack[i].length == -1)
            {
                _stack[i].note    = note;
                _stack[i].length  = gateLength;
                _stack[i].offFrac = offFrac;
                send (Acid303EventType::NoteOn,
                      note,
                      (cur.accent && ! cur.mute) ? SEQ303_ACCENT_VELOCITY : SEQ303_NOTE_VELOCITY,
                      sampleOffset + onsetFrac * _tickSamples,
                      cur.mute,
                      cur.reverse != rev,
                      rev ? cur.hammer : _prevHammer,
                      gateTicks * _tickSamples);
                _prevHammer = cur.hammer;
                break;
            }
        }
    }

    // ── onTick — fires every 96-PPQN tick; decrements note stack ─────────────
    // Replaces onPulse(). The countdown unit is now ticks (= pulses at 96 PPQN)
    // so all NOTE_LENGTH values remain identical to the original engine.
    void onTick (double sampleOffset)
    {
        for (int i = 0; i < SEQ303_NOTE_STACK_SIZE; ++i)
        {
            if (_stack[i].length > 0)
            {
                --_stack[i].length;
                if (_stack[i].length == 0)
                {
                    send (Acid303EventType::NoteOff,
                          _stack[i].note,
                          0,
                          sampleOffset + _stack[i].offFrac * _tickSamples);
                    _stack[i].length = -1;
                }
            }
        }
    }

    // =========================================================================
    // ── Utility ──────────────────────────────────────────────────────────────
    // =========================================================================

    /** Step whose pitch the note step at clock step `stepTick` plays with
     *  Pitch Length on.  The pitch list is the note steps in the active
     *  length, in order; the note steps cycle through its first N entries,
     *  counted from the play position across loops. */
    int resolvePitchStep (uint32_t stepTick, uint8_t stepLen) const
    {
        const uint8_t stepPos = static_cast<uint8_t>(stepTick % stepLen);

        uint8_t noteSteps[SEQ303_STEP_MAX];
        int n = 0, k = 0;
        for (uint8_t i = 0; i < stepLen; ++i)
        {
            if (_data.step[i].rest) continue;
            if (i == stepPos) k = n;
            noteSteps[n++] = i;
        }

        const int     len   = juce::jmin<int> (_data.pitchLength, n);
        const int64_t index = static_cast<int64_t>(stepTick / stepLen) * n + k;
        return noteSteps[index % len];
    }

    inline void emit (const Acid303Event& ev) const
    {
        if (onNoteEvent) onNoteEvent (ev);
    }

    /** Delay of a clock step in ticks: off-beat 16ths are swung. */
    double swingDelay (uint32_t step) const
    {
        return (step & 1u) ? _swingLatched : 0.0;
    }

    /** Queue an event at `sampleOffset` (may fall past this block), or on the
     *  CPU-timing interrupt grid. */
    void send (Acid303EventType type, uint8_t note, uint8_t velocity, double sampleOffset,
               bool mute = false, bool reverse = false, bool hammerInto = false,
               double gateSamples = 0.0)
    {
        const double t = static_cast<double>(_blockStart) + sampleOffset;
        int64_t      at;
        if (_cpuTiming.load (std::memory_order_relaxed))
        {
            const double msToSamples = _sampleRate / 1000.0;
            const double period      = kIrqPeriodMs * msToSamples;
            const double latency     = (type == Acid303EventType::NoteOn ? kGateOnLatencyMs
                                                                        : kGateOffLatencyMs) * msToSamples;
            at = static_cast<int64_t>(std::llround (std::ceil (t / period) * period + latency));
        }
        else
        {
            at = static_cast<int64_t>(std::llround (t));
        }

        // A release sent before this NoteOn never lands after it: the longer
        // NoteOff latency would otherwise turn the next note into a slide.
        if (type == Acid303EventType::NoteOn)
            for (int i = 0; i < _queueCount; ++i)
                if (_queue[i].ev.type == Acid303EventType::NoteOff && _queue[i].at > at)
                    _queue[i].at = at;

        const Acid303Event ev { type, note, velocity, 0, mute, reverse, hammerInto, gateSamples };
        if (_queueCount == kQueueSize)
        {
            emit ({ type, note, velocity,
                    static_cast<int>(juce::jlimit<int64_t> (0, std::max (0, _blockLength - 1), at - _blockStart)) });
            return;
        }
        _queue[_queueCount++] = { ev, at };
    }

    /** Emit queued events that fall inside the current block, in time order
     *  (send order for events at the same sample). */
    void flushQueue (int numSamples)
    {
        const int64_t blockEnd = _blockStart + numSamples;
        QueuedEvent   due[kQueueSize];
        int           numDue = 0;
        int           kept   = 0;
        for (int i = 0; i < _queueCount; ++i)
        {
            const auto q = _queue[i];
            if (q.at < blockEnd)
            {
                int j = numDue++;
                for (; j > 0 && due[j - 1].at > q.at; --j)
                    due[j] = due[j - 1];
                due[j] = q;
            }
            else
            {
                _queue[kept++] = q;
            }
        }
        _queueCount = kept;

        for (int i = 0; i < numDue; ++i)
        {
            auto ev = due[i].ev;
            ev.sampleOffset = static_cast<int>(std::max<int64_t> (0, due[i].at - _blockStart));
            emit (ev);
        }
    }

    /** Must be called with _dataLock already held. */
    void clearStepDataInternal (bool fillWithRests)
    {
        for (int i = 0; i < SEQ303_STEP_MAX; ++i)
        {
            _data.step[i].note   = SEQ303_DEFAULT_NOTE;
            _data.step[i].rest   = fillWithRests;
            _data.step[i].accent = false;
            _data.step[i].slide  = false;
            _data.step[i].tie    = false;
            _data.step[i].mute   = false;
            _data.step[i].hammer = false;
            _data.step[i].reverse = false;
        }
    }

    /** Send Note Off for all active stack entries, then clear the stack.
     *  Safe to call from either thread. */
    void clearStackInternal (bool sendNoteOffs)
    {
        bool sentAny = false;
        for (int i = 0; i < _queueCount; ++i)
        {
            if (sendNoteOffs && _queue[i].ev.type == Acid303EventType::NoteOff)
            {
                emit ({ Acid303EventType::NoteOff, _queue[i].ev.note, 0, 0 });
                sentAny = true;
            }
        }
        _queueCount = 0;
        for (int i = 0; i < SEQ303_NOTE_STACK_SIZE; ++i)
        {
            if (_stack[i].length != -1)
            {
                if (sendNoteOffs)
                {
                    emit ({ Acid303EventType::NoteOff, _stack[i].note, 0, 0 });
                    sentAny = true;
                }
                _stack[i].length = -1;
            }
        }
        // Notify JC303 to reset its held-note tracking whenever we silence the stack
        if (sendNoteOffs && sentAny && onAllNotesOff)
            onAllNotesOff();
    }

    using Steps = std::array<StepData303, SEQ303_STEP_MAX>;

    /** Push the current steps onto `stack`, dropping the oldest when full.
     *  Must be called with _dataLock held. */
    void pushSnapshot (std::array<Steps, kUndoDepth>& stack, std::atomic<int>& count)
    {
        int n = count.load (std::memory_order_relaxed);
        if (n == kUndoDepth)
        {
            std::move (stack.begin() + 1, stack.end(), stack.begin());
            --n;
        }
        std::copy (std::begin (_data.step), std::end (_data.step), stack[n].begin());
        count.store (n + 1, std::memory_order_relaxed);
    }

    /** Pop `from`, saving the current steps onto `to`; silences held notes. */
    bool swapHistory (std::array<Steps, kUndoDepth>& from, std::atomic<int>& fromCount,
                      std::array<Steps, kUndoDepth>& to,   std::atomic<int>& toCount)
    {
        if (fromCount.load (std::memory_order_relaxed) == 0)
            return false;

        setMute (true);
        clearStackInternal (true);
        {
            juce::SpinLock::ScopedLockType lk (_dataLock);
            pushSnapshot (to, toCount);
            const int n = fromCount.load (std::memory_order_relaxed) - 1;
            std::copy (from[n].begin(), from[n].end(), std::begin (_data.step));
            fromCount.store (n, std::memory_order_relaxed);
        }
        setMute (false);
        return true;
    }

    /** Chromatic-quantisation correction table (mirrors fix_tones[]). */
    static void makeFixTones (uint8_t numberOfTones, int8_t (&fixTones)[12])
    {
        std::fill (std::begin (fixTones), std::end (fixTones), int8_t { 0 });
        if (numberOfTones == 0)
            return;

        const int8_t fixMod = static_cast<int8_t>(12 / numberOfTones);
        int8_t fixCounter   = 0;
        for (uint8_t i = 0; i < 12; ++i)
        {
            if ((i % fixMod) == 0 || i == 0)
            {
                fixTones[i] = 0;
                fixCounter  = 0;
            }
            else
            {
                --fixCounter;
                fixTones[i] = fixCounter;
            }
        }
    }

    /** Random note in [lowerNote, lowerNote + rangeNote), snapped to the tone grid. */
    uint8_t randomNote (const int8_t (&fixTones)[12], uint8_t numberOfTones,
                        uint8_t lowerNote, uint8_t rangeNote)
    {
        const uint8_t highNote = static_cast<uint8_t>(
            juce::jlimit (0, 127, static_cast<int>(lowerNote) + rangeNote));

        const uint8_t noteRange = (highNote > lowerNote)
                                    ? static_cast<uint8_t>(highNote - lowerNote)
                                    : 1;

        uint8_t note = static_cast<uint8_t>(lowerNote + (randPercent() % noteRange));

        if (numberOfTones > 0)
        {
            const int fixed = static_cast<int>(note) + fixTones[note % 12];
            note = static_cast<uint8_t>(juce::jlimit (0, 127, fixed));
        }
        return note;
    }

    /** Roll one step's rest / tie / accent / slide.  Returns true for a note
     *  step.  A tie is only possible on a rest after a note or a tie.
     *  Must be called with _dataLock held. */
    bool rollRhythmStep (int i, uint8_t fill, uint8_t accentProbability,
                         uint8_t slideProbability, uint8_t tieProbability)
    {
        auto& st = _data.step[i];
        const bool isOn = (randPercent() < fill);
        st.rest   = ! isOn;
        st.tie    = false;
        st.mute   = false;
        st.hammer = false;
        st.reverse = false;

        if (! isOn)
        {
            st.accent = false;
            st.slide  = false;
            const auto& last = _data.step[(i == 0) ? SEQ303_STEP_MAX - 1 : i - 1];
            if (! last.rest || last.tie)
                st.tie = (randPercent() < tieProbability);
            return false;
        }

        st.accent = (randPercent() < accentProbability);
        st.slide  = (randPercent() < slideProbability);
        return true;
    }

    /** Returns a pseudo-random integer 0–99 inclusive. */
    inline uint8_t randPercent()
    {
        return static_cast<uint8_t>(_rng() % 100u);
    }

    // =========================================================================
    // ── Members ───────────────────────────────────────────────────────────────
    // =========================================================================

    // Pattern data — UI-thread writes under _dataLock, audio-thread reads
    TrackData303           _data;
    mutable juce::SpinLock _dataLock;

    // Note stack — audio thread only, no lock needed
    NoteStackEntry303      _stack[SEQ303_NOTE_STACK_SIZE];

    // Hammer flag of the last played step: its overlap into the next NoteOn snaps pitch
    bool                   _prevHammer { false };

    // Current step — written on audio thread; atomic for UI read
    std::atomic<uint8_t>   _stepLocation { 0 };

    // Mute — settable from any thread
    std::atomic<bool>      _mute { false };
    std::atomic<bool>      _reversePlay { false };
    // mirrored step-indicator run for a reversed tied section (audio thread only)
    uint8_t                _mirrorBase { 0 };
    uint8_t                _mirrorK    { 0 };
    uint8_t                _mirrorJ    { 0 };
    uint8_t                _mirrorLeft { 0 };
    bool                   _mirrorRev  { false };

    // Rec state + cursor — UI and audio threads both touch them (all updates
    // are single-word atomic; the advance is guarded by _dataLock in recNote/recRest)
    std::atomic<bool>      _recOn   { false };
    std::atomic<uint8_t>   _recStep { 0 };

    // Harmonizer — writes under _dataLock, reads on audio thread
    Harmonizer             _harmonizer;

    // Sync / start mode — set from the UI thread, read on the audio thread
    std::atomic<SyncMode>    _syncMode  { SyncMode::Internal };
    std::atomic<StartMode>   _startMode { StartMode::TransportStart };

    // Audio-thread-only mirror of _syncMode.  Used by processBlock to detect
    // a sync-mode change and reset transport/clock state on the audio thread.
    SyncMode                 _lastMode  { SyncMode::Internal };

    // Transport
    std::atomic<bool>      _running { false };

    // Clock parameters (set in prepare(), updated in setTempo())
    double                 _sampleRate      { 44100.0 };
    double                 _ppqn            { 960.0   };
    double                 _pulsesPerStep   { 240.0   };   // ppqn / 4 (kept for reference)
    double                 _ticksPerPulse   { 10.0    };   // ppqn / 96
    int                    _ticksPerStep    { 24      };   // always 24 at 96-PPQN resolution

    // Internal clock — event-driven accumulator (advances by samplesPerTick each tick)
    // _samplesPerTick is written only on the audio thread (see processBlock).
    double                 _samplesPerTick  { 0.0     };   // sampleRate*60 / (bpm*96)
    double                 _sampleAccum     { 0.0     };   // fractional sample accumulator
    uint32_t               _tickCounter     { 0       };   // monotonic 96-PPQN tick counter

    // Host sync — last 96-PPQN tick seen from AudioPlayHead
    int64_t                _lastHostTick    { -1 };

    // Note lengths in 96-PPQN ticks (identical numbers to original engine_303)
    int32_t                _noteLengthTicks { 0 };   // = 12 ticks = 50% gate
    int32_t                _slideExtraTicks { 0 };   // = 20 ticks

    std::atomic<float>     _internalBpm { 120.f };

    // Absolute sample clock and events waiting for their sample (swing
    // fraction, spread MIDI clock ticks) or CPU-timing interrupt
    static constexpr double kIrqPeriodMs      = 1.8;
    static constexpr double kGateOnLatencyMs  = 1.0;
    static constexpr double kGateOffLatencyMs = 2.35;
    static constexpr int    kQueueSize        = 32;
    struct QueuedEvent { Acid303Event ev; int64_t at; };

    // Pattern undo / redo history (audio thread writes, UI reads the counts)
    std::array<Steps, kUndoDepth> _undo {};
    std::array<Steps, kUndoDepth> _redo {};
    std::atomic<int>       _undoCount { 0 };
    std::atomic<int>       _redoCount { 0 };

    std::atomic<int>       _pitchPlayhead { -1 }; // step whose pitch the last note used (display)

    std::atomic<bool>      _cpuTiming  { false };
    std::atomic<float>     _swingPercent { 50.f };
    double                 _swingLatched { 0.0 };   // audio thread: off-beat delay in ticks for the current pair of 16ths
    bool                   _swingRelatch { true };  // audio thread: latch swing on the next tick
    double                 _tickSamples { 0.0 };    // audio thread: samples per tick of the active clock
    int64_t                _lastMidiClock { -1 };   // absolute sample of the last MIDI clock
    int64_t                _blockStart { 0 };
    int                    _blockLength { 0 };
    QueuedEvent            _queue[kQueueSize] {};
    int                    _queueCount { 0 };

    // RNG (portable std::mt19937 replaces Arduino random())
    std::mt19937           _rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidSequencer303)
};
