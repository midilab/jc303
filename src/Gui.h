#pragma once

#include <JuceHeader.h>
#include "JC303.h"
#include "ui/BinaryData.h"
#include "ui/KnobLookAndFeel.h"

//==============================================================================
class JC303Editor  : public juce::AudioProcessorEditor,
                     public juce::Slider::Listener
{
public:
    explicit JC303Editor (JC303&);
    ~JC303Editor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider* createSlider(juce::AudioParameterFloat* parameter);
    void sliderValueChanged(juce::Slider* slider) override;
    void setControlsLayout();

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    JC303& processorRef;

    // Declare your Slider members
    juce::Slider* waveFormSlider;
    juce::Slider* tuningSlider;
    juce::Slider* cutoffFreqSlider;
    juce::Slider* resonanceSlider;
    juce::Slider* envelopModSlider;
    juce::Slider* decaySlider;
    juce::Slider* accentSlider;
    juce::Slider* volumeSlider;

    KnobLookAndFeel lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JC303Editor)
};
