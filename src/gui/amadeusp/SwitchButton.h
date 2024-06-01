#pragma once

#include <JuceHeader.h>
#include "JuceLibraryCode/BinaryData.h"

class SwitchButton : public juce::Button
{
public:
    SwitchButton()
        : juce::Button("")
    {
        imageSwitch = juce::ImageCache::getFromMemory(BinaryData::switch_png, BinaryData::switch_pngSize);
    }

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        int frameHeight = imageSwitch.getHeight() / 2;
        int sourceY = getToggleState() ? 0 : frameHeight;

        juce::Rectangle<int> sourceRect(0, sourceY, imageSwitch.getWidth(), frameHeight);

        g.drawImage(imageSwitch, 0, 0, getWidth(), getHeight(),
                    sourceRect.getX(), sourceRect.getY(), sourceRect.getWidth(), sourceRect.getHeight(),
                    false);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        // Toggle the state when the mouse is released
        setToggleState(!getToggleState(), juce::sendNotification);
    }

private:
    juce::Image imageSwitch;
};