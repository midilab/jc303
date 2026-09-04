#pragma once

#include <JuceHeader.h>
#include "../../JC303.h"
#include "KnobLookAndFeel.h"
#include "ModKnobLookAndFeel.h"
#include "SwitchButton.h"
#include "MenuSwitchButton.h"
#include "SwitchStepSeqButton.h"
#include "SequencerStepSelector.h"
#include "../shared/MenuPage.h"
#include "../shared/SeqKeyboard.h"

typedef juce::AudioProcessorValueTreeState::SliderAttachment SliderAttachment;
typedef juce::AudioProcessorValueTreeState::ButtonAttachment ButtonAttachment;

//==============================================================================
class JC303Editor  : public juce::AudioProcessorEditor,
                         public juce::Timer
{
public:
    explicit JC303Editor (JC303&, juce::AudioProcessorValueTreeState&);
    ~JC303Editor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void updateKeyboardForSelectedStep();
    void selectStepFromSelector(int step);

    void setScaleFactor(float scale) override { juce::AudioProcessorEditor::setScaleFactor(1.0f); }

private:
    juce::Slider* createKnob(const juce::String& knobType, bool useModLookAndFeel = false);
    juce::Slider* createModKnob(const juce::String& label);
    AssignableSlider* createAssignableSlider(const juce::String& label, juce::Label*& labelOut);
    SwitchButton* createSwitch();
    MenuSwitchButton* createMenuSwitch(MenuSwitchButton::Mode mode);
    SwitchStepSeqButton* createSwitchStepSeq(SwitchStepSeqButton::Mode mode = SwitchStepSeqButton::Mode::Toggle, SwitchStepSeqButton::Size size = SwitchStepSeqButton::Size::Large);
    juce::Label* createLabel(const juce::String& text);
    void setControlsLayout();
    void selectMenu(int mode);

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
    // overdrive
    juce::Slider* overdriveLevelSlider;
    juce::Slider* overdriveDryWetSlider;
    SwitchButton* switchOverdriveButton;
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
    // menu navigation controls
    MenuSwitchButton* menuPresetButton;
    MenuSwitchButton* menuOverdriveButton;
    MenuSwitchButton* menuModButton;
    MenuSwitchButton* menuSeqButton;
    MenuSwitchButton* menuPrevButton;
    MenuSwitchButton* menuNextButton;
    MenuSwitchButton* menuDecButton;
    MenuSwitchButton* menuIncButton;
    juce::Slider* menuKnob;
    AssignableSlider* modAssign1;
    AssignableSlider* modAssign2;
    juce::Label* modAssign1Label;
    juce::Label* modAssign2Label;
    int menuMode = 0;
    // sequencer step toggles (note/rest editing), per-step accent/slide/tie toggles,
    // and click-to-select display LEDs
    SwitchButton* seqStepButtons[16];
    SwitchStepSeqButton* seqAccentButtons[16];
    SwitchStepSeqButton* seqSlideButtons[16];
    SwitchStepSeqButton* seqTieButtons[16];
    SequencerStepSelector* stepSelectors[16];
    int selectedStep = 0;

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
    // shared LCD menu page component (pages: PST, OVD, MOD, SEQ)
    MenuPage* menuPage;
    // shared single-octave keyboard (API-ready for sequencer wiring)
    SeqKeyboard* seqKeyboard;

    // our value tree state
    juce::AudioProcessorValueTreeState& valueTreeState;

    KnobLookAndFeel knobLookAndFeel;
    ModKnobLookAndFeel modKnobLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JC303Editor)
};
