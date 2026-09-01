#pragma once

#include <JuceHeader.h>

class MenuSwitchButton : public juce::Button
{
public:
    enum class Mode
    {
        Toggle,
        Press
    };

    explicit MenuSwitchButton(Mode mode = Mode::Toggle)
        : juce::Button(""), buttonMode(mode)
    {
        imageSwitch = juce::ImageCache::getFromMemory(BinaryData::switch2_png, BinaryData::switch2_pngSize);
    }

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        int frameHeight = imageSwitch.getHeight() / 2;
        int sourceY = (buttonMode == Mode::Toggle ? getToggleState() : isButtonDown) ? frameHeight : 0;

        juce::Rectangle<int> sourceRect(0, sourceY, imageSwitch.getWidth(), frameHeight);

        g.setOpacity(isEnabled() ? 1.0f : 0.35f);
        g.drawImage(imageSwitch, 0, 0, getWidth(), getHeight(),
                    sourceRect.getX(), sourceRect.getY(), sourceRect.getWidth(), sourceRect.getHeight(),
                    false);
        g.setOpacity(1.0f);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        if (buttonMode == Mode::Press)
        {
            setToggleState(true, juce::sendNotification);
            if (onPress != nullptr)
                onPress();
        }
    }

    void mouseUp(const juce::MouseEvent&) override
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

    std::function<void()> onPress;

private:
    juce::Image imageSwitch;
    Mode buttonMode;
};
