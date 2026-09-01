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
        float scale = (float) getWidth() / imageButton.getWidth();
        int buttonFrameHeight = (int) (imageButton.getHeight() / 2.0f * scale);
        int sourceY;

        if (buttonMode == Mode::Toggle)
        {
            sourceY = getToggleState() ? (int)(imageButton.getHeight() / 2.0f) : 0;
        }
        else
        {
            sourceY = isButtonDown ? (int)(imageButton.getHeight() / 2.0f) : 0;
        }

        int buttonY = 0;

        g.drawImage(imageButton, 0, buttonY, getWidth(), buttonFrameHeight,
                    0, sourceY, imageButton.getWidth(), imageButton.getHeight() / 2,
                    false);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (buttonMode == Mode::Press)
        {
            setToggleState(true, juce::sendNotification);
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
            setToggleState(false, juce::sendNotification);
        }
    }

private:
    juce::Image imageButton;
    Mode buttonMode;
    Size buttonSize;
};
