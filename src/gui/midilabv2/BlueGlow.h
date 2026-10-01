#pragma once

#include <JuceHeader.h>

// Builds the "secondary" (blue) frame of a button strip: the OFF frame plus only the
// light the ON frame adds (the lit glow), recoloured blue, so the button body and
// edge stay untouched. frameCount frames are stacked vertically in the strip.
inline juce::Image makeBlueGlowFrame(const juce::Image& strip, int frameCount, int offFrame, int onFrame)
{
    if (! strip.isValid())
        return {};

    const int w = strip.getWidth();
    const int h = strip.getHeight() / frameCount;
    const auto off = strip.getClippedImage({ 0, offFrame * h, w, h });
    const auto on  = strip.getClippedImage({ 0, onFrame * h, w, h });
    juce::Image out(juce::Image::ARGB, w, h, true);

    const auto lum = [] (juce::Colour c)
    {
        return 0.299f * c.getFloatRed() + 0.587f * c.getFloatGreen() + 0.114f * c.getFloatBlue();
    };

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const auto c0 = off.getPixelAt(x, y);
            const auto c1 = on.getPixelAt(x, y);
            const float added = juce::jmax(0.0f, lum(c1) - lum(c0));
            out.setPixelAt(x, y, juce::Colour::fromFloatRGBA(
                juce::jlimit(0.0f, 1.0f, c0.getFloatRed()   + added * 0.25f),
                juce::jlimit(0.0f, 1.0f, c0.getFloatGreen() + added * 0.55f),
                juce::jlimit(0.0f, 1.0f, c0.getFloatBlue()  + added * 1.0f),
                c1.getFloatAlpha()));
        }
    }

    return out;
}
