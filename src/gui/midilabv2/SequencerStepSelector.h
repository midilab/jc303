#pragma once

#include <JuceHeader.h>

// Small LED used to visualise the sequencer step state; clicking it selects the
// step to edit. The image is a 3-frame vertical strip: OFF (top), ON (middle),
// and playing (bottom). setState() repaints only when the state actually changes.
class SequencerStepSelector : public juce::Component
{
public:
    SequencerStepSelector()
    {
        imageLed = juce::ImageCache::getFromMemory(BinaryData::sequencer_step_selector_png, BinaryData::sequencer_step_selector_pngSize);
        setState(0);
    }

    void setState(int state)
    {
        if (ledState == state) return;
        ledState = state;
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
            const int frameHeight = imageLed.getHeight() / 3;
            const int sourceY = ledState * frameHeight;

            g.drawImage(imageLed, 0,  0, getWidth(), getHeight(),
                        0, sourceY, imageLed.getWidth(), frameHeight,
                        false);
        }
    }

private:
    juce::Image imageLed;
    int ledState = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerStepSelector)
};