#pragma once

#include <JuceHeader.h>

class SwitchLed : public juce::Component,
                  public juce::AudioProcessorValueTreeState::Listener
{
public:
    SwitchLed(juce::AudioProcessorValueTreeState& vts, const juce::String& paramID)
        : valueTreeState(vts), paramID(paramID)
    {
        // Load the LED image from the binary data
        imageLed = juce::ImageCache::getFromMemory(BinaryData::darkledstepsequencer_png, BinaryData::darkledstepsequencer_pngSize);

        // Initialize the local state based on the current parameter value
        ledState = valueTreeState.getParameter(paramID)->getValue() > 0.5f;

        // Listen for changes to the parameter
        valueTreeState.addParameterListener(paramID, this);
    }

    ~SwitchLed() override
    {
        // Remove the listener
        valueTreeState.removeParameterListener(paramID, this);
    }

    void paint(juce::Graphics& g) override
    {
        // Ensure the image is loaded
        if (imageLed.isValid())
        {
            int frameHeight = imageLed.getHeight() / 2; 
            int sourceY = ledState == false ? 0 : frameHeight; 

            juce::Rectangle<int> sourceRect(0, sourceY, imageLed.getWidth(), frameHeight);

            g.drawImage(imageLed, 0, 0, getWidth(), getHeight(),
                        sourceRect.getX(), sourceRect.getY(), sourceRect.getWidth(), sourceRect.getHeight(),
                        false);
        }
    }

    // This function will be triggered when the value in the AudioProcessorValueTreeState changes
    void parameterChanged(const String& parameterID, float newValue) override
    {
        if (parameterID == paramID)
        {
            ledState = newValue > 0.5f;
            repaint();
        }
    }

private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::Image imageLed;
    juce::String paramID;

    bool ledState = false;
};