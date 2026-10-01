#pragma once

#include <JuceHeader.h>
#include "BlueGlow.h"

class SwitchButton : public juce::Button
{
public:
    SwitchButton()
        : juce::Button("")
    {
        imageSwitch = juce::ImageCache::getFromMemory(BinaryData::switch_png, BinaryData::switch_pngSize);
        imageSecondary = makeBlueGlowFrame(imageSwitch, 2, 0, 1, juce::Colour::fromFloatRGBA(1.0f, 0.2f, 0.15f, 1.0f));
    }

    // Shift-click runs onShiftClick instead of toggling; the lit frame shows blue
    // while the secondary state is set.
    std::function<void()> onShiftClick;
    void setSecondary(bool on)
    {
        if (secondary == on) return;
        secondary = on;
        repaint();
    }

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        int frameHeight = imageSwitch.getHeight() / 2;
        int sourceY = getToggleState() ? frameHeight : 0;

        if (getToggleState() && secondary && imageSecondary.isValid())
        {
            g.drawImage(imageSecondary, 0, 0, getWidth(), getHeight(),
                        0, 0, imageSecondary.getWidth(), imageSecondary.getHeight(),
                        false);
            return;
        }

        juce::Rectangle<int> sourceRect(0, sourceY, imageSwitch.getWidth(), frameHeight);

        g.drawImage(imageSwitch, 0, 0, getWidth(), getHeight(),
                    sourceRect.getX(), sourceRect.getY(), sourceRect.getWidth(), sourceRect.getHeight(),
                    false);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (event.mods.isShiftDown() && onShiftClick != nullptr)
        {
            onShiftClick();
            return;
        }

        // Toggle the state when the mouse is released
        setToggleState(!getToggleState(), juce::sendNotification);
    }

private:
    juce::Image imageSwitch;
    juce::Image imageSecondary;
    bool secondary = false;
};