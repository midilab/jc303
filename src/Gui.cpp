#include "Gui.h"

//==============================================================================
JC303Editor::JC303Editor (JC303& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);

    // Create and configure rotary sliders for each parameter
    addAndMakeVisible(waveFormSlider = createSlider(processorRef.waveForm));
    addAndMakeVisible(tuningSlider = createSlider(processorRef.tuning));
    addAndMakeVisible(cutoffFreqSlider = createSlider(processorRef.cutoffFreq));
    addAndMakeVisible(resonanceSlider = createSlider(processorRef.resonance));
    addAndMakeVisible(envelopModSlider = createSlider(processorRef.envelopMod));
    addAndMakeVisible(decaySlider = createSlider(processorRef.decay));
    addAndMakeVisible(accentSlider = createSlider(processorRef.accent));
    addAndMakeVisible(volumeSlider = createSlider(processorRef.volume));

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

    // main background with labels
    juce::Image background = ImageCache::getFromMemory (BinaryData::jc303gui_png, BinaryData::jc303gui_pngSize);
    g.drawImageAt (background, 0, 0);
    //g.drawFittedText ("JC303", getLocalBounds(), juce::Justification::centred, 1);
}

void JC303Editor::resized()
{
    //knobComponent.setBounds(50, 50, 100, 100);
    setControlsLayout();
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}

juce::Slider* JC303Editor::createSlider(juce::AudioParameterFloat* parameter)
{
    auto* slider = new juce::Slider();
    slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider->setLookAndFeel(&lookAndFeel);
    //slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    slider->setTextBoxStyle(juce::Slider::TextEntryBoxPosition::NoTextBox, true, 0, 0);
    slider->setRange(parameter->range.start, parameter->range.end);
    slider->setValue(*parameter);
    slider->addListener(this);
    return slider;
}

void JC303Editor::sliderValueChanged(juce::Slider* slider)
{
    if (slider == waveFormSlider)
        processorRef.waveForm->setValueNotifyingHost(slider->getValue());
    else if (slider == tuningSlider)
        processorRef.tuning->setValueNotifyingHost(slider->getValue());
    else if (slider == cutoffFreqSlider)
        processorRef.cutoffFreq->setValueNotifyingHost(slider->getValue());
    else if (slider == resonanceSlider)
        processorRef.resonance->setValueNotifyingHost(slider->getValue());
    else if (slider == envelopModSlider)
        processorRef.envelopMod->setValueNotifyingHost(slider->getValue());
    else if (slider == decaySlider)
        processorRef.decay->setValueNotifyingHost(slider->getValue());
    else if (slider == accentSlider)
        processorRef.accent->setValueNotifyingHost(slider->getValue());
    else if (slider == volumeSlider)
        processorRef.volume->setValueNotifyingHost(slider->getValue());
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

    waveFormSlider->setBounds(waveFormLocation.first, waveFormLocation.second, sliderWidth, sliderHeight);
    volumeSlider->setBounds(volumeLocation.first, volumeLocation.second, sliderWidth, sliderHeight);
    tuningSlider->setBounds(tuningLocation.first, tuningLocation.second, sliderWidth, sliderHeight);
    cutoffFreqSlider->setBounds(cutoffFreqLocation.first, cutoffFreqLocation.second, sliderWidth, sliderHeight);
    resonanceSlider->setBounds(resonanceLocation.first, resonanceLocation.second, sliderWidth, sliderHeight);
    envelopModSlider->setBounds(envelopeLocation.first, envelopeLocation.second, sliderWidth, sliderHeight);
    decaySlider->setBounds(decayLocation.first, decayLocation.second, sliderWidth, sliderHeight);
    accentSlider->setBounds(accentLocation.first, accentLocation.second, sliderWidth, sliderHeight);
}