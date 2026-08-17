#include "Gui.h"

//==============================================================================
JC303Editor::JC303Editor (JC303& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), processorRef (p), valueTreeState (vts)
{
    // Create and configure rotary sliders for each parameter
    addAndMakeVisible(waveformSlider = createKnob("medium"));
    addAndMakeVisible(volumeSlider = createKnob("medium"));
    addAndMakeVisible(tuningSlider = createKnob("medium"));
    addAndMakeVisible(cutoffFreqSlider = createKnob("medium"));
    addAndMakeVisible(resonanceSlider = createKnob("medium"));
    addAndMakeVisible(envelopModSlider = createKnob("medium"));
    addAndMakeVisible(decaySlider = createKnob("medium"));
    addAndMakeVisible(accentSlider = createKnob("medium"));
    // MODs row
    addAndMakeVisible(normalDecaySlider = createKnob("small"));
    addAndMakeVisible(accentDecaySlider = createKnob("small"));
    addAndMakeVisible(feedbackFilterSlider = createKnob("small"));
    addAndMakeVisible(softAttackSlider = createKnob("small"));
    addAndMakeVisible(slideTimeSlider = createKnob("small"));
    addAndMakeVisible(sqrDriverSlider = createKnob("small"));
    // on/off mod switch
    addAndMakeVisible(switchModButton = createSwitch());
    //addAndMakeVisible(ledModButton = createLed("switchModState"));
    // overdrive
    addAndMakeVisible(overdriveLevelSlider = createKnob("medium"));
    addAndMakeVisible(overdriveDryWetSlider = createKnob("medium"));
    // on/off overdrive switch
    addAndMakeVisible(switchOverdriveButton = createSwitch());
    //addAndMakeVisible(ledOverdriveButton = createLed("switchOverdriveState"));
    // overdrive model select component
    addAndMakeVisible(menuPage = new MenuPage(valueTreeState, MenuPage::buildPages(processorRef.getModelListNames())));

    // generative sequencer controls
    addAndMakeVisible(seqPlayButton = createSwitchStepSeq(SwitchStepSeqButton::Mode::Toggle, SwitchStepSeqButton::Size::Large));
    addAndMakeVisible(seqClearButton = createSwitchStepSeq(SwitchStepSeqButton::Mode::Press, SwitchStepSeqButton::Size::Small));
    addAndMakeVisible(seqGenerateButton = createSwitchStepSeq(SwitchStepSeqButton::Mode::Press, SwitchStepSeqButton::Size::Medium));
    addAndMakeVisible(seqGenerativeFillSlider = createModKnob("FILL"));
    addAndMakeVisible(seqGenerativeAccentProbabilitySlider = createModKnob("ACC"));
    addAndMakeVisible(seqGenerativeSlideProbabilitySlider = createModKnob("SLIDE"));
    addAndMakeVisible(seqGenerativeTieProbabilitySlider = createModKnob("TIE"));
    addAndMakeVisible(numberOfTonesSlider = createModKnob("TONES"));
    addAndMakeVisible(lowerNoteSlider = createModKnob("LOW"));
    addAndMakeVisible(rangeNoteSlider = createModKnob("RANGE"));
    addAndMakeVisible(seqHarmonizerSlider = createModKnob("HARM"));
    // sequencer controls
    addAndMakeVisible(seqLengthSlider = createModKnob("LEN"));
    addAndMakeVisible(seqShiftSlider = createModKnob("SHIFT"));
    // LFO controls
    addAndMakeVisible(lfoRateSlider = createModKnob("RATE"));
    addAndMakeVisible(lfoDepthSlider = createModKnob("DEPTH"));
    addAndMakeVisible(lfoDestinationSlider = createModKnob("DEST"));
    addAndMakeVisible(lfoWaveformSlider = createModKnob("WAVE"));

    // Easter egg mr. smile
    addAndMakeVisible(acidSmile);

    // menu navigation controls
    addAndMakeVisible(menuPresetButton = createMenuSwitch(MenuSwitchButton::Mode::Toggle));
    addAndMakeVisible(menuOverdriveButton = createMenuSwitch(MenuSwitchButton::Mode::Toggle));
    addAndMakeVisible(menuModButton = createMenuSwitch(MenuSwitchButton::Mode::Toggle));
    addAndMakeVisible(menuSeqButton = createMenuSwitch(MenuSwitchButton::Mode::Toggle));
    addAndMakeVisible(menuPrevButton = createMenuSwitch(MenuSwitchButton::Mode::Press));
    addAndMakeVisible(menuNextButton = createMenuSwitch(MenuSwitchButton::Mode::Press));
    addAndMakeVisible(menuDecButton = createMenuSwitch(MenuSwitchButton::Mode::Press));
    addAndMakeVisible(menuIncButton = createMenuSwitch(MenuSwitchButton::Mode::Press));
    addAndMakeVisible(menuKnob = createKnob("medium"));
    menuKnob->setRange(0.0, 100.0);

    menuPresetButton->setToggleState(true, juce::dontSendNotification);
    menuPresetButton->onClick = [this] { selectMenu(0); };
    menuOverdriveButton->onClick = [this] { selectMenu(1); };
    menuModButton->onClick = [this] { selectMenu(2); };
    menuSeqButton->onClick = [this] { selectMenu(3); };
    menuPrevButton->onPress = [this] { menuPage->indexStep(-1); };
    menuNextButton->onPress = [this] { menuPage->indexStep(1); };
    menuDecButton->onPress = [this] { menuPage->valueStep(-1); };
    menuIncButton->onPress = [this] { menuPage->valueStep(1); };
    menuKnob->onValueChange = [this] { menuPage->setValue((float) menuKnob->getValue()); };
    menuPage->onCurrentItemChanged = [this] (float v)
    {
        menuKnob->setEnabled(v >= 0.0f);
        if (v >= 0.0f)
            menuKnob->setValue(v, juce::dontSendNotification);
    };

    // sequencer step toggles
    addAndMakeVisible(seqStep1Button = createSwitch());
    addAndMakeVisible(seqStep2Button = createSwitch());
    addAndMakeVisible(seqStep3Button = createSwitch());
    addAndMakeVisible(seqStep4Button = createSwitch());
    addAndMakeVisible(seqStep5Button = createSwitch());
    addAndMakeVisible(seqStep6Button = createSwitch());
    addAndMakeVisible(seqStep7Button = createSwitch());
    addAndMakeVisible(seqStep8Button = createSwitch());
    addAndMakeVisible(seqStep9Button = createSwitch());
    addAndMakeVisible(seqStep10Button = createSwitch());
    addAndMakeVisible(seqStep11Button = createSwitch());
    addAndMakeVisible(seqStep12Button = createSwitch());
    addAndMakeVisible(seqStep13Button = createSwitch());
    addAndMakeVisible(seqStep14Button = createSwitch());
    addAndMakeVisible(seqStep15Button = createSwitch());
    addAndMakeVisible(seqStep16Button = createSwitch());

    // attach controls to processor parameters tree
    waveformAttachment.reset (new SliderAttachment (valueTreeState, "waveform", *waveformSlider));
    tuningAttachment.reset (new SliderAttachment (valueTreeState, "tuning", *tuningSlider));
    cutoffFreqAttachment.reset (new SliderAttachment (valueTreeState, "cutoff", *cutoffFreqSlider));
    resonanceAttachment.reset (new SliderAttachment (valueTreeState, "resonance", *resonanceSlider));
    envelopModAttachment.reset (new SliderAttachment (valueTreeState, "envmod", *envelopModSlider));
    decayAttachment.reset (new SliderAttachment (valueTreeState, "decay", *decaySlider));
    accentAttachment.reset (new SliderAttachment (valueTreeState, "accent", *accentSlider));
    volumeAttachment.reset (new SliderAttachment (valueTreeState, "volume", *volumeSlider));
    // MODs row
    normalDecayAttachment.reset(new SliderAttachment(valueTreeState, "normalDecay", *normalDecaySlider));
    accentDecayAttachment.reset(new SliderAttachment(valueTreeState, "accentDecay", *accentDecaySlider));
    feedbackFilterAttachment.reset(new SliderAttachment(valueTreeState, "feedbackFilter", *feedbackFilterSlider));
    softAttackAttachment.reset(new SliderAttachment(valueTreeState, "softAttack", *softAttackSlider));
    slideTimeAttachment.reset(new SliderAttachment(valueTreeState, "slideTime", *slideTimeSlider));
    sqrDriverAttachment.reset(new SliderAttachment(valueTreeState, "sqrDriver", *sqrDriverSlider));
    switchModButtonAttachment.reset(new ButtonAttachment(valueTreeState, "switchModState", *switchModButton));
    // overdrive
    overdriveLevelAttachment.reset(new SliderAttachment(valueTreeState, "overdriveLevel", *overdriveLevelSlider));
    overdriveDryWetAttachment.reset(new SliderAttachment(valueTreeState, "overdriveDryWet", *overdriveDryWetSlider));
    switchOverdriveButtonAttachment.reset(new ButtonAttachment(valueTreeState, "switchOverdriveState", *switchOverdriveButton));

    // generative sequencer attachments
    seqGenerativeFillAttachment.reset(new SliderAttachment(valueTreeState, "seqGenerativeFill", *seqGenerativeFillSlider));
    seqGenerativeAccentProbabilityAttachment.reset(new SliderAttachment(valueTreeState, "seqGenerativeAccentProbability", *seqGenerativeAccentProbabilitySlider));
    seqGenerativeSlideProbabilityAttachment.reset(new SliderAttachment(valueTreeState, "seqGenerativeSlideProbability", *seqGenerativeSlideProbabilitySlider));
    seqGenerativeTieProbabilityAttachment.reset(new SliderAttachment(valueTreeState, "seqGenerativeTieProbability", *seqGenerativeTieProbabilitySlider));
    numberOfTonesAttachment.reset(new SliderAttachment(valueTreeState, "numberOfTones", *numberOfTonesSlider));
    lowerNoteAttachment.reset(new SliderAttachment(valueTreeState, "lowerNote", *lowerNoteSlider));
    rangeNoteAttachment.reset(new SliderAttachment(valueTreeState, "rangeNote", *rangeNoteSlider));
    seqPlayButtonAttachment.reset(new ButtonAttachment(valueTreeState, "seqPlayState", *seqPlayButton));
    seqGenerateButtonAttachment.reset(new ButtonAttachment(valueTreeState, "seqGenerate", *seqGenerateButton));
    seqClearButtonAttachment.reset(new ButtonAttachment(valueTreeState, "seqClear", *seqClearButton));
    seqHarmonizerAttachment.reset(new SliderAttachment(valueTreeState, "seqHarmonizer", *seqHarmonizerSlider));
    seqLengthAttachment.reset(new SliderAttachment(valueTreeState, "seqLength", *seqLengthSlider));
    seqShiftAttachment.reset(new SliderAttachment(valueTreeState, "seqShift", *seqShiftSlider));
    lfoWaveformAttachment.reset(new SliderAttachment(valueTreeState, "lfoWaveform", *lfoWaveformSlider));
    lfoRateAttachment.reset(new SliderAttachment(valueTreeState, "lfoRate", *lfoRateSlider));
    lfoDepthAttachment.reset(new SliderAttachment(valueTreeState, "lfoDepth", *lfoDepthSlider));
    lfoDestinationAttachment.reset(new SliderAttachment(valueTreeState, "lfoDestination", *lfoDestinationSlider));

    setControlsLayout();

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (930, 523);
}

JC303Editor::~JC303Editor()
{
}

//==============================================================================
void JC303Editor::paint (juce::Graphics& g)
{
    // opaque base layer
    juce::Image background = ImageCache::getFromMemory (BinaryData::jc303guibackground_png, BinaryData::jc303guibackground_pngSize);
    g.drawImage (background, getLocalBounds().toFloat());

    // transparent detail layer on top
    juce::Image overlay = ImageCache::getFromMemory (BinaryData::jc303gui_png, BinaryData::jc303gui_pngSize);
    g.drawImage (overlay, getLocalBounds().toFloat());
}

void JC303Editor::resized()
{
    setControlsLayout();
}

juce::Slider* JC303Editor::createKnob(const juce::String& knobType, bool useModLookAndFeel)
{
    auto* slider = new juce::Slider();
    slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);

    if (useModLookAndFeel)
    {
        slider->setLookAndFeel(&modKnobLookAndFeel);
    }
    else if (knobType == "small")
    {
        slider->setLookAndFeel(&modKnobLookAndFeel);
    }
    else
    {
        slider->setLookAndFeel(&knobLookAndFeel);
    }

    slider->setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);

    // adjust our start and end point for knob
    slider->setRotaryParameters(0, 5.3, true);
    return slider;
}

SwitchButton* JC303Editor::createSwitch()
{
    auto* button = new SwitchButton();
    button->setClickingTogglesState(false);

    return button;
}

MenuSwitchButton* JC303Editor::createMenuSwitch(MenuSwitchButton::Mode mode)
{
    auto* button = new MenuSwitchButton(mode);
    button->setClickingTogglesState(false);

    return button;
}

void JC303Editor::selectMenu(int mode)
{
    menuMode = mode;

    menuPresetButton->setToggleState(mode == 0, juce::dontSendNotification);
    menuOverdriveButton->setToggleState(mode == 1, juce::dontSendNotification);
    menuModButton->setToggleState(mode == 2, juce::dontSendNotification);
    menuSeqButton->setToggleState(mode == 3, juce::dontSendNotification);

    menuPage->selectPage(mode);
}

SwitchStepSeqButton* JC303Editor::createSwitchStepSeq(SwitchStepSeqButton::Mode mode, SwitchStepSeqButton::Size size)
{
    auto* button = new SwitchStepSeqButton(mode, size);
    return button;
}

juce::Slider* JC303Editor::createModKnob(const juce::String& label)
{
    auto* slider = new juce::Slider();
    slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider->setLookAndFeel(&modKnobLookAndFeel);
    slider->setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
    slider->setRotaryParameters(0, 5.3, true);

    auto* labelComponent = new AttachedLabel();
    labelComponent->setText(label, juce::dontSendNotification);
    labelComponent->setJustificationType(juce::Justification::centredTop);
    labelComponent->setColour(juce::Label::textColourId, juce::Colours::black);
    labelComponent->attachToComponent(slider, true);

    return slider;
}

void JC303Editor::setControlsLayout()
{
    // Set the bounds and other properties for each gui component
    const int sliderLargeSize = 70;
    const int sliderMediumSize = 60;
    const int sliderSmallSize = 30;
    const int switchWidth = 30;
    const int switchHeight = 30;
    //const int ledWidth = 15;
    //const int ledHeight = 15;
    const int displayMenuWidth = 260;
    const int selectModelHeight = 130;
    const float acidSmileWidth = 56.25; //225/4;
    const float acidSmileHeight = 77.5; //310/4;
    const float seqPlayButtonWidth = 100 / 2;
    const float seqPlayButtonHeight = (70 / 2) + 15;
    const float seqSmallButtonWidth = 60 / 2;
    const float seqSmallButtonHeight = 36 / 2;
    const float seqMediumButtonWidth = 36 / 2;
    const float seqMediumButtonHeight = 70 / 2;

    // knob positioning location
    // first row
    pair<int, int> waveFormLocation = {45, 122};
    pair<int, int> cutoffFreqLocation = {156, 122};
    pair<int, int> resonanceLocation = {267, 122};
    pair<int, int> envelopeLocation = {378, 122};
    pair<int, int> decayLocation = {489, 122};
    pair<int, int> accentLocation = {600, 122};
    pair<int, int> tuningLocation = {722, 122};
    pair<int, int> volumeLocation = {840, 122};
    // MODs knobs row
    //pair<int, int> normalDecayLocation = {147, 380};
    //pair<int, int> accentDecayLocation = {208, 380};
    //pair<int, int> feedbackFilterLocation = {269, 380};
    //pair<int, int> softAttackLocation = {330, 380};
    //pair<int, int> slideTimeLocation = {391, 380};
    //pair<int, int> sqrDriverLocation = {452, 380};
    // MODs switch
    pair<int, int> switchLocation = {563, 255};
    //pair<int, int> modLedLocation = {566, 243};
    // overdrive
    pair<int, int> overdriveLevelLocation = {718, 240};
    pair<int, int> overdriveDryWetLocation = {838, 240};
    // overdrive switch
    pair<int, int> overdriveSwitchLocation = {794, 255};
    //pair<int, int> overdriveLedLocation = {856, 243};
    pair<int, int> displayMenuLocation = {60, 238};

    // Easter egg mr. smile
    pair<int, int> acidSmileLocation = {484, 16};

    // generative sequencer controls (top row, left to right)
    pair<int, int> seqPlayButtonLocation = {50, 375};
    pair<int, int> seqClearButtonLocation = {500, 370};
    pair<int, int> seqGenerateButtonLocation = {720, 340};

    pair<int, int> seqGenerativeFillLocation = {760, 340};
    pair<int, int> seqGenerativeAccentProbabilityLocation = {800, 340};
    pair<int, int> seqGenerativeSlideProbabilityLocation = {840, 340};
    pair<int, int> seqGenerativeTieProbabilityLocation = {880, 340};
    pair<int, int> numberOfTonesLocation = {760, 380};
    pair<int, int> lowerNoteLocation = {800, 380};
    pair<int, int> rangeNoteLocation = {840, 380};
    pair<int, int> seqHarmonizerLocation = {880, 380};

    pair<int, int> seqLengthLocation = {480, 340};
    pair<int, int> seqShiftLocation = {520, 340};

    // LFO controls
    //pair<int, int> lfoDepthLocation = {680, 20};
    //pair<int, int> lfoRateLocation = {720, 20};
    //pair<int, int> lfoWaveformLocation = {680, 60};
    //pair<int, int> lfoDestinationLocation = {720, 60};

    // sequencer step toggles
    const int switchStepWidth = 45;
    const int switchStepHeight = 45;
    const int switchStepGap = 8;
    pair<int, int> seqStep1ButtonLocation = {44, 425};
    pair<int, int> seqStep2ButtonLocation = {97, 425};
    pair<int, int> seqStep3ButtonLocation = {150, 425};
    pair<int, int> seqStep4ButtonLocation = {203, 425};
    pair<int, int> seqStep5ButtonLocation = {256, 425};
    pair<int, int> seqStep6ButtonLocation = {309, 425};
    pair<int, int> seqStep7ButtonLocation = {362, 425};
    pair<int, int> seqStep8ButtonLocation = {415, 425};
    pair<int, int> seqStep9ButtonLocation = {468, 425};
    pair<int, int> seqStep10ButtonLocation = {521, 425};
    pair<int, int> seqStep11ButtonLocation = {574, 425};
    pair<int, int> seqStep12ButtonLocation = {627, 425};
    pair<int, int> seqStep13ButtonLocation = {680, 425};
    pair<int, int> seqStep14ButtonLocation = {733, 425};
    pair<int, int> seqStep15ButtonLocation = {786, 425};
    pair<int, int> seqStep16ButtonLocation = {839, 425};

    // menu navigation controls (top row)
    const int menuButtonWidth = 34; //53;
    const int menuButtonHeight = 31; //48;

    pair<int, int> menuPresetButtonLocation = {50, 320};
    pair<int, int> menuOverdriveButtonLocation = {100, 320};
    pair<int, int> menuModButtonLocation = {150, 320};
    pair<int, int> menuSeqButtonLocation = {200, 320};

    pair<int, int> menuPrevButtonLocation = {250, 320};
    pair<int, int> menuNextButtonLocation = {300, 320};

    pair<int, int> menuDecButtonLocation = {365, 320};
    pair<int, int> menuIncButtonLocation = {416, 320};

    pair<int, int> menuKnobLocation = {375, 240};

    // menu navigation controls (top row)
    menuPresetButton->setBounds(menuPresetButtonLocation.first, menuPresetButtonLocation.second, menuButtonWidth, menuButtonHeight);
    menuOverdriveButton->setBounds(menuOverdriveButtonLocation.first, menuOverdriveButtonLocation.second, menuButtonWidth, menuButtonHeight);
    menuModButton->setBounds(menuModButtonLocation.first, menuModButtonLocation.second, menuButtonWidth, menuButtonHeight);
    menuSeqButton->setBounds(menuSeqButtonLocation.first, menuSeqButtonLocation.second, menuButtonWidth, menuButtonHeight);
    menuPrevButton->setBounds(menuPrevButtonLocation.first, menuPrevButtonLocation.second, menuButtonWidth, menuButtonHeight);
    menuNextButton->setBounds(menuNextButtonLocation.first, menuNextButtonLocation.second, menuButtonWidth, menuButtonHeight);
    menuDecButton->setBounds(menuDecButtonLocation.first, menuDecButtonLocation.second, menuButtonWidth, menuButtonHeight);
    menuIncButton->setBounds(menuIncButtonLocation.first, menuIncButtonLocation.second, menuButtonWidth, menuButtonHeight);
    menuKnob->setBounds(menuKnobLocation.first, menuKnobLocation.second, sliderMediumSize, sliderMediumSize);

    // large knobs
    waveformSlider->setBounds(waveFormLocation.first, waveFormLocation.second, sliderMediumSize, sliderMediumSize);
    volumeSlider->setBounds(volumeLocation.first, volumeLocation.second, sliderMediumSize, sliderMediumSize);
    // medium knobs
    tuningSlider->setBounds(tuningLocation.first, tuningLocation.second, sliderMediumSize, sliderMediumSize);
    cutoffFreqSlider->setBounds(cutoffFreqLocation.first, cutoffFreqLocation.second, sliderMediumSize, sliderMediumSize);
    resonanceSlider->setBounds(resonanceLocation.first, resonanceLocation.second, sliderMediumSize, sliderMediumSize);
    envelopModSlider->setBounds(envelopeLocation.first, envelopeLocation.second, sliderMediumSize, sliderMediumSize);
    decaySlider->setBounds(decayLocation.first, decayLocation.second, sliderMediumSize, sliderMediumSize);
    accentSlider->setBounds(accentLocation.first, accentLocation.second, sliderMediumSize, sliderMediumSize);
    // MODs, small knobs, switch
    //normalDecaySlider->setBounds(normalDecayLocation.first, normalDecayLocation.second, sliderSmallSize, sliderSmallSize);
    //accentDecaySlider->setBounds(accentDecayLocation.first, accentDecayLocation.second, sliderSmallSize, sliderSmallSize);
    //feedbackFilterSlider->setBounds(feedbackFilterLocation.first, feedbackFilterLocation.second, sliderSmallSize, sliderSmallSize);
    //softAttackSlider->setBounds(softAttackLocation.first, softAttackLocation.second, sliderSmallSize, sliderSmallSize);
    //slideTimeSlider->setBounds(slideTimeLocation.first, slideTimeLocation.second, sliderSmallSize, sliderSmallSize);
    //sqrDriverSlider->setBounds(sqrDriverLocation.first, sqrDriverLocation.second, sliderSmallSize, sliderSmallSize);
    switchModButton->setBounds(switchLocation.first, switchLocation.second, switchWidth, switchHeight);
    //ledModButton->setBounds(modLedLocation.first, modLedLocation.second, ledWidth, ledHeight);
    // overdrive
    overdriveLevelSlider->setBounds(overdriveLevelLocation.first, overdriveLevelLocation.second, sliderMediumSize, sliderMediumSize);
    overdriveDryWetSlider->setBounds(overdriveDryWetLocation.first, overdriveDryWetLocation.second, sliderMediumSize, sliderMediumSize);
    switchOverdriveButton->setBounds(overdriveSwitchLocation.first, overdriveSwitchLocation.second, switchWidth, switchHeight);
    //ledOverdriveButton ->setBounds(overdriveLedLocation.first, overdriveLedLocation.second, ledWidth, ledHeight);
    menuPage->setBounds(displayMenuLocation.first, displayMenuLocation.second, displayMenuWidth, selectModelHeight);

    // Easter egg mr. smile
    acidSmile.setBounds(acidSmileLocation.first, acidSmileLocation.second, acidSmileWidth, acidSmileHeight);

    // generative sequencer controls
    seqPlayButton->setBounds(seqPlayButtonLocation.first, seqPlayButtonLocation.second,
                             seqPlayButtonWidth, seqPlayButtonHeight);
    seqGenerativeFillSlider->setBounds(seqGenerativeFillLocation.first, seqGenerativeFillLocation.second, sliderSmallSize, sliderSmallSize);
    seqGenerativeAccentProbabilitySlider->setBounds(seqGenerativeAccentProbabilityLocation.first, seqGenerativeAccentProbabilityLocation.second, sliderSmallSize, sliderSmallSize);
    seqGenerativeSlideProbabilitySlider->setBounds(seqGenerativeSlideProbabilityLocation.first, seqGenerativeSlideProbabilityLocation.second, sliderSmallSize, sliderSmallSize);
    seqGenerativeTieProbabilitySlider->setBounds(seqGenerativeTieProbabilityLocation.first, seqGenerativeTieProbabilityLocation.second, sliderSmallSize, sliderSmallSize);
    numberOfTonesSlider->setBounds(numberOfTonesLocation.first, numberOfTonesLocation.second, sliderSmallSize, sliderSmallSize);
    lowerNoteSlider->setBounds(lowerNoteLocation.first, lowerNoteLocation.second, sliderSmallSize, sliderSmallSize);
    rangeNoteSlider->setBounds(rangeNoteLocation.first, rangeNoteLocation.second, sliderSmallSize, sliderSmallSize);
    seqGenerateButton->setBounds(seqGenerateButtonLocation.first, seqGenerateButtonLocation.second,
                                 seqMediumButtonWidth, seqMediumButtonHeight);
    seqClearButton->setBounds(seqClearButtonLocation.first, seqClearButtonLocation.second,
                              seqSmallButtonWidth, seqSmallButtonHeight);
    // generative sequencer new controls
    seqHarmonizerSlider->setBounds(seqHarmonizerLocation.first, seqHarmonizerLocation.second, sliderSmallSize, sliderSmallSize);
    seqLengthSlider->setBounds(seqLengthLocation.first, seqLengthLocation.second, sliderSmallSize, sliderSmallSize);
    seqShiftSlider->setBounds(seqShiftLocation.first, seqShiftLocation.second, sliderSmallSize, sliderSmallSize);
    // LFO controls
    //lfoWaveformSlider->setBounds(lfoWaveformLocation.first, lfoWaveformLocation.second, sliderSmallSize, sliderSmallSize);
    //lfoRateSlider->setBounds(lfoRateLocation.first, lfoRateLocation.second, sliderSmallSize, sliderSmallSize);
    //lfoDepthSlider->setBounds(lfoDepthLocation.first, lfoDepthLocation.second, sliderSmallSize, sliderSmallSize);
    //lfoDestinationSlider->setBounds(lfoDestinationLocation.first, lfoDestinationLocation.second, sliderSmallSize, sliderSmallSize);

    // sequencer step toggles
    seqStep1Button->setBounds(seqStep1ButtonLocation.first, seqStep1ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep2Button->setBounds(seqStep2ButtonLocation.first, seqStep2ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep3Button->setBounds(seqStep3ButtonLocation.first, seqStep3ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep4Button->setBounds(seqStep4ButtonLocation.first, seqStep4ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep5Button->setBounds(seqStep5ButtonLocation.first, seqStep5ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep6Button->setBounds(seqStep6ButtonLocation.first, seqStep6ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep7Button->setBounds(seqStep7ButtonLocation.first, seqStep7ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep8Button->setBounds(seqStep8ButtonLocation.first, seqStep8ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep9Button->setBounds(seqStep9ButtonLocation.first, seqStep9ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep10Button->setBounds(seqStep10ButtonLocation.first, seqStep10ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep11Button->setBounds(seqStep11ButtonLocation.first, seqStep11ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep12Button->setBounds(seqStep12ButtonLocation.first, seqStep12ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep13Button->setBounds(seqStep13ButtonLocation.first, seqStep13ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep14Button->setBounds(seqStep14ButtonLocation.first, seqStep14ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep15Button->setBounds(seqStep15ButtonLocation.first, seqStep15ButtonLocation.second, switchStepWidth, switchStepHeight);
    seqStep16Button->setBounds(seqStep16ButtonLocation.first, seqStep16ButtonLocation.second, switchStepWidth, switchStepHeight);
}
