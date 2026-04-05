#pragma once

#include <JuceHeader.h>
#include "../../JC303.h"
#include "KnobLookAndFeel.h"
#include "ModKnobLookAndFeel.h"
#include "SwitchButton.h"
#include "SwitchStepSeqButton.h"
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

    void setScaleFactor(float scale) override { juce::AudioProcessorEditor::setScaleFactor(1.0f); }

private:
    juce::Slider* createKnob(const juce::String& knobType, bool useModLookAndFeel = false);
    juce::Slider* createModKnob(const juce::String& label);
    SwitchButton* createSwitch();
    SwitchStepSeqButton* createSwitchStepSeq(SwitchStepSeqButton::Mode mode = SwitchStepSeqButton::Mode::Toggle, const juce::String& labelText = "");
    SwitchLed* createLed(const juce::String& paramID);
    juce::Label* createLabel(const juce::String& text);
    void setControlsLayout();

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    JC303& processorRef;

    // Main slider controls
    juce::Slider* waveformSlider;
    juce::Slider* tuningSlider;
    juce::Slider* cutoffFreqSlider;
    juce::Slider* resonanceSlider;
    juce::Slider* envelopModSlider;
    juce::Slider* decaySlider;
    juce::Slider* accentSlider;
    juce::Slider* volumeSlider;
    // MODs
    juce::Slider* normalDecaySlider;
    juce::Slider* accentDecaySlider;
    juce::Slider* feedbackFilterSlider;
    juce::Slider* softAttackSlider;
    juce::Slider* slideTimeSlider;
    juce::Slider* sqrDriverSlider;
    SwitchButton* switchModButton;
    SwitchLed* ledModButton;
    // overdrive
    juce::Slider* overdriveLevelSlider;
    juce::Slider* overdriveDryWetSlider;
    SwitchButton* switchOverdriveButton;
    SwitchLed* ledOverdriveButton;
    // generative sequencer
    juce::Slider* seqGenerativeFillSlider;
    juce::Slider* seqGenerativeAccentProbabilitySlider;
    juce::Slider* seqGenerativeSlideProbabilitySlider;
    juce::Slider* seqGenerativeTieProbabilitySlider;
    juce::Slider* numberOfTonesSlider;
    juce::Slider* lowerNoteSlider;
    juce::Slider* rangeNoteSlider;
    SwitchStepSeqButton* seqPlayButton;
    SwitchStepSeqButton* seqGenerateButton;
    SwitchStepSeqButton* seqClearButton;
    // generative sequencer new controls
    juce::Slider* seqHarmonizerSlider;
    juce::Slider* seqLengthSlider;
    juce::Slider* seqShiftSlider;
    // LFO controls
    juce::Slider* lfoWaveformSlider;
    juce::Slider* lfoRateSlider;
    juce::Slider* lfoDepthSlider;
    juce::Slider* lfoDestinationSlider;

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
    std::unique_ptr<SliderAttachment> slideTimeAttachment;
    std::unique_ptr<SliderAttachment> sqrDriverAttachment;
    std::unique_ptr<ButtonAttachment> switchModButtonAttachment;
    // overdrive
    std::unique_ptr<SliderAttachment> overdriveLevelAttachment;
    std::unique_ptr<SliderAttachment> overdriveDryWetAttachment;
    std::unique_ptr<ButtonAttachment> switchOverdriveButtonAttachment;
    // generative sequencer
    std::unique_ptr<SliderAttachment> seqGenerativeFillAttachment;
    std::unique_ptr<SliderAttachment> seqGenerativeAccentProbabilityAttachment;
    std::unique_ptr<SliderAttachment> seqGenerativeSlideProbabilityAttachment;
    std::unique_ptr<SliderAttachment> seqGenerativeTieProbabilityAttachment;
    std::unique_ptr<SliderAttachment> numberOfTonesAttachment;
    std::unique_ptr<SliderAttachment> lowerNoteAttachment;
    std::unique_ptr<SliderAttachment> rangeNoteAttachment;
    std::unique_ptr<ButtonAttachment> seqPlayButtonAttachment;
    std::unique_ptr<ButtonAttachment> seqGenerateButtonAttachment;
    std::unique_ptr<ButtonAttachment> seqClearButtonAttachment;
    std::unique_ptr<SliderAttachment> seqHarmonizerAttachment;
    std::unique_ptr<SliderAttachment> seqLengthAttachment;
    std::unique_ptr<SliderAttachment> seqShiftAttachment;
    std::unique_ptr<SliderAttachment> lfoWaveformAttachment;
    std::unique_ptr<SliderAttachment> lfoRateAttachment;
    std::unique_ptr<SliderAttachment> lfoDepthAttachment;
    std::unique_ptr<SliderAttachment> lfoDestinationAttachment;
    // previous, next buttons and model name display component
    OverdriveModelSelect* overdriveModelSelect;

    // our value tree state
    juce::AudioProcessorValueTreeState& valueTreeState;

    KnobLookAndFeel smallKnobLookAndFeel{"small"};
    KnobLookAndFeel mediumKnobLookAndFeel{"medium"};
    KnobLookAndFeel largeKnobLookAndFeel{"large"};
    ModKnobLookAndFeel modKnobLookAndFeel;

    // Easter egg mr. acid smile.
    AcidSmile acidSmile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JC303Editor)
};
