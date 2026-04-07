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

    explicit SwitchStepSeqButton(Mode mode = Mode::Toggle, Size size = Size::Large, bool useLedIndicator = false, int8_t ledOffset = 0)
        : juce::Button(""),
          buttonMode(mode),
          buttonSize(size),
          useLed(useLedIndicator),
          ledShift(ledOffset)
    {
        if (buttonSize == Size::Large)
        {
            imageButton = juce::ImageCache::getFromMemory(BinaryData::largebuttonstepsequencer_png, BinaryData::largebuttonstepsequencer_pngSize);
        }
        else if (buttonSize == Size::Medium)
        {
            imageButton = juce::ImageCache::getFromMemory(BinaryData::mediumbuttonstepsequencer_png, BinaryData::mediumbuttonstepsequencer_pngSize);
        }
        else
        {
            imageButton = juce::ImageCache::getFromMemory(BinaryData::smallbuttonstepsequencer_png, BinaryData::smallbuttonstepsequencer_pngSize);
        }

        if (useLed)
        {
            ledImage = juce::ImageCache::getFromMemory(BinaryData::darkledstepsequencer_png, BinaryData::darkledstepsequencer_pngSize);
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

        if (useLed && ledImage.isValid())
        {
            int ledFrameHeight = (int) (ledImage.getHeight() / 2.0f * scale);
            int ledWidth = (int) (ledImage.getWidth() * scale);
            int ledSourceY = ((buttonMode == Mode::Toggle) ? getToggleState() : isButtonDown) ? (int)(ledImage.getHeight() / 2.0f) : 0;
            int scaledGap = (int) (4.0f * scale);

            buttonY = ledFrameHeight + scaledGap;

            int ledX = (getWidth() - ledWidth) / 2 + (int)(ledShift * scale);
            g.drawImage(ledImage, ledX, 0, ledWidth, ledFrameHeight,
                        0, ledSourceY, ledImage.getWidth(), ledImage.getHeight() / 2,
                        false);
        }

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
    juce::Image ledImage;
    Mode buttonMode;
    Size buttonSize;
    bool useLed;
    int8_t ledShift;
};
