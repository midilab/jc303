#pragma once

#include <JuceHeader.h>
#include "../../JC303.h"
#include "KnobLookAndFeel.h"
#include "SwitchButton.h"
#include "SwitchLed.h"
#include "OverdriveModelSelect.h"
#include "AcidSmile.h"

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

private:
    std::unique_ptr<juce::Slider> createKnob(const juce::String& knobType);
    std::unique_ptr<SwitchButton> createSwitch();
    std::unique_ptr<SwitchLed> createLed(const juce::String& paramID);
    void setControlsLayout();

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    JC303& processorRef;

    // Main slider controls
    std::unique_ptr<juce::Slider> waveformSlider;
    std::unique_ptr<juce::Slider> tuningSlider;
    std::unique_ptr<juce::Slider> cutoffFreqSlider;
    std::unique_ptr<juce::Slider> resonanceSlider;
    std::unique_ptr<juce::Slider> envelopModSlider;
    std::unique_ptr<juce::Slider> decaySlider;
    std::unique_ptr<juce::Slider> accentSlider;
    std::unique_ptr<juce::Slider> volumeSlider;
    // MODs
    std::unique_ptr<juce::Slider> normalDecaySlider;
    std::unique_ptr<juce::Slider> accentDecaySlider;
    std::unique_ptr<juce::Slider> feedbackFilterSlider;
    std::unique_ptr<juce::Slider> softAttackSlider;
    std::unique_ptr<juce::Slider> accentSoftAttackSlider;
    std::unique_ptr<juce::Slider> slideTimeSlider;
    std::unique_ptr<juce::Slider> sqrDriverSlider;
    std::unique_ptr<SwitchButton> switchModButton;
    std::unique_ptr<SwitchLed> ledModButton;
    // overdrive
    std::unique_ptr<juce::Slider> overdriveLevelSlider;
    std::unique_ptr<juce::Slider> overdriveDryWetSlider;
    std::unique_ptr<SwitchButton> switchOverdriveButton;
    std::unique_ptr<SwitchLed> ledOverdriveButton;

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
    std::unique_ptr<SliderAttachment> normalDecayAttachment;
    std::unique_ptr<SliderAttachment> accentDecayAttachment;
    std::unique_ptr<SliderAttachment> feedbackFilterAttachment;
    std::unique_ptr<SliderAttachment> softAttackAttachment;
    std::unique_ptr<SliderAttachment> accentSoftAttackAttachment;
    std::unique_ptr<SliderAttachment> slideTimeAttachment;
    std::unique_ptr<SliderAttachment> sqrDriverAttachment;
    std::unique_ptr<ButtonAttachment> switchModButtonAttachment;
    // overdrive
    std::unique_ptr<SliderAttachment> overdriveLevelAttachment;
    std::unique_ptr<SliderAttachment> overdriveDryWetAttachment;
    std::unique_ptr<ButtonAttachment> switchOverdriveButtonAttachment;
    // previous, next buttons and model name display component
    std::unique_ptr<OverdriveModelSelect> overdriveModelSelect;

    // our value tree state
    juce::AudioProcessorValueTreeState& valueTreeState;

    KnobLookAndFeel smallKnobLookAndFeel{"small"};
    KnobLookAndFeel mediumKnobLookAndFeel{"medium"};
    KnobLookAndFeel largeKnobLookAndFeel{"large"};

    // Easter egg mr. acid smile.
    AcidSmile acidSmile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JC303Editor)
};
