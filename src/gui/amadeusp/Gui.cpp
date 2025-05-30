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
    
    setControlsLayout();

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (930, 363);
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

juce::Slider* JC303Editor::createKnob(const juce::String& knobType)
{
    auto* slider = new juce::Slider();
    slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    if (knobType == "small")
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
    const int acidSmileWidth = 56.25; //225/4;
    const int acidSmileHeight = 77.5; //310/4;

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
}