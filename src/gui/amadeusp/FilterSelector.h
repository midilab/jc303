#pragma once

#include <JuceHeader.h>
#include "JuceLibraryCode/BinaryData.h"

class FilterSelector : public juce::Button
{
public:
    FilterSelector()
        : juce::Button(""), currentState(0)
    {
        filterSelectorStrip = juce::ImageCache::getFromMemory(BinaryData::filter_png, BinaryData::filter_pngSize);
    }

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        int frameHeight = filterSelectorStrip.getHeight() / 12;
        int sourceY = currentState * frameHeight;

        juce::Rectangle<int> sourceRect(0, sourceY, filterSelectorStrip.getWidth(), frameHeight);

        g.drawImage(filterSelectorStrip, 0, 0, getWidth(), getHeight(),
                    sourceRect.getX(), sourceRect.getY(), sourceRect.getWidth(), sourceRect.getHeight(),
                    false);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        // Increment the state when the mouse is released and wrap around using modulo 12
        currentState = (currentState + 1) % 12;
        repaint();
    }

private:
    juce::Image filterSelectorStrip;
    int currentState;
};