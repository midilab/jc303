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

  FILTER_TYPE,

#ifdef SHOW_INTERNAL_PARAMETERS
  AMP_SUSTAIN,
  TANH_SHAPER_DRIVE,
  TANH_SHAPER_OFFSET,
  PRE_FILTER_HPF,
  FEEDBACK_HPF,
  POST_FILTER_HPF,
  SQUARE_PHASE_SHIFT,
#endif

  OPEN303_NUM_PARAMETERS
};

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

    //==============================================================================
    juce::AudioParameterFloat* waveForm;
    juce::AudioParameterFloat* tuning;
    juce::AudioParameterFloat* cutoffFreq;
    juce::AudioParameterFloat* resonance;
    juce::AudioParameterFloat* envelopMod;
    juce::AudioParameterFloat* decay;
    juce::AudioParameterFloat* accent;
    juce::AudioParameterFloat* volume;

    // the embedded core dsp object:
    Open303 open303Core;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JC303)
};
