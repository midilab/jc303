#pragma once

#include <JuceHeader.h>

class SwitchStepSeqButton : public juce::Button
{
public:
    enum class Mode
    {
        Toggle,
        Press
    };

    explicit SwitchStepSeqButton(Mode mode = Mode::Toggle, const juce::String& labelText = "")
        : juce::Button(""),
          buttonMode(mode),
          buttonLabelText(labelText)
    {
    }

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        juce::Image imageButton;

        if (buttonMode == Mode::Toggle)
        {
            imageButton = juce::ImageCache::getFromMemory(BinaryData::largebuttonstepsequencer_png, BinaryData::largebuttonstepsequencer_pngSize);
        }
        else
        {
            imageButton = juce::ImageCache::getFromMemory(BinaryData::smallbuttonstepsequencer_png, BinaryData::smallbuttonstepsequencer_pngSize);
        }

        int frameHeight = imageButton.getHeight() / 2;
        int sourceY;

        if (buttonMode == Mode::Toggle)
        {
            sourceY = getToggleState() ? frameHeight : 0;
        }
        else
        {
            sourceY = isButtonDown ? frameHeight : 0;
        }

        juce::Rectangle<int> sourceRect(0, sourceY, imageButton.getWidth(), frameHeight);

        g.drawImage(imageButton, 0, 0, getWidth(), getHeight(),
                    sourceRect.getX(), sourceRect.getY(), sourceRect.getWidth(), sourceRect.getHeight(),
                    false);

        if (!buttonLabelText.isEmpty())
        {
            g.setColour(juce::Colour(28, 28, 28));
            g.setFont(juce::Font(10.0f));
            g.drawText(buttonLabelText, getLocalBounds(), juce::Justification::centred, false);
        }
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
    Mode buttonMode;
    juce::String buttonLabelText;
};
