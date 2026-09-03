#pragma once

#include <JuceHeader.h>

// Small LED used to visualise the sequencer step state; clicking it selects the
// step to edit (the top/bottom frames are off/on). setOn() repaints only when
// the state actually changes.
class StepLed : public juce::Component
{
public:
    StepLed()
    {
        imageLed = juce::ImageCache::getFromMemory(BinaryData::sequencer_led_png, BinaryData::sequencer_led_pngSize);
        setOn(false);
    }

    void setOn(bool on)
    {
        if (ledOn == on) return;
        ledOn = on;
        repaint();
    }

    // Clicking an LED selects the corresponding sequencer step (wired by the editor).
    std::function<void()> onClick;

    void mouseDown(const juce::MouseEvent&) override
    {
        if (onClick != nullptr)
            onClick();
    }

    void paint(juce::Graphics& g) override
    {
        if (imageLed.isValid())
        {
            const int frameHeight = imageLed.getHeight() / 2;
            const int sourceY = ledOn ? frameHeight : 0;

            g.drawImage(imageLed, 0,  0, getWidth(), getHeight(),
                        0, sourceY, imageLed.getWidth(), frameHeight,
                        false);
        }
    }

private:
    juce::Image imageLed;
    bool ledOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepLed)
};