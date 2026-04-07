#pragma once

#include <JuceHeader.h>

// Open303
#include "dsp/open303/rosic_Open303.h"
using namespace rosic;

// GuitarML BYOD implementation
#include "dsp/guitarml-byod/processors/drive/GuitarMLAmp.h"

// Aciduino Sequencer
#include "sequencer/AcidSequencer303.h"

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
  // MODs
  SWITCH_MOD,
  NORMAL_DECAY,
  ACCENT_DECAY,
  FEEDBACK_HPF,
  SOFT_ATTACK,
  SLIDE_TIME,
  TANH_SHAPER_DRIVE,
  // LFO
  LFO_WAVEFORM,
  LFO_RATE,
  LFO_DEPTH,
  LFO_DESTINATION,
  // Overdrive
  OVERDRIVE_SWITCH,
  OVERDRIVE_LEVEL,
  OVERDRIVE_DRY_WET,
  OVERDRIVE_MODEL_INDEX,

  OPEN303_NUM_PARAMETERS
};

//==============================================================================
class JC303  :  public juce::AudioProcessor,
                public juce::AudioProcessorValueTreeState::Listener
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

private:
    void renderMidi  (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);
    void render303   (juce::AudioBuffer<float>& buffer, int beginSample, int endSample);
    void setParameter (Open303Parameters index, float value);

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
    };
    static constexpr int kPendingMax = 64;
    PendingNote  _pendingNotes[kPendingMax];
    int          _pendingCount { 0 };

    // Tracks host play state across buffers to detect start/stop edges
    bool _wasHostPlaying { false };

    // Currently held note from the sequencer (-1 = none).
    // Prevents stale NoteOffs from closing a new note after a slide,
    // and detects wrap-around ties (same pitch re-firing at pattern start).
    int  _heldNote         { -1    };

    // Slide flag from the last dispatched step, carried across buffer
    // boundaries so the *receiving* step's NoteOn gets slide=1 correctly.
    bool _lastStepHadSlide { false };

    // Mute flag for sequencer (used during acidRandomize to prevent note triggering)
    std::atomic<bool> _sequencerMuted { false };

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
    // MODs
    std::atomic<float>* switchModState = nullptr;
    std::atomic<float>* normalDecay = nullptr;
    std::atomic<float>* accentDecay = nullptr;
    std::atomic<float>* feedbackFilter = nullptr;
    std::atomic<float>* softAttack = nullptr;
    std::atomic<float>* slideTime = nullptr;
    std::atomic<float>* sqrDriver = nullptr;
    // LFO
    std::atomic<float>* lfoWaveform = nullptr;
    std::atomic<float>* lfoRate = nullptr;
    std::atomic<float>* lfoDepth = nullptr;
    std::atomic<float>* lfoDestination = nullptr;
    // overdrive
    std::atomic<float>* overdriveModelIndex = nullptr;
    std::atomic<float>* switchOverdriveState = nullptr;
    std::atomic<float>* overdriveLevel = nullptr;
    std::atomic<float>* overdriveDryWet = nullptr;
    // generative sequencer
    std::atomic<float>* seqGenerativeFill = nullptr;
    std::atomic<float>* seqGenerativeAccentProbability = nullptr;
    std::atomic<float>* seqGenerativeSlideProbability = nullptr;
    std::atomic<float>* seqGenerativeTieProbability = nullptr;
    std::atomic<float>* numberOfTones = nullptr;
    std::atomic<float>* lowerNote = nullptr;
    std::atomic<float>* rangeNote = nullptr;
    std::atomic<float>* seqPlayState = nullptr;
    std::atomic<float>* seqGenerate = nullptr;
    std::atomic<float>* seqClear = nullptr;
    std::atomic<float>* seqHarmonizer = nullptr;
    std::atomic<float>* seqLength = nullptr;
    std::atomic<float>* seqShift = nullptr;

    double decayMin = 200;
    double decayMax = 2000;

    // Flag to track if any parameter has changed
    std::atomic<bool> parametersNeedUpdate { false };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JC303)
};
