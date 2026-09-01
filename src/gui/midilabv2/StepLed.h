#pragma once

#include <JuceHeader.h>

// Small display-only LED used to visualise the step rest state of the sequencer.
// The sprite (sequencer_led.png) is a vertical two-frame strip: off = top
// frame, on = bottom frame.  setOn() repaints only when the state actually changes.
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