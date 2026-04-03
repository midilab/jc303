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
static constexpr uint8_t  SEQ303_DEFAULT_NOTE      = 36;   // C2

// NOTE_LENGTH_303 = 12 pulses at 96 PPQN  →  12/24 = 50% gate (original TB-303).
// The comment in engine_303.h says explicitly: "12 = 50% on 96ppqn, same as original tb303".
// Slide extra = 20 pulses at 96 PPQN (extends the gate slightly past the next note-on).
// Both are stored as ratios so they scale correctly at any host PPQN.
static constexpr double SEQ303_NOTE_LENGTH_RATIO = 12.0 / 24.0;  // 50% gate
static constexpr double SEQ303_SLIDE_EXTRA_RATIO = 20.0 / 24.0;  // slide overhang

// =============================================================================
// Data structures
// =============================================================================
struct StepData303
{
    uint8_t note   { SEQ303_DEFAULT_NOTE };
    bool    rest   { true  };
    bool    accent { false };
    bool    slide  { false };
    bool    tie    { false };
};

struct NoteStackEntry303
{
    uint8_t  note   { 0  };
    int32_t  length { -1 };   // -1 = free slot; positive = pulse countdown
};

/** Full pattern data for one track (serialisable, UI-readable). */
struct TrackData303
{
    StepData303 step[SEQ303_STEP_MAX];
    int8_t      shift      { 0 };
    uint8_t     stepLength { SEQ303_STEP_MAX };
    int8_t      transpose  { 0 };
    uint8_t     tune       { 0 };  // 0=off; 1+N = harmonize + N semitone offset
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
     *  acidRandomize).  JC303 uses this to reset _heldNote / _lastStepHadSlide so
     *  Open303 receives a clean all-notes-off and no hung notes are left behind. */
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
            _data.shift      = 0;
            _data.stepLength = SEQ303_STEP_MAX;
            _data.transpose  = 0;
            _data.tune       = 0;
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
    }

    // =========================================================================
    // Sync / Start modes
    // =========================================================================

    void      setSyncMode  (SyncMode  m) { _syncMode  = m; }
    void      setStartMode (StartMode m) { _startMode = m; }
    SyncMode  getSyncMode()  const       { return _syncMode;  }
    StartMode getStartMode() const       { return _startMode; }

    // =========================================================================
    // Internal tempo (SyncMode::Internal)
    // =========================================================================

    void  setTempo (float bpm)
    {
        _internalBpm = juce::jlimit (20.f, 300.f, bpm);
        updateSamplesPerTick();
    }
    float getTempo() const { return _internalBpm; }

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
        _running      = true;
    }

    void stop()
    {
        if (! _running) return;
        _running = false;
        clearStackInternal (true);
    }

    bool isRunning() const { return _running.load(); }

    // =========================================================================
    // processBlock — called every buffer from JC303::processBlock
    // =========================================================================
    /**
     * @param midiIn            Incoming MidiBuffer (MIDI sync + note-trigger).
     * @param numSamples        Buffer length in samples.
     * @param hostIsPlaying     AudioPlayHead::isPlaying.
     * @param ppqAtBufferStart  ppqPosition at sample 0 (Host sync).
     * @param bpm               Current host BPM (Host sync; ignored in Internal).
     * @param shufflePulses     Signed pulse offset applied to gate length.
     */
    void processBlock (const juce::MidiBuffer& midiIn,
                       int    numSamples,
                       bool   hostIsPlaying,
                       double ppqAtBufferStart,
                       double bpm,
                       int    shufflePulses = 0)
    {
        // ── MIDI transport + note-triggered start ─────────────────────────────
        for (const auto meta : midiIn)
        {
            const auto msg       = meta.getMessage();
            const int  samplePos = meta.samplePosition;

            if (_syncMode == SyncMode::MidiClock)
            {
                if (msg.isMidiStart())
                {
                    _tickCounter  = 0;
                    _sampleAccum  = 0.0;
                    _lastHostTick = -1;
                    if (_startMode == StartMode::TransportStart)
                        start();
                }
                else if (msg.isMidiStop())
                {
                    stop();
                }
                else if (msg.isMidiClock() && _running)
                {
                    tickMidiClock (samplePos, shufflePulses);
                }
            }

            if (_startMode == StartMode::NoteTriggered && ! _running && msg.isNoteOn())
                start();
        }

        // ── Clock advancement ─────────────────────────────────────────────────
        switch (_syncMode)
        {
            case SyncMode::Internal:
                processInternalClock (numSamples, shufflePulses);
                break;

            case SyncMode::Host:
                processHostClock (numSamples, hostIsPlaying, ppqAtBufferStart, bpm, shufflePulses);
                break;

            case SyncMode::MidiClock:
                break; // handled above per-message
        }
    }

    // =========================================================================
    // ── Per-step setters / getters — UI-thread-safe ───────────────────────────
    // =========================================================================

    /** rest(step, state) — mirrors Engine303::rest() */
    void setRest   (int step, bool v) { juce::SpinLock::ScopedLockType lk(_dataLock); _data.step[step].rest   = v; }
    /** setAccent(step, state) */
    void setAccent (int step, bool v) { juce::SpinLock::ScopedLockType lk(_dataLock); _data.step[step].accent = v; }
    /** setSlide(step, state) */
    void setSlide  (int step, bool v) { juce::SpinLock::ScopedLockType lk(_dataLock); _data.step[step].slide  = v; }
    /** setTie(step, state) */
    void setTie    (int step, bool v) { juce::SpinLock::ScopedLockType lk(_dataLock); _data.step[step].tie    = v; }

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

    // =========================================================================
    // ── Track-level getters / setters — mirrors engine_303 API ───────────────
    // =========================================================================

    /** setShiftPos / getShiftPos */
    void   setShiftPos (int8_t shift)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        _data.shift = shift;
    }
    int8_t getShiftPos() const { return _data.shift; }

    /** setTune / getTune
     *  0   = harmonizer off (raw MIDI note)
     *  1+N = harmonize note then add N semitones of offset */
    void    setTune (uint8_t tune)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        _data.tune = tune;
    }
    uint8_t getTune() const { return _data.tune; }

    /** setTranspose / getTranspose — global semitone offset applied at note-on */
    void   setTranspose (int8_t t)
    {
        juce::SpinLock::ScopedLockType lk (_dataLock);
        _data.transpose = t;
    }
    int8_t getTranspose() const { return _data.transpose; }

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
        // Build the chromatic-quantisation correction table (mirrors fix_tones[])
        int8_t fixTones[12] = {};
        if (numberOfTones > 0)
        {
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

        // Clear the track: mutes, sends NoteOff for any held note (fires
        // onAllNotesOff so JC303 resets _heldNote), clears all steps, then unmutes.
        clearTrack();

        {
            juce::SpinLock::ScopedLockType lk (_dataLock);

            for (int i = 0; i < SEQ303_STEP_MAX; ++i)
            {
                const bool isOn = (randPercent() < fill);
                _data.step[i].rest = ! isOn;

                if (! isOn)
                {
                    // REST step
                    _data.step[i].accent = false;
                    _data.step[i].slide  = false;
                    _data.step[i].tie    = false;

                    // Tie: only probable when previous step had a note or a tie
                    const int lastIdx = (i == 0) ? SEQ303_STEP_MAX - 1 : i - 1;
                    if (! _data.step[lastIdx].rest || _data.step[lastIdx].tie)
                        _data.step[i].tie = (randPercent() < tieProbability);

                    continue;
                }

                // ON step — pick a note
                const uint8_t highNote = static_cast<uint8_t>(
                    juce::jlimit (0, 127, static_cast<int>(lowerNote) + rangeNote));

                const uint8_t noteRange = (highNote > lowerNote)
                                            ? static_cast<uint8_t>(highNote - lowerNote)
                                            : 1;

                uint8_t note = static_cast<uint8_t>(
                    lowerNote + (randPercent() % noteRange));

                if (numberOfTones > 0)
                {
                    const int fixed = static_cast<int>(note) + fixTones[note % 12];
                    note = static_cast<uint8_t>(juce::jlimit (0, 127, fixed));
                }

                _data.step[i].note   = note;
                _data.step[i].accent = (randPercent() < accentProbability);
                _data.step[i].slide  = (randPercent() < slideProbability);
                _data.step[i].tie    = false;
            }
        }

        // clearTrack() already called setMute(false) — no need to unmute again.
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
        if (_internalBpm > 0.f && _sampleRate > 0.0)
            _samplesPerTick = (_sampleRate * 60.0)
                              / (static_cast<double>(_internalBpm) * 96.0);
    }

    // ── Internal clock ────────────────────────────────────────────────────────
    // Event-driven: skip directly to the next tick boundary instead of
    // iterating every sample.  The inner body runs once per 96-PPQN tick
    // (~1920 times/sec at 120 BPM) rather than once per sample (~44100/sec).
    void processInternalClock (int numSamples, int shufflePulses)
    {
        if (! _running) return;

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

            fireTick (_tickCounter, sampleCursor, shufflePulses);
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
                           double bpm,
                           int    shufflePulses)
    {
        if (! hostIsPlaying)
        {
            if (_running) stop();
            return;
        }

        if (! _running && _startMode == StartMode::TransportStart)
            start();

        if (! _running) return;

        if (bpm > 0.0)
        {
            _internalBpm = static_cast<float>(bpm);
            updateSamplesPerTick();
        }

        // Convert buffer start position to 96-PPQN ticks
        // tick = ppq * 96  (96 ticks per quarter note)
        const int64_t tickAtStart = static_cast<int64_t>(ppqStart * 96.0);

        // Samples per quarter note at current BPM
        const double samplesPerQuarter = (_sampleRate * 60.0) / bpm;
        // Samples per 96-PPQN tick
        const double sampPerTick = samplesPerQuarter / 96.0;

        int sampleCursor = 0;

        while (sampleCursor < numSamples)
        {
            // Which tick does sampleCursor land on?
            const double  offsetInBuffer = static_cast<double>(sampleCursor);
            const int64_t tickNow = tickAtStart
                                    + static_cast<int64_t>(offsetInBuffer / sampPerTick);

            if (tickNow <= _lastHostTick)
            {
                // Still inside the same tick — jump to where the next one starts
                const double sampUntilNext = sampPerTick
                    - (offsetInBuffer - static_cast<double>(tickNow - tickAtStart) * sampPerTick);
                const int jump = std::max (1, static_cast<int>(sampUntilNext));

                if (sampleCursor + jump >= numSamples)
                    break;

                sampleCursor += jump;
                continue;
            }

            // New tick(s) have started — fire each one
            const int64_t firstNew = _lastHostTick < 0 ? tickNow : _lastHostTick + 1;
            for (int64_t t = firstNew; t <= tickNow; ++t)
            {
                // Sample offset of tick t within this buffer
                const double tickSampleF = static_cast<double>(t - tickAtStart) * sampPerTick;
                const int    tickSample  = juce::jlimit (0, numSamples - 1,
                                               static_cast<int>(tickSampleF));
                fireTick (static_cast<uint32_t>(t), tickSample, shufflePulses);
            }

            _lastHostTick  = tickNow;
            _tickCounter   = static_cast<uint32_t>(tickNow);

            // Advance to where the next tick begins
            const double nextTickSampleF = static_cast<double>(tickNow + 1 - tickAtStart) * sampPerTick;
            const int    nextTickSample  = static_cast<int>(nextTickSampleF);
            if (nextTickSample >= numSamples) break;
            sampleCursor = nextTickSample;
        }
    }

    // ── MIDI clock ────────────────────────────────────────────────────────────
    // MIDI clock = 24 PPQN.  Our internal resolution is 96 PPQN ticks, so
    // each incoming MIDI clock message = 4 ticks.  Integer math, no fractions.
    void tickMidiClock (int samplePos, int shufflePulses)
    {
        // 96 / 24 = 4 ticks per MIDI clock message — always exact integer
        static constexpr int kTicksPerMidiClock = 4;

        for (int i = 0; i < kTicksPerMidiClock; ++i)
        {
            fireTick (_tickCounter, samplePos, shufflePulses);
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

    inline void fireTick (uint32_t tick, int sampleOffset, int shufflePulses)
    {
        // Step boundary: every _ticksPerStep (= 24) ticks
        if ((tick % static_cast<uint32_t>(_ticksPerStep)) == 0)
            onStep (tick / static_cast<uint32_t>(_ticksPerStep), sampleOffset, shufflePulses);

        // Note-off stack: check every tick (1-pulse resolution, same as original)
        onTick (sampleOffset);
    }

    // ── onStep — fires once per 16th-note step (every 24 ticks) ──────────────
    void onStep (uint32_t stepTick, int sampleOffset, int shufflePulses)
    {
        const uint8_t stepLen = _data.stepLength;

        // Circular step position with shift
        const uint8_t stepPos = static_cast<uint8_t>(
            (stepTick
             + static_cast<uint32_t>(static_cast<int32_t>(_data.shift)
                                     + static_cast<int32_t>(stepLen) * 1024))
            % stepLen);

        _stepLocation.store (stepPos, std::memory_order_relaxed);

        if (_mute) return;

        const StepData303& cur = _data.step[stepPos];
        if (cur.rest) return;

        // ── Gate length in 96-PPQN ticks: slide / tie lookahead ──────────────
        // Identical to the original engine_303 logic.
        // _noteLengthTicks = 12 (50% of 24 ticks), _slideExtraTicks = 20.
        int32_t       gateLength     = _noteLengthTicks;
        const uint8_t lookaheadBound = static_cast<uint8_t>(stepLen + 1);
        uint8_t       nextStep       = stepPos;

        for (uint8_t i = 1; i < lookaheadBound; ++i)
        {
            nextStep = static_cast<uint8_t>((nextStep + 1) % lookaheadBound);

            if (cur.slide && ! _data.step[nextStep].rest)
            {
                gateLength = _noteLengthTicks + _slideExtraTicks;
                break;
            }
            else if (_data.step[nextStep].tie && _data.step[nextStep].rest)
            {
                // Each tied rest adds one full step (24 ticks)
                gateLength = _noteLengthTicks
                             + static_cast<int32_t>(i) * static_cast<int32_t>(_ticksPerStep);
            }
            else if (! _data.step[nextStep].rest || ! _data.step[nextStep].tie)
            {
                break;
            }
        }

        if (shufflePulses != 0)
            gateLength = std::max (1, gateLength + shufflePulses);

        // ── Note resolution ───────────────────────────────────────────────────
        uint8_t note = cur.note;
        if (_data.tune > 0)
            note = _harmonizer.harmonize (note) + static_cast<uint8_t>(_data.tune - 1);

        const int finalNote = static_cast<int>(note) + static_cast<int>(_data.transpose);
        if (finalNote < 0 || finalNote > 127) return;

        note = static_cast<uint8_t>(finalNote);

        // ── Place in note stack (gate length in 96-PPQN ticks) ───────────────
        for (int i = 0; i < SEQ303_NOTE_STACK_SIZE; ++i)
        {
            if (_stack[i].length == -1)
            {
                _stack[i].note   = note;
                _stack[i].length = gateLength;
                emit ({ Acid303EventType::NoteOn,
                        note,
                        cur.accent ? SEQ303_ACCENT_VELOCITY : SEQ303_NOTE_VELOCITY,
                        sampleOffset });
                break;
            }
        }
    }

    // ── onTick — fires every 96-PPQN tick; decrements note stack ─────────────
    // Replaces onPulse(). The countdown unit is now ticks (= pulses at 96 PPQN)
    // so all NOTE_LENGTH values remain identical to the original engine.
    void onTick (int sampleOffset)
    {
        for (int i = 0; i < SEQ303_NOTE_STACK_SIZE; ++i)
        {
            if (_stack[i].length > 0)
            {
                --_stack[i].length;
                if (_stack[i].length == 0)
                {
                    emit ({ Acid303EventType::NoteOff,
                            _stack[i].note,
                            0,
                            sampleOffset });
                    _stack[i].length = -1;
                }
            }
        }
    }

    // =========================================================================
    // ── Utility ──────────────────────────────────────────────────────────────
    // =========================================================================

    inline void emit (const Acid303Event& ev) const
    {
        if (onNoteEvent) onNoteEvent (ev);
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
        }
    }

    /** Send Note Off for all active stack entries, then clear the stack.
     *  Safe to call from either thread. */
    void clearStackInternal (bool sendNoteOffs)
    {
        bool sentAny = false;
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

    // Current step — written on audio thread; atomic for UI read
    std::atomic<uint8_t>   _stepLocation { 0 };

    // Mute — settable from any thread
    std::atomic<bool>      _mute { false };

    // Harmonizer — writes under _dataLock, reads on audio thread
    Harmonizer             _harmonizer;

    // Sync / start mode
    SyncMode               _syncMode  { SyncMode::Internal };
    StartMode              _startMode { StartMode::TransportStart };

    // Transport
    std::atomic<bool>      _running { false };

    // Clock parameters (set in prepare(), updated in setTempo())
    double                 _sampleRate      { 44100.0 };
    double                 _ppqn            { 960.0   };
    double                 _pulsesPerStep   { 240.0   };   // ppqn / 4 (kept for reference)
    double                 _ticksPerPulse   { 10.0    };   // ppqn / 96
    int                    _ticksPerStep    { 24      };   // always 24 at 96-PPQN resolution

    // Internal clock — event-driven accumulator (advances by samplesPerTick each tick)
    double                 _samplesPerTick  { 0.0     };   // sampleRate*60 / (bpm*96)
    double                 _sampleAccum     { 0.0     };   // fractional sample accumulator
    uint32_t               _tickCounter     { 0       };   // monotonic 96-PPQN tick counter

    // Host sync — last 96-PPQN tick seen from AudioPlayHead
    int64_t                _lastHostTick    { -1 };

    // Note lengths in 96-PPQN ticks (identical numbers to original engine_303)
    int32_t                _noteLengthTicks { 0 };   // = 12 ticks = 50% gate
    int32_t                _slideExtraTicks { 0 };   // = 20 ticks

    float                  _internalBpm { 120.f };

    // RNG (portable std::mt19937 replaces Arduino random())
    std::mt19937           _rng;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AcidSequencer303)
};
