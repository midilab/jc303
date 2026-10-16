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

        // red variant of the lit frame: swap the red and green channels
        imageSwitchAlt = imageSwitch.createCopy();
        juce::Image::BitmapData pixels(imageSwitchAlt, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < pixels.height; ++y)
            for (int x = 0; x < pixels.width; ++x)
            {
                auto c = pixels.getPixelColour(x, y);
                pixels.setPixelColour(x, y, juce::Colour(c.getGreen(), c.getRed(), c.getBlue(), c.getAlpha()));
            }

        imageSecondary = makeBlueGlowFrame(imageSwitch, 2, 0, 1);
    }

    // Shift-click calls onShiftClick instead of toggling, when set
    std::function<void()> onShiftClick;

    // Draws the lit frame red instead of green
    void setAltState(bool shouldBeAlt)
    {
        if (altState != shouldBeAlt)
        {
            altState = shouldBeAlt;
            repaint();
        }
    }

    // Lit frame shows blue while the secondary state is set
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

        g.drawImage(altState ? imageSwitchAlt : imageSwitch, 0, 0, getWidth(), getHeight(),
                    sourceRect.getX(), sourceRect.getY(), sourceRect.getWidth(), sourceRect.getHeight(),
                    false);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (event.mods.isShiftDown() && onShiftClick)
        {
            onShiftClick();
            return;
        }
        // Toggle the state when the mouse is released
        setToggleState(!getToggleState(), juce::sendNotification);
    }

private:
    juce::Image imageSwitch;
    juce::Image imageSwitchAlt;
    juce::Image imageSecondary;
    bool altState = false;
    bool secondary = false;
};