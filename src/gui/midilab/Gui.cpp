#include "Gui.h"

//==============================================================================
JC303Editor::JC303Editor (JC303& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), processorRef (p), valueTreeState (vts)
{
    // Create and configure rotary sliders for each parameter
    addAndMakeVisible(waveformSlider = create303Knob());
    addAndMakeVisible(tuningSlider = create303Knob());
    addAndMakeVisible(cutoffFreqSlider = create303Knob());
    addAndMakeVisible(resonanceSlider = create303Knob());
    addAndMakeVisible(envelopModSlider = create303Knob());
    addAndMakeVisible(decaySlider = create303Knob());
    addAndMakeVisible(accentSlider = create303Knob());
    addAndMakeVisible(volumeSlider = create303Knob());
    // MODs
    addAndMakeVisible(normalDecaySlider = createModKnob("n_decay"));
    addAndMakeVisible(accentDecaySlider = createModKnob("a_decay"));
    addAndMakeVisible(feedbackFilterSlider = createModKnob("filter_q"));
    addAndMakeVisible(softAttackSlider = createModKnob("s_attack"));
    addAndMakeVisible(slideTimeSlider = createModKnob("slide_t"));
    addAndMakeVisible(sqrDriverSlider = createModKnob("s_drive"));
    // on/off mod switch
    addAndMakeVisible(switchModButton = createSwitch("mod on"));

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
    switchModButtonAttachment.reset(new ButtonAttachment(valueTreeState, "switchModState", *switchModButton));
    sqrDriverAttachment.reset(new SliderAttachment(valueTreeState, "sqrDriver", *sqrDriverSlider));
    slideTimeAttachment.reset(new SliderAttachment(valueTreeState, "slideTime", *slideTimeSlider));
    feedbackFilterAttachment.reset(new SliderAttachment(valueTreeState, "feedbackFilter", *feedbackFilterSlider));
    softAttackAttachment.reset(new SliderAttachment(valueTreeState, "softAttack", *softAttackSlider));
    normalDecayAttachment.reset(new SliderAttachment(valueTreeState, "normalDecay", *normalDecaySlider));
    accentDecayAttachment.reset(new SliderAttachment(valueTreeState, "accentDecay", *accentDecaySlider));

    setControlsLayout();

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (745, 288);
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

juce::Slider* JC303Editor::create303Knob()
{
    auto* slider = new juce::Slider();
    slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider->setLookAndFeel(&knobLookAndFeel);
    slider->setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
    return slider;
}

juce::Slider* JC303Editor::createModKnob(const juce::String& label)
{
    auto* slider = new juce::Slider();
    slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider->setLookAndFeel(&modKnobLookAndFeel);
    slider->setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);

    // Create a label for the text
    auto* labelComponent = new AttachedLabel();
    labelComponent->setText(label, juce::dontSendNotification);
    labelComponent->setJustificationType(juce::Justification::centredTop);
    labelComponent->setColour(juce::Label::textColourId, juce::Colours::black);
    labelComponent->attachToComponent(slider, true);

    return slider;
}

SwitchButton* JC303Editor::createSwitch(const juce::String& label)
{
    //auto* button = new juce::ImageButton();
    //button->setLookAndFeel(&buttonLookAndFeel);
    auto* button = new SwitchButton(label);
    button->setClickingTogglesState(false);

    // Create a label for the text
    auto* labelComponent = new AttachedLabel();
    labelComponent->setText(label, juce::dontSendNotification);
    labelComponent->setJustificationType(juce::Justification::centredTop);
    labelComponent->setColour(juce::Label::textColourId, juce::Colours::black);
    labelComponent->attachToComponent(button, true);

    return button;
}

void JC303Editor::setControlsLayout()
{
    // Set the bounds and other properties for each slider
    // Adjust the parameters accordingly to fit your needs
    const int sliderWidth = 120;
    const int sliderHeight = 120;

    // knob positioning location
    // first row
    pair<int, int> waveFormLocation = {9, 20}; 
    pair<int, int> volumeLocation = {130, 20}; 
    // second row
    pair<int, int> tuningLocation = {9, 160}; 
    pair<int, int> cutoffFreqLocation = {130, 160}; 
    pair<int, int> resonanceLocation = {252, 160}; 
    pair<int, int> envelopeLocation = {374, 160}; 
    pair<int, int> decayLocation = {496, 160}; 
    pair<int, int> accentLocation = {618, 160}; 
    // MOD Switch
    pair<int, int> switchLocation = {395, 21};
    // MODs knobs
    pair<int, int> normalDecayLocation = {450, 34};
    pair<int, int> accentDecayLocation = {502, 34};
    pair<int, int> feedbackFilterLocation = {554, 34};
    pair<int, int> softAttackLocation = {606, 34};
    pair<int, int> slideTimeLocation = {658, 34};
    pair<int, int> sqrDriverLocation = {710, 34};

    waveformSlider->setBounds(waveFormLocation.first, waveFormLocation.second, sliderWidth, sliderHeight);
    volumeSlider->setBounds(volumeLocation.first, volumeLocation.second, sliderWidth, sliderHeight);
    tuningSlider->setBounds(tuningLocation.first, tuningLocation.second, sliderWidth, sliderHeight);
    cutoffFreqSlider->setBounds(cutoffFreqLocation.first, cutoffFreqLocation.second, sliderWidth, sliderHeight);
    resonanceSlider->setBounds(resonanceLocation.first, resonanceLocation.second, sliderWidth, sliderHeight);
    envelopModSlider->setBounds(envelopeLocation.first, envelopeLocation.second, sliderWidth, sliderHeight);
    decaySlider->setBounds(decayLocation.first, decayLocation.second, sliderWidth, sliderHeight);
    accentSlider->setBounds(accentLocation.first, accentLocation.second, sliderWidth, sliderHeight);
    // MODs
    sqrDriverSlider->setBounds(sqrDriverLocation.first, sqrDriverLocation.second, sliderWidth / 4, sliderHeight / 4);
    slideTimeSlider->setBounds(slideTimeLocation.first, slideTimeLocation.second, sliderWidth / 4, sliderHeight / 4);
    feedbackFilterSlider->setBounds(feedbackFilterLocation.first, feedbackFilterLocation.second, sliderWidth / 4, sliderHeight / 4);
    softAttackSlider->setBounds(softAttackLocation.first, softAttackLocation.second, sliderWidth / 4, sliderHeight / 4);
    normalDecaySlider->setBounds(normalDecayLocation.first, normalDecayLocation.second, sliderWidth / 4, sliderHeight / 4);
    accentDecaySlider->setBounds(accentDecayLocation.first, accentDecayLocation.second, sliderWidth / 4, sliderHeight / 4);

    switchModButton->setBounds(switchLocation.first, switchLocation.second, sliderWidth / 2, sliderHeight / 2);
}