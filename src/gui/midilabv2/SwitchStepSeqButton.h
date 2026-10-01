#pragma once

#include <JuceHeader.h>
#include "BlueGlow.h"
#include <cstdint>

class SwitchStepSeqButton : public juce::Button
{
public:
    enum class Mode
    {
        Toggle,
        Press
    };

    enum class Size
    {
        Large,
        Medium,
        Small
    };

    explicit SwitchStepSeqButton(Mode mode = Mode::Toggle, Size size = Size::Large)
        : juce::Button(""),
          buttonMode(mode),
          buttonSize(size)
    {
        if (buttonSize == Size::Large)
        {
            imageButton = juce::ImageCache::getFromMemory(BinaryData::sequencerbutton1_png, BinaryData::sequencerbutton1_pngSize);
        }
        else
        {
            imageButton = juce::ImageCache::getFromMemory(BinaryData::sequencerbutton2_png, BinaryData::sequencerbutton2_pngSize);
        }
        imageSecondary = makeBlueGlowFrame(imageButton, 2, 0, 1, juce::Colour::fromFloatRGBA(0.2f, 1.0f, 0.3f, 1.0f));
    }

    // Shift-click runs onShiftClick instead of toggling; the lit frame shows green
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
        const int frameHeight = imageButton.getHeight() / 2;
        const bool litBlue = secondary && getToggleState() && imageSecondary.isValid();
        const int sourceY = buttonMode == Mode::Toggle
                                ? (getToggleState() ? frameHeight : 0)
                                : (pressed ? frameHeight : 0);

        const juce::Image& img = litBlue ? imageSecondary : imageButton;
        const int srcY = litBlue ? 0 : sourceY;

        if (buttonSize == Size::Small)
        {
            // square buttons (clear/rec/rest): stretch the frame to fill the
            // full (square) bounds instead of the artwork's landscape aspect
            g.drawImage(img, 0, 0, getWidth(), getHeight(),
                        0, srcY, img.getWidth(), frameHeight,
                        false);
            return;
        }

        const float scale = (float) getWidth() / imageButton.getWidth();
        const int buttonFrameHeight = jmin((int) (imageButton.getHeight() / 2.0f * scale), getHeight());
        g.drawImage(img, 0, 0, getWidth(), buttonFrameHeight,
                    0, srcY, img.getWidth(), frameHeight,
                    false);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (buttonMode == Mode::Press)
        {
            pressed = true;
            repaint();
            if (onPress != nullptr)
                onPress();
        }
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (buttonMode == Mode::Toggle)
        {
            if (event.mods.isShiftDown() && onShiftClick != nullptr)
            {
                onShiftClick();
                return;
            }
            setToggleState(!getToggleState(), juce::sendNotification);
        }
        else
        {
            pressed = false;
            repaint();
        }
    }

std::function<void()> onPress;

private:
    juce::Image imageButton;
    juce::Image imageSecondary;
    bool secondary = false;
    Mode buttonMode;
    Size buttonSize;
    bool pressed = false;
};
