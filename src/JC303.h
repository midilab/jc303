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
  TANH_SHAPER_OFFSET,
  SQUARE_PHASE_SHIFT,
  SLIDE_TIME,
  PRE_FILTER_HPF,
  POST_FILTER_HPF,
  FEEDBACK_HPF,
  AMP_SUSTAIN,
  //FILTER_TYPE,

  OPEN303_NUM_PARAMETERS
};

// we need to keep track of defaults 303 internal values to restore in case MODs reset request.
// driver           =     36.9; dB2amp(TANH_SHAPER_DRIVE) => applies to square wave only (waveTable2.tanhShaperFactor)
// driverOffset     =     4.37; (TANH_SHAPER_OFFSET) => applies to square wave only (waveTable2.tanhShaperOffset)
// phaseShift       =     180.0; (SQUARE_PHASE_SHIFT) => applies to square wave only (waveTable2.squarePhaseShift)
// slideTime        =     60.0;
// preFilter        =     44.486; (PRE_FILTER_HPF) => (highpass1.setCutoff)
// postFilter       =     24.167; (POST_FILTER_HPF) => (highpass2.setCutoff)
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
    std::atomic<float>* driver = nullptr;
    std::atomic<float>* driverOffset = nullptr;
    std::atomic<float>* phaseShift = nullptr;
    std::atomic<float>* slideTime = nullptr;
    std::atomic<float>* preFilter = nullptr;
    std::atomic<float>* postFilter = nullptr;
    std::atomic<float>* feedbackFilter = nullptr;
    std::atomic<float>* ampSustain = nullptr;
    //std::atomic<float>* filterType = nullptr;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JC303)
};
