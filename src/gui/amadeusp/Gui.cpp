#include "Gui.h"

//==============================================================================
JC303Editor::JC303Editor (JC303& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), processorRef (p), valueTreeState (vts)
{
    // Create and configure rotary sliders for each parameter
    addAndMakeVisible(waveformSlider = createKnob("large"));
    addAndMakeVisible(volumeSlider = createKnob("large"));
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
    addAndMakeVisible(ledModButton = createLed("switchModState"));
    // overdrive
    addAndMakeVisible(overdriveLevelSlider = createKnob("small"));
    addAndMakeVisible(overdriveDryWetSlider = createKnob("small"));
    // on/off overdrive switch
    addAndMakeVisible(switchOverdriveButton = createSwitch());
    addAndMakeVisible(ledOverdriveButton = createLed("switchOverdriveState"));
    // overdrive model select component
    addAndMakeVisible(overdriveModelSelect = new OverdriveModelSelect(valueTreeState, processorRef.getModelListNames()));

    // generative sequencer controls
    addAndMakeVisible(seqPlayButton = createSwitchStepSeq(SwitchStepSeqButton::Mode::Toggle, "PLAY"));
    addAndMakeVisible(seqGenerativeFillSlider = createModKnob("FILL"));
    addAndMakeVisible(seqGenerativeAccentProbabilitySlider = createModKnob("ACC"));
    addAndMakeVisible(seqGenerativeSlideProbabilitySlider = createModKnob("SLIDE"));
    addAndMakeVisible(seqGenerativeTieProbabilitySlider = createModKnob("TIE"));
    addAndMakeVisible(numberOfTonesSlider = createModKnob("TONES"));
    addAndMakeVisible(lowerNoteSlider = createModKnob("LOW"));
    addAndMakeVisible(rangeNoteSlider = createModKnob("RANGE"));
    addAndMakeVisible(seqGenerateButton = createSwitchStepSeq(SwitchStepSeqButton::Mode::Press, "GEN"));
    addAndMakeVisible(seqClearButton = createSwitchStepSeq(SwitchStepSeqButton::Mode::Press, "CLEAR"));
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
    //setSize (930, 363);
    setSize (930, 557);
}

JC303Editor::~JC303Editor()
{
}

//==============================================================================
void JC303Editor::paint (juce::Graphics& g)
{
    // Fill the background with a solid colour
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    // Set the drawing colour and font
    g.setColour (juce::Colours::white);
    g.setFont (15.0f);

    // Get the background image from memory
    juce::Image background = ImageCache::getFromMemory (BinaryData::jc303gui_png, BinaryData::jc303gui_pngSize);

    // Draw the image to fill the entire component area
    g.drawImage (background, getLocalBounds().toFloat());
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
        slider->setLookAndFeel(&smallKnobLookAndFeel);
    }
    else if (knobType == "medium")
    {
        slider->setLookAndFeel(&mediumKnobLookAndFeel);
    }
    else if (knobType == "large")
    {
        slider->setLookAndFeel(&largeKnobLookAndFeel);
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

SwitchStepSeqButton* JC303Editor::createSwitchStepSeq(SwitchStepSeqButton::Mode mode, const juce::String& labelText)
{
    auto* button = new SwitchStepSeqButton(mode, labelText);
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

SwitchLed* JC303Editor::createLed(const juce::String& paramID)
{
    auto* led = new SwitchLed(valueTreeState, paramID);
    return led;
}

void JC303Editor::setControlsLayout()
{
    // Set the bounds and other properties for each gui component
    const int sliderLargeSize = 70;
    const int sliderMediumSize = 60;
    const int sliderSmallSize = 30;
    const int switchWidth = 50;
    const int switchHeight = 18;
    const int ledWidth = 15;
    const int ledHeight = 15;
    const int selectModellWidth = 127;
    const int selectModelHeight = 100;
    const float acidSmileWidth = 56.25; //225/4;
    const float acidSmileHeight = 77.5; //310/4;
    const float seqPlayButtonWidth = 100 / 2;
    const float seqPlayButtonHeight = 70 / 2;
    const float seqGenerateButtonWidth = 50; //60 / 2;
    const float seqGenerateButtonHeight = 36 / 2;

    // knob positioning location
    // first row
    pair<int, int> waveFormLocation = {46, 140};
    pair<int, int> volumeLocation = {813, 140};
    // second row
    pair<int, int> tuningLocation = {188, 139};
    pair<int, int> cutoffFreqLocation = {287, 139};
    pair<int, int> resonanceLocation = {386, 139};
    pair<int, int> envelopeLocation = {485, 139};
    pair<int, int> decayLocation = {584, 139};
    pair<int, int> accentLocation = {683, 139};
    // MODs knobs row
    pair<int, int> normalDecayLocation = {147, 273};
    pair<int, int> accentDecayLocation = {208, 273};
    pair<int, int> feedbackFilterLocation = {269, 273};
    pair<int, int> softAttackLocation = {330, 273};
    pair<int, int> slideTimeLocation = {391, 273};
    pair<int, int> sqrDriverLocation = {452, 273};
    // MODs switch
    pair<int, int> switchLocation = {52, 273};
    pair<int, int> modLedLocation = {82, 243};
    // overdrive
    pair<int, int> overdriveLevelLocation = {566, 273};
    pair<int, int> overdriveDryWetLocation = {749, 273};
    // overdrive switch
    pair<int, int> overdriveSwitchLocation = {826, 273};
    pair<int, int> overdriveLedLocation = {856, 243};
    pair<int, int> overdriveModelSelectLocation = {610, 265};

    // Easter egg mr. smile
    pair<int, int> acidSmileLocation = {484, 16};

    // generative sequencer controls (top row, left to right)
    pair<int, int> seqPlayButtonLocation = {105, 13};
    pair<int, int> seqGenerateButtonLocation = {105, 51};
    pair<int, int> seqClearButtonLocation = {105, 70};
    pair<int, int> seqGenerativeFillLocation = {165, 20};
    pair<int, int> seqGenerativeAccentProbabilityLocation = {205, 20};
    pair<int, int> seqGenerativeSlideProbabilityLocation = {245, 20};
    pair<int, int> seqGenerativeTieProbabilityLocation = {285, 20};
    pair<int, int> seqLengthLocation = {325, 20};
    pair<int, int> numberOfTonesLocation = {165, 60};
    pair<int, int> lowerNoteLocation = {205, 60};
    pair<int, int> rangeNoteLocation = {245, 60};
    pair<int, int> seqHarmonizerLocation = {285, 60};
    pair<int, int> seqShiftLocation = {325, 60};
    // LFO controls
    pair<int, int> lfoDepthLocation = {680, 20};
    pair<int, int> lfoRateLocation = {720, 20};
    pair<int, int> lfoWaveformLocation = {680, 60};
    pair<int, int> lfoDestinationLocation = {720, 60};

    // large knobs
    waveformSlider->setBounds(waveFormLocation.first, waveFormLocation.second, sliderLargeSize, sliderLargeSize);
    volumeSlider->setBounds(volumeLocation.first, volumeLocation.second, sliderLargeSize, sliderLargeSize);
    // medium knobs
    tuningSlider->setBounds(tuningLocation.first, tuningLocation.second, sliderMediumSize, sliderMediumSize);
    cutoffFreqSlider->setBounds(cutoffFreqLocation.first, cutoffFreqLocation.second, sliderMediumSize, sliderMediumSize);
    resonanceSlider->setBounds(resonanceLocation.first, resonanceLocation.second, sliderMediumSize, sliderMediumSize);
    envelopModSlider->setBounds(envelopeLocation.first, envelopeLocation.second, sliderMediumSize, sliderMediumSize);
    decaySlider->setBounds(decayLocation.first, decayLocation.second, sliderMediumSize, sliderMediumSize);
    accentSlider->setBounds(accentLocation.first, accentLocation.second, sliderMediumSize, sliderMediumSize);
    // MODs, small knobs, switch
    normalDecaySlider->setBounds(normalDecayLocation.first, normalDecayLocation.second, sliderSmallSize, sliderSmallSize);
    accentDecaySlider->setBounds(accentDecayLocation.first, accentDecayLocation.second, sliderSmallSize, sliderSmallSize);
    feedbackFilterSlider->setBounds(feedbackFilterLocation.first, feedbackFilterLocation.second, sliderSmallSize, sliderSmallSize);
    softAttackSlider->setBounds(softAttackLocation.first, softAttackLocation.second, sliderSmallSize, sliderSmallSize);
    slideTimeSlider->setBounds(slideTimeLocation.first, slideTimeLocation.second, sliderSmallSize, sliderSmallSize);
    sqrDriverSlider->setBounds(sqrDriverLocation.first, sqrDriverLocation.second, sliderSmallSize, sliderSmallSize);
    switchModButton->setBounds(switchLocation.first, switchLocation.second, switchWidth, switchHeight);
    ledModButton->setBounds(modLedLocation.first, modLedLocation.second, ledWidth, ledHeight);
    // overdrive
    overdriveLevelSlider->setBounds(overdriveLevelLocation.first, overdriveLevelLocation.second, sliderSmallSize, sliderSmallSize);
    overdriveDryWetSlider->setBounds(overdriveDryWetLocation.first, overdriveDryWetLocation.second, sliderSmallSize, sliderSmallSize);
    switchOverdriveButton->setBounds(overdriveSwitchLocation.first, overdriveSwitchLocation.second, switchWidth, switchHeight);
    ledOverdriveButton ->setBounds(overdriveLedLocation.first, overdriveLedLocation.second, ledWidth, ledHeight);
    overdriveModelSelect->setBounds(overdriveModelSelectLocation.first, overdriveModelSelectLocation.second, selectModellWidth, selectModelHeight);

    // Easter egg mr. smile
    acidSmile.setBounds(acidSmileLocation.first, acidSmileLocation.second, acidSmileWidth, acidSmileHeight);

    // generative sequencer controls
    seqPlayButton->setBounds(seqPlayButtonLocation.first, seqPlayButtonLocation.second, seqPlayButtonWidth, seqPlayButtonHeight);
    seqGenerativeFillSlider->setBounds(seqGenerativeFillLocation.first, seqGenerativeFillLocation.second, sliderSmallSize, sliderSmallSize);
    seqGenerativeAccentProbabilitySlider->setBounds(seqGenerativeAccentProbabilityLocation.first, seqGenerativeAccentProbabilityLocation.second, sliderSmallSize, sliderSmallSize);
    seqGenerativeSlideProbabilitySlider->setBounds(seqGenerativeSlideProbabilityLocation.first, seqGenerativeSlideProbabilityLocation.second, sliderSmallSize, sliderSmallSize);
    seqGenerativeTieProbabilitySlider->setBounds(seqGenerativeTieProbabilityLocation.first, seqGenerativeTieProbabilityLocation.second, sliderSmallSize, sliderSmallSize);
    numberOfTonesSlider->setBounds(numberOfTonesLocation.first, numberOfTonesLocation.second, sliderSmallSize, sliderSmallSize);
    lowerNoteSlider->setBounds(lowerNoteLocation.first, lowerNoteLocation.second, sliderSmallSize, sliderSmallSize);
    rangeNoteSlider->setBounds(rangeNoteLocation.first, rangeNoteLocation.second, sliderSmallSize, sliderSmallSize);
    seqGenerateButton->setBounds(seqGenerateButtonLocation.first, seqGenerateButtonLocation.second, seqGenerateButtonWidth, seqGenerateButtonHeight);
    seqClearButton->setBounds(seqClearButtonLocation.first, seqClearButtonLocation.second, seqGenerateButtonWidth, seqGenerateButtonHeight);
    // generative sequencer new controls
    seqHarmonizerSlider->setBounds(seqHarmonizerLocation.first, seqHarmonizerLocation.second, sliderSmallSize, sliderSmallSize);
    seqLengthSlider->setBounds(seqLengthLocation.first, seqLengthLocation.second, sliderSmallSize, sliderSmallSize);
    seqShiftSlider->setBounds(seqShiftLocation.first, seqShiftLocation.second, sliderSmallSize, sliderSmallSize);
    // LFO controls
    lfoWaveformSlider->setBounds(lfoWaveformLocation.first, lfoWaveformLocation.second, sliderSmallSize, sliderSmallSize);
    lfoRateSlider->setBounds(lfoRateLocation.first, lfoRateLocation.second, sliderSmallSize, sliderSmallSize);
    lfoDepthSlider->setBounds(lfoDepthLocation.first, lfoDepthLocation.second, sliderSmallSize, sliderSmallSize);
    lfoDestinationSlider->setBounds(lfoDestinationLocation.first, lfoDestinationLocation.second, sliderSmallSize, sliderSmallSize);
}
