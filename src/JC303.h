#pragma once

#include <JuceHeader.h>

// Open303
#include "dsp/open303/rosic_Open303.h"
#include "dsp/open303/dfl_DispersionFilter.h"
using namespace rosic;
using namespace dfl;

// GuitarML BYOD implementation
#include "dsp/guitarml-byod/processors/drive/GuitarMLAmp.h"

// Aciduino Sequencer
#include "sequencer/AcidSequencer303.h"

// Allpass dispersion in front of the overdrive: it leaves the spectrum alone but smears the harmonics' phases,
// reshaping the waveform's peaks, which changes how the distortion bites. Eight 2nd-order allpasses spread over
// two octaves around the Disperse corner (150 Hz..3 kHz, where a 303 line's harmonics sit; 0 is bypass). Once the
// harmonics are scrambled the Q matters little, so it is fixed at 1, which gave the peakiest waveform.
class Disperser
{
public:
    void prepare (double newSampleRate)
    {
        filter.setSampleRate (newSampleRate);
        filter.setStages (dfl::DispersionFilter::MAX_STAGES);
        filter.setSpread (2.0);
        filter.setQ (1.0);
        filter.reset();
        active = false;
    }

    void setAmount (float newAmount) { amount = newAmount; }

    static double cornerFor (float p) { return minCorner * std::pow (maxCorner / minCorner, (double) p); }

    void process (float* x, int numSamples)
    {
        if (amount <= 0.0f)
        {
            active = false;
            return;
        }
        const double target = std::log (cornerFor (amount));
        if (! active)
        {
            // coming out of bypass: start clean instead of from stale stage state
            active = true;
            filter.reset();
            logCorner = target;
            filter.setCenterFreq (std::exp (logCorner));
        }
        // glide the corner in the log domain, updating the coefficients every few samples
        for (int start = 0; start < numSamples; start += updateInterval)
        {
            if (std::abs (target - logCorner) > 1.0e-4)
            {
                logCorner += 0.1 * (target - logCorner);
                filter.setCenterFreq (std::exp (logCorner));
            }
            const int end = juce::jmin (numSamples, start + updateInterval);
            for (int n = start; n < end; ++n)
                x[n] = filter.process (x[n]);
        }
    }

private:
    static constexpr double minCorner = 150.0, maxCorner = 3000.0;
    static constexpr int updateInterval = 16;

    dfl::DispersionFilter filter;
    float amount = 0.0f;
    double logCorner = 0.0;
    bool active = false;
};

// The overdrive in pre-VCA mode: GuitarML after the filter, before the VCA (Open303's pre-VCA insert).
class PreVcaOverdrive : public Open303::PreVcaInsert
{
public:
    void prepare (double sampleRate)
    {
        amp.prepareProcessing (sampleRate, Open303::maxBlockSize);
        mix.prepare ({ sampleRate, (uint32_t) Open303::maxBlockSize, 1 });
        mix.setMixingRule (juce::dsp::DryWetMixingRule::sin3dB);
        disperser.prepare (sampleRate);
        buffer.setSize (1, Open303::maxBlockSize);
    }

    void process (double* x, int numSamples) override
    {
        buffer.setSize (1, numSamples, false, false, true);
        auto* b = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            b[i] = (float) x[i];
        disperser.process (b, numSamples);
        mix.pushDrySamples (juce::dsp::AudioBlock<float> (buffer));
        amp.processAudioBlock (buffer);
        mix.mixWetSamples (juce::dsp::AudioBlock<float> (buffer));
        for (int i = 0; i < numSamples; ++i)
            x[i] = b[i];
    }

    GuitarMLAmp amp;
    juce::dsp::DryWetMixer<float> mix;
    Disperser disperser;

private:
    juce::AudioBuffer<float> buffer;
};

enum Open303Parameters
{
  WAVEFORM = 0,
  TUNING,
  CUTOFF,
  RESONANCE,
  ENVMOD,
  DECAY,
  ACCENT,
  VOLUME,
  REVERSE_GATE,
  // MODs
  SWITCH_MOD,
  NORMAL_DECAY,
  ACCENT_DECAY,
  ACCENT_SWEEP,
  FEEDBACK_HPF,
  SOFT_ATTACK,
  SLIDE_TIME,
  TANH_SHAPER_DRIVE,
  FILTER_DRIVE,
  FILTER_MORPH,
  BASS_COMP,
  FILTER_FM,
  RES_TRIM,
  FILTER_QUALITY,
  FILTER_TENSION,
  FILTER_BIAS,
  FILTER_RAIL,
  // LFO
  LFO_WAVEFORM,
  LFO_RATE,
  LFO_DEPTH,
  LFO_DESTINATION,
  LFO_PHASE,
  LFO_SYNC,
  // Overdrive
  OVERDRIVE_SWITCH,
  OVERDRIVE_LEVEL,
  OVERDRIVE_DRY_WET,
  OVERDRIVE_MODEL_INDEX,
  OVERDRIVE_DISPERSE,
  // Sub oscillator
  SUB_OSC_GAIN,
  SUB_OSC_BLEND,

  OPEN303_NUM_PARAMETERS
};

//==============================================================================
// Accent Sweep knob 0..1 -> accent capacitor time constant in ms; 15 ms is the stock 303
inline double accentSweepMs (float value) { return linToExp (value, 0.0, 1.0, 15.0, 2000.0); }

class JC303  :  public juce::AudioProcessor,
                public juce::AudioProcessorValueTreeState::Listener,
                private juce::AsyncUpdater
{
public:
    //==============================================================================
    JC303();
    ~JC303() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    void setDevilMod(bool mode);

    // Bipolar (-1..1) parameters: a centre detent snaps |v| < bipolarDetentZone to exactly 0 and the rest of the travel
    // is stretched so the ends still reach +-1 (as the web UI's sticky zero, with a narrower zone so the Mods menu's
    // 2 % steps leave it in two clicks).
    static constexpr float bipolarDetentZone = 0.05f;
    static float bipolarDetent (float v)
    {
        const float a = std::abs (v);
        return a < bipolarDetentZone ? 0.0f : std::copysign ((a - bipolarDetentZone) / (1.0f - bipolarDetentZone), v);
    }

    // TB-303 service notes: TUNING approx. +/-700 cents (perfect fifth)
    static constexpr float tuningRangeCents = 700.0f;
    static float tuningCents (float v) { return tuningRangeCents * bipolarDetent (v); }

    static juce::String signedPercent (float v)
    {
        const int pct = juce::roundToInt (v * 100.0f);
        return (pct > 0 ? "+" : "") + juce::String (pct) + "%";
    }

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::StringArray getModelListNames() { return guitarML.getModelListNames(); }

    // ── Sequencer public API (called from Editor / host automation) ───────────

    /** Direct access for the Editor — pattern editing, randomize, display. */
    AcidSequencer303& getSequencer() { return _sequencer; }

    /** Sync source: Internal (own BPM), Host (DAW transport), MidiClock. */
    void setSequencerSyncMode  (AcidSequencer303::SyncMode  m) { _sequencer.setSyncMode  (m); }
    /** Start trigger: TransportStart (play/MIDI Start) or NoteTriggered. */
    void setSequencerStartMode (AcidSequencer303::StartMode m) { _sequencer.setStartMode (m); }

    /** Internal BPM — only active when SyncMode == Internal. */
    void  setSequencerTempo (float bpm) { _sequencer.setTempo (bpm); }
    float getSequencerTempo()     const { return _sequencer.getTempo(); }

    /** Hard start / stop from the UI. */
    void sequencerStart() { _sequencer.start(); }
    void sequencerStop()  { _sequencer.stop();  }

    // ── Sequencer UI→audio command channel ────────────────────────────────────
    enum class SeqCommand : int { None = 0, Play, Stop, Generate, Clear, GeneratePitches, GenerateRhythm, Undo, Redo };
    void seqCommand (SeqCommand command) { _seqCommand.store (static_cast<int>(command), std::memory_order_release); }

private:
    void renderMidi  (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);
    void render303   (juce::AudioBuffer<float>& buffer, int beginSample, int endSample);
    void setParameter (Open303Parameters index, float value);
    // Re-applies all cheap DSP parameters from the APVTS atomics on the audio thread. Called from
    // processBlock when parametersNeedUpdate is set, so parameter writes never race getSample().
    void updateOpen303Parameters();

    // Sequencer UI→audio command channel, consumed at the top of every audio
    // buffer so play/stop/generate/clear (and the accompanying Open303 note
    // flushes) always run on the audio thread.  Setting it from the message
    // thread keeps sequencer + Open303 state single-threaded while running.
    void applySeqCommands();
    void applySeqRewrites();
    void resetSeqOffsets (int mask);
    void handleAsyncUpdate() override;

    // presets and overdrive models user data management
    void setupDataDirectories();
    void installTones();
    int loadOverdriveTones();

    // embedded core dsp objects
    // Open303
    Open303 open303Core;
    // GuitarML - BYOD
    GuitarMLAmp guitarML;
    juce::dsp::DryWetMixer<float> overdriveMix;
    Disperser overdriveDisperser;
    PreVcaOverdrive preVcaOverdrive;

    // ── Acid Sequencer ────────────────────────────────────────────────────────
    AcidSequencer303 _sequencer;

    // Pending note events populated by the sequencer callback during
    // _sequencer.processBlock(), then flushed into Open303 inside the
    // existing MIDI render loop. 64 slots >> anything a mono 16-step
    // sequencer can produce per audio buffer.
    struct PendingNote
    {
        Acid303EventType type;
        uint8_t          note;
        uint8_t          velocity;
        int              sampleOffset;
        bool             mute;
        bool             reverse;
        bool             hammerInto;
        double           gateSamples;
    };
    static constexpr int kPendingMax = 64;
    PendingNote  _pendingNotes[kPendingMax];
    int          _pendingCount { 0 };

    // Tracks host play state across buffers to detect start/stop edges
    bool _wasHostPlaying { false };

    // Mute flag for sequencer (used during acidRandomize to prevent note triggering)
    std::atomic<bool> _sequencerMuted { false };

    // Rec-mode live state (audio thread only):
    // - _recHeldNote: last MIDI note recorded while still held, for legato/slide
    //   detection (a new noteOn while one is held records the step with slide).
    // - _sustainArmed: CC64 (sustain pedal) re-arm latch — a 127 tap records one
    //   rest, then waits for 0 before the next 127 can record another.
    // - _recWasOn: edge detector to reset the two above when rec is re-enabled.
    int  _recHeldNote   { -1    };
    bool _sustainArmed  { true  };
    bool _recWasOn      { false };

    // presets storage: user documents folder
    File userAppDataDirectory = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile(JucePlugin_Manufacturer).getChildFile(JucePlugin_Name);
    File userAppDataDirectory_tones = userAppDataDirectory.getFullPathName() + "/overdrive_models";

    //==============================================================================
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* waveForm = nullptr;
    std::atomic<float>* tuning = nullptr;
    std::atomic<float>* cutoffFreq = nullptr;
    std::atomic<float>* resonance = nullptr;
    std::atomic<float>* envelopMod = nullptr;
    std::atomic<float>* decay = nullptr;
    std::atomic<float>* accent = nullptr;
    std::atomic<float>* volume = nullptr;
    std::atomic<float>* reverseGate = nullptr;
    // MODs
    std::atomic<float>* switchModState = nullptr;
    std::atomic<float>* normalDecay = nullptr;
    std::atomic<float>* accentDecay = nullptr;
    std::atomic<float>* accentSweep = nullptr;
    std::atomic<float>* feedbackFilter = nullptr;
    std::atomic<float>* softAttack = nullptr;
    std::atomic<float>* slideTime = nullptr;
    std::atomic<float>* sqrDriver = nullptr;
    // LFO
    std::atomic<float>* lfoWaveform = nullptr;
    std::atomic<float>* lfoRate = nullptr;
    std::atomic<float>* lfoDepth = nullptr;
    std::atomic<float>* lfoDestination = nullptr;
    std::atomic<float>* lfoPhase = nullptr;
    std::atomic<float>* lfoSync = nullptr;
    // overdrive
    std::atomic<float>* overdriveModelIndex = nullptr;
    std::atomic<float>* switchOverdriveState = nullptr;
    std::atomic<float>* overdriveLevel = nullptr;
    std::atomic<float>* overdriveDryWet = nullptr;
    std::atomic<float>* overdriveDisperse = nullptr;
    std::atomic<float>* overdrivePreVca = nullptr;
    std::atomic<float>* filterType = nullptr;
    std::atomic<float>* squareModel = nullptr;
    std::atomic<float>* filterDrive = nullptr;
    std::atomic<float>* filterMorph = nullptr;
    std::atomic<float>* bassComp = nullptr;
    std::atomic<float>* filterFm = nullptr;
    std::atomic<float>* resTrim = nullptr;
    std::atomic<float>* filterQuality = nullptr;
    std::atomic<float>* filterTension = nullptr;
    std::atomic<float>* filterBias = nullptr;
    std::atomic<float>* filterRail = nullptr;
    // generative sequencer
    std::atomic<float>* seqGenerativeFill = nullptr;
    std::atomic<float>* seqGenerativeAccentProbability = nullptr;
    std::atomic<float>* seqGenerativeSlideProbability = nullptr;
    std::atomic<float>* seqGenerativeTieProbability = nullptr;
    std::atomic<float>* numberOfTones = nullptr;
    std::atomic<float>* lowerNote = nullptr;
    std::atomic<float>* rangeNote = nullptr;
    std::atomic<float>* seqPlayState = nullptr;
    std::atomic<float>* seqCpuTiming = nullptr;
    std::atomic<float>* seqTranspose = nullptr;
    std::atomic<float>* seqShift = nullptr;
    std::atomic<float>* seqPitchRotate = nullptr;
    std::atomic<float>* seqSwing = nullptr;
    std::atomic<float>* seqPitchLength = nullptr;
    std::atomic<float>* seqGenerate = nullptr;
    std::atomic<float>* seqClear = nullptr;
    //std::atomic<float>* seqHarmonizer = nullptr;
    std::atomic<float>* seqLength = nullptr;
    std::atomic<float>* seqSyncMode = nullptr;
    std::atomic<float>* seqStartMode = nullptr;
    std::atomic<float>* seqTempo = nullptr;
    // Sub oscillator
    std::atomic<float>* subOscGain = nullptr;
    std::atomic<float>* subOscBlend = nullptr;

    double decayMin = 200;
    double decayMax = 2000;

    // Flag to track if any parameter has changed
    std::atomic<bool> parametersNeedUpdate { false };

    // Pending sequencer command set by parameterChanged, applied by the audio
    // thread in renderMidi (see applySeqCommands).
    std::atomic<int> _seqCommand { static_cast<int>(SeqCommand::None) };

    // Transpose / Shift / Pitch Rotate values already written into the pattern
    std::atomic<int> _appliedTranspose { 0 };
    std::atomic<int> _appliedShift { 0 };
    std::atomic<int> _appliedPitchRotate { 0 };

    // Offsets a new or cleared pattern zeroes; the message thread resets the
    // params, and rewrites wait until it has
    enum SeqOffset : int { OffsetTranspose = 1, OffsetShift = 2, OffsetPitchRotate = 4 };
    std::atomic<int> _seqOffsetReset { 0 };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JC303)
};
