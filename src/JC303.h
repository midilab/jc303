#pragma once

#include <JuceHeader.h>

// Open303
#include "dsp/open303/rosic_Open303.h"
using namespace rosic;

// GuitarML BYOD implementation
#include "dsp/guitarml-byod/processors/drive/GuitarMLAmp.h"

// Custom scale / microtuning (table only — loader is an implementation detail)
#include "dsp/tuning/TuningTable.h"

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
    // Custom tuning (scale / microtuning files)
    /** Load an AnaMark .tun file. Returns true on success. */
    bool loadTuningFile (const juce::File& file, juce::String* errorMessage = nullptr);

    /** Restore equal temperament using the current TUNING knob (master A4). */
    void resetTuningToDefault();

    juce::String getActiveTuningName() const;
    bool isCustomTuningActive() const;

    /** Listeners notified on the message thread when tuning changes. */
    void addTuningChangeListener (juce::ChangeListener* listener);
    void removeTuningChangeListener (juce::ChangeListener* listener);

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

private:
    void render303(juce::AudioBuffer<float>& buffer, int beginSample, int endSample);
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
    // overdrive
    std::atomic<float>* overdriveModelIndex = nullptr;
    std::atomic<float>* switchOverdriveState = nullptr;
    std::atomic<float>* overdriveLevel = nullptr;
    std::atomic<float>* overdriveDryWet = nullptr;

    double decayMin = 200;
    double decayMax = 2000;

    // Flag to track if any parameter has changed
    std::atomic<bool> parametersNeedUpdate { false };

    /** Message-thread source of truth for tuning name/path/freqs (UI + project state).
        Live pitch for audio is installed into open303Core's double-buffered map.
        Do not mutate tuningState from the parameter/audio path. */
    TuningTable tuningState;

    /** Mirrors tuningState.isCustom() for lock-free reads from the parameter path. */
    std::atomic<bool> customTuningActive { false };

    /** Broadcasts when custom tuning is loaded/reset/restored from state. */
    juce::ChangeBroadcaster tuningChangeBroadcaster;

    /** A4 Hz from the TUNING parameter (400..480). */
    double a4FromTuningParameter() const;

    /** Push tuningState frequencies into the DSP pitch bank (one install path). */
    void installActiveTuning();

    /** Clear custom scale: ET from TUNING knob → tuningState + DSP. Message thread only. */
    void applyEqualTemperamentFromParameter();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JC303)
};
