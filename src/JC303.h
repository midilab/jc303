#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/rosic_Open303.h"
using namespace rosic;

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
  TANH_SHAPER_DRIVE,
  AMP_SUSTAIN,
  AMP_RELEASE,
  SLIDE_TIME,
  FEEDBACK_HPF,
  SOFT_ATTACK,
  NORMAL_DECAY,
  ACCENT_DECAY,
  //FILTER_TYPE,

  OPEN303_NUM_PARAMETERS
};

// we need to keep track of defaults 303 internal values to restore in case MODs reset request.
// driver           =     36.9; dB2amp(TANH_SHAPER_DRIVE) => applies to square wave only (waveTable2.tanhShaperFactor)
// ampSustain     =     
// ampRelease       =    
// slideTime        =     60.0;
// preFilter        =     
// softAttack       =     
// feedbackFilter   =     150.0; (FEEDBACK_HPF) => (filter.setFeedbackHighpassCutoff)
// ampSustain       =     0.5; => wich is AnalogEnvelope::sustainLevel   = 0.5;

// filterType       =     TeeBeeFilter::TB_303 => it will be a select box with TeeBeeFilter::NUM_MODES options

//==============================================================================
class JC303  : public juce::AudioProcessor
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

    // what is the signature for this one on juce7?
    //void parameterValueChanged(int parameterIndex, float newValue) override;

    void setSwitchModState(bool newState);
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

    //void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    void render(juce::AudioBuffer<float>& buffer, int beginSample, int endSample);
    void setParameter (Open303Parameters index, float value);

    // the embedded core dsp object:
    Open303 open303Core;

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
    std::atomic<float>* sqrDriver = nullptr;
    std::atomic<float>* ampSustain = nullptr;
    std::atomic<float>* ampRelease = nullptr;
    std::atomic<float>* slideTime = nullptr;
    std::atomic<float>* feedbackFilter = nullptr;
    std::atomic<float>* softAttack = nullptr;
    std::atomic<float>* normalDecay = nullptr;
    std::atomic<float>* accentDecay = nullptr;
    //std::atomic<float>* filterType = nullptr;
    // mod switcc
    // Atomic variable to safely communicate between GUI and audio threads
    // mod fixed ranges support
    double decayMin;
    double decayMax;
    juce::Atomic<bool> switchModState{ false };

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JC303)
};
