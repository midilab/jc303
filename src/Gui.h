#pragma once

#include <JuceHeader.h>
#include "JC303.h"
#include "JuceLibraryCode/BinaryData.h"
#include "gui/KnobLookAndFeel.h"
#include "gui/ModKnobLookAndFeel.h"
#include "gui/SwitchButton.h"

typedef juce::AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
typedef juce::AudioProcessorValueTreeState::ButtonAttachment ButtonAttachment;

//==============================================================================
class JC303Editor  : public juce::AudioProcessorEditor
{
public:
    explicit JC303Editor (JC303&, juce::AudioProcessorValueTreeState&);
    ~JC303Editor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    // query switch state
    bool getSwitchState() const
    {
        return switchModButton->getToggleState();
    }

private:
    juce::Slider* create303Knob();
    juce::Slider* createModKnob(const juce::String& label);
    SwitchButton* createSwitch(const juce::String& label);
    void handleSwitchModButton();
    void setControlsLayout();

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    JC303& processorRef;

    // Declare your Slider members
    juce::Slider* waveformSlider;
    juce::Slider* tuningSlider;
    juce::Slider* cutoffFreqSlider;
    juce::Slider* resonanceSlider;
    juce::Slider* envelopModSlider;
    juce::Slider* decaySlider;
    juce::Slider* accentSlider;
    juce::Slider* volumeSlider;
    // MODs
    juce::Slider* sqrDriverSlider;
    juce::Slider* ampReleaseSlider;
    juce::Slider* ampSustainSlider;
    juce::Slider* slideTimeSlider;
    juce::Slider* feedbackFilterSlider;
    juce::Slider* softAttackSlider;
    juce::Slider* normalDecaySlider;
    juce::Slider* accentDecaySlider;
    SwitchButton* switchModButton;

    // declare the attchaments
    std::unique_ptr<SliderAttachment> waveformAttachment;
    std::unique_ptr<SliderAttachment> tuningAttachment;
    std::unique_ptr<SliderAttachment> cutoffFreqAttachment;
    std::unique_ptr<SliderAttachment> resonanceAttachment;
    std::unique_ptr<SliderAttachment> envelopModAttachment;
    std::unique_ptr<SliderAttachment> decayAttachment;
    std::unique_ptr<SliderAttachment> accentAttachment;
    std::unique_ptr<SliderAttachment> volumeAttachment;
    // MODs
    std::unique_ptr<ButtonAttachment> switchModButtonAttachment;
    std::unique_ptr<SliderAttachment> sqrDriverAttachment;
    std::unique_ptr<SliderAttachment> ampReleaseAttachment;
    std::unique_ptr<SliderAttachment> ampSustainAttachment;
    std::unique_ptr<SliderAttachment> slideTimeAttachment;
    std::unique_ptr<SliderAttachment> feedbackFilterAttachment;
    std::unique_ptr<SliderAttachment> softAttackAttachment;
    std::unique_ptr<SliderAttachment> normalDecayAttachment;
    std::unique_ptr<SliderAttachment> accentDecayAttachment;

    // our value tree state
    juce::AudioProcessorValueTreeState& valueTreeState;

    KnobLookAndFeel knobLookAndFeel;
    ModKnobLookAndFeel modKnobLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JC303Editor)
};
