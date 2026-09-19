#pragma once

#include <JuceHeader.h>
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
    }

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        const int frameHeight = imageButton.getHeight() / 2;
        const int sourceY = buttonMode == Mode::Toggle
                                ? (getToggleState() ? frameHeight : 0)
                                : (pressed ? frameHeight : 0);

        if (buttonSize == Size::Small)
        {
            // square buttons (clear/rec/rest): stretch the frame to fill the
            // full (square) bounds instead of the artwork's landscape aspect
            g.drawImage(imageButton, 0, 0, getWidth(), getHeight(),
                        0, sourceY, imageButton.getWidth(), frameHeight,
                        false);
            return;
        }

        const float scale = (float) getWidth() / imageButton.getWidth();
        const int buttonFrameHeight = jmin((int) (imageButton.getHeight() / 2.0f * scale), getHeight());
        g.drawImage(imageButton, 0, 0, getWidth(), buttonFrameHeight,
                    0, sourceY, imageButton.getWidth(), frameHeight,
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
    Mode buttonMode;
    Size buttonSize;
    bool pressed = false;
};
