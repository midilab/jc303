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
    // MODs
    // row 1
    addAndMakeVisible(sqrDriverSlider = createKnob("small"));
    addAndMakeVisible(ampReleaseSlider = createKnob("small"));
    addAndMakeVisible(ampSustainSlider = createKnob("small"));
    addAndMakeVisible(slideTimeSlider = createKnob("small"));
    // row 2
    addAndMakeVisible(feedbackFilterSlider = createKnob("small"));
    addAndMakeVisible(softAttackSlider = createKnob("small"));
    addAndMakeVisible(normalDecaySlider = createKnob("small"));
    addAndMakeVisible(accentDecaySlider = createKnob("small"));
    // filter selector
    addAndMakeVisible(filterSelectorButton = createFilterSelector());
    // on/off mod switch
    addAndMakeVisible(switchModButton = createSwitch());

    // attch controls to processor parameters tree
    waveformAttachment.reset (new SliderAttachment (valueTreeState, "waveform", *waveformSlider));
    tuningAttachment.reset (new SliderAttachment (valueTreeState, "tuning", *tuningSlider));
    cutoffFreqAttachment.reset (new SliderAttachment (valueTreeState, "cutoff", *cutoffFreqSlider));
    resonanceAttachment.reset (new SliderAttachment (valueTreeState, "resonance", *resonanceSlider));
    envelopModAttachment.reset (new SliderAttachment (valueTreeState, "envmod", *envelopModSlider));
    decayAttachment.reset (new SliderAttachment (valueTreeState, "decay", *decaySlider));
    accentAttachment.reset (new SliderAttachment (valueTreeState, "accent", *accentSlider));
    volumeAttachment.reset (new SliderAttachment (valueTreeState, "volume", *volumeSlider));
    // MODs
    filterSelectorButtonAttachment.reset(new ButtonAttachment(valueTreeState, "filterSelector", *filterSelectorButton));
    switchModButtonAttachment.reset(new ButtonAttachment(valueTreeState, "switchModState", *switchModButton));
    sqrDriverAttachment.reset(new SliderAttachment(valueTreeState, "sqrDriver", *sqrDriverSlider));
    ampReleaseAttachment.reset(new SliderAttachment(valueTreeState, "ampSustain", *ampSustainSlider));
    ampSustainAttachment.reset(new SliderAttachment(valueTreeState, "ampRelease", *ampReleaseSlider));
    slideTimeAttachment.reset(new SliderAttachment(valueTreeState, "slideTime", *slideTimeSlider));
    feedbackFilterAttachment.reset(new SliderAttachment(valueTreeState, "feedbackFilter", *feedbackFilterSlider));
    softAttackAttachment.reset(new SliderAttachment(valueTreeState, "softAttack", *softAttackSlider));
    normalDecayAttachment.reset(new SliderAttachment(valueTreeState, "normalDecay", *normalDecaySlider));
    accentDecayAttachment.reset(new SliderAttachment(valueTreeState, "accentDecay", *accentDecaySlider));

    setControlsLayout();

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (900, 440);
}

JC303Editor::~JC303Editor()
{
}

//==============================================================================
void JC303Editor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);

    // main gui is a simple background with labels
    juce::Image background = ImageCache::getFromMemory (BinaryData::jc303gui_png, BinaryData::jc303gui_pngSize);
    g.drawImageAt (background, 0, 0);
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

FilterSelector* JC303Editor::createFilterSelector()
{
    auto* filterSelector = new FilterSelector();
    filterSelector->setClickingTogglesState(false);

    return filterSelector;
}

void JC303Editor::setControlsLayout()
{
    // Set the bounds and other properties for each slider
    // Adjust the parameters accordingly to fit your needs
    const int sliderLargeSize = 70;
    const int sliderMediumSize = 60;
    const int sliderSmallSize = 30;
    const int switchWidth = 50;
    const int switchHeight = 18;
    const int filterSelectorWidth = 100;
    const int filterSelectorHeight = 50;

    // knob positioning location
    // first row
    pair<int, int> waveFormLocation = {45, 56}; 
    pair<int, int> volumeLocation = {783, 56}; 
    // second row
    pair<int, int> tuningLocation = {173, 153}; 
    pair<int, int> cutoffFreqLocation = {272, 153}; 
    pair<int, int> resonanceLocation = {370, 153}; 
    pair<int, int> envelopeLocation = {469, 153}; 
    pair<int, int> decayLocation = {567, 153}; 
    pair<int, int> accentLocation = {667, 153}; 
    // MOD Switch
    pair<int, int> switchLocation = {58, 291};
    // MODs knobs
    // first row
    pair<int, int> sqrDriverLocation = {156, 294};
    pair<int, int> ampReleaseLocation = {224, 294};
    pair<int, int> ampSustainLocation = {292, 294};
    pair<int, int> slideTimeLocation = {361, 294};
    // second row
    pair<int, int> feedbackFilterLocation = {430, 294};
    pair<int, int> softAttackLocation = {508, 294};
    pair<int, int> normalAttackLocation = {587, 294};
    pair<int, int> accentDecayLocation = {666, 294};
    pair<int, int> filterSelectorLocation = {740, 275};

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
    // MODs, small knobs
    sqrDriverSlider->setBounds(sqrDriverLocation.first, sqrDriverLocation.second, sliderSmallSize, sliderSmallSize);
    ampReleaseSlider->setBounds(ampReleaseLocation.first, ampReleaseLocation.second, sliderSmallSize, sliderSmallSize);
    ampSustainSlider->setBounds(ampSustainLocation.first, ampSustainLocation.second, sliderSmallSize, sliderSmallSize);
    slideTimeSlider->setBounds(slideTimeLocation.first, slideTimeLocation.second, sliderSmallSize, sliderSmallSize);
    feedbackFilterSlider->setBounds(feedbackFilterLocation.first, feedbackFilterLocation.second, sliderSmallSize, sliderSmallSize);
    softAttackSlider->setBounds(softAttackLocation.first, softAttackLocation.second, sliderSmallSize, sliderSmallSize);
    normalDecaySlider->setBounds(normalAttackLocation.first, normalAttackLocation.second, sliderSmallSize, sliderSmallSize);
    accentDecaySlider->setBounds(accentDecayLocation.first, accentDecayLocation.second, sliderSmallSize, sliderSmallSize);

    switchModButton->setBounds(switchLocation.first, switchLocation.second, switchWidth, switchHeight);
    filterSelectorButton->setBounds(filterSelectorLocation.first, filterSelectorLocation.second, filterSelectorWidth, filterSelectorHeight);

    
}