#pragma once

#include "JuceLibraryCode/BinaryData.h"

class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    KnobLookAndFeel(const juce::String& knobType)
    {
        // Load the image for the rotary knob according to type
        if (knobType == "small")
        {
            knobImage = juce::ImageCache::getFromMemory(BinaryData::smallrotary_png, BinaryData::smallrotary_pngSize);
        }
        else if (knobType == "medium")
        {
            knobImage = juce::ImageCache::getFromMemory(BinaryData::mediumrotary_png, BinaryData::mediumrotary_pngSize);
        }
        else if (knobType == "large")
        {
            knobImage = juce::ImageCache::getFromMemory(BinaryData::largerotary_png, BinaryData::largerotary_pngSize);
        }
    }

    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        const float radius = juce::jmin (width / 2, height / 2) - 4.0f;
        const float centreX = x + width * 0.5f;
        const float centreY = y + height * 0.5f;
        const float rx = centreX - radius;
        const float ry = centreY - radius;
        const float rw = radius * 2.0f;
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Draw the knob image
        g.drawImageTransformed (knobImage,
                                juce::AffineTransform::rotation (angle, knobImage.getWidth() * 0.5f, knobImage.getHeight() * 0.5f)
                                                     .translated (rx, ry)
                                                     .scaled (rw / knobImage.getWidth(), rw / knobImage.getHeight()));
    }

private:
    juce::Image knobImage;
};

