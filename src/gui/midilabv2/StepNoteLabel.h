#pragma once

#include <JuceHeader.h>

// Step pitch name (C2, F#6, ...) with the sharp drawn slanted. The pixel font has no
// italic and JUCE won't fake one, so the '#' glyph is sheared, shrunk and raised by hand at paint time.
// The '-' of octave -1 is drawn shorter and its cell narrowed so "F#-1" fits the button.
class StepNoteLabel : public juce::Label
{
public:
    static constexpr float kSharpShear = -0.3f; // x shift per unit of y; negative leans right
    static constexpr float kSharpRise = 0.2f;   // lift above the baseline, as a fraction of font height
    static constexpr float kSharpScale = 0.8f;  // size relative to the other glyphs
    static constexpr float kMinusScale = 0.5f;  // width (and advance) of the '-' relative to a full cell

    void paint(juce::Graphics& g) override
    {
        const auto text = getText();
        if (text.isEmpty())
            return;

        const auto font = getFont();
        const auto area = getLocalBounds().toFloat();

        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText(font, text, 0.0f, 0.0f);
        const auto textBounds = glyphs.getBoundingBox(0, -1, true);

        float minusSaving = 0.0f;
        for (int i = 0; i < glyphs.getNumGlyphs(); ++i)
            if (glyphs.getGlyph(i).getCharacter() == '-')
                minusSaving += glyphs.getGlyph(i).getBounds().getWidth() * (1.0f - kMinusScale);
        const float textWidth = textBounds.getWidth() - minusSaving;

        // centre the run, squeezing horizontally if it doesn't fit (like Label's minimum scale)
        const float scale = juce::jmin(1.0f, area.getWidth() / juce::jmax(1.0f, textWidth));
        const float x0 = area.getCentreX() - textWidth * scale * 0.5f;
        const float baseline = area.getCentreY() - textBounds.getCentreY();
        const auto place = juce::AffineTransform::scale(scale, 1.0f).translated(x0 - textBounds.getX() * scale, baseline);

        g.setColour(findColour(juce::Label::textColourId));
        float pullLeft = 0.0f; // accumulated advance removed by narrowed '-' glyphs
        for (int i = 0; i < glyphs.getNumGlyphs(); ++i)
        {
            const auto& glyph = glyphs.getGlyph(i);
            auto transform = juce::AffineTransform::translation(-pullLeft, 0.0f).followedBy(place);
            if (glyph.getCharacter() == '-')
            {
                const float cellWidth = glyph.getBounds().getWidth();
                const float cx = glyph.getLeft() + cellWidth * 0.5f;
                transform = juce::AffineTransform::translation(-cx, 0.0f)
                                .scaled(kMinusScale, 1.0f)
                                .translated(glyph.getLeft() + cellWidth * kMinusScale * 0.5f - pullLeft, 0.0f)
                                .followedBy(place);
                pullLeft += cellWidth * (1.0f - kMinusScale);
            }
            else if (glyph.getCharacter() == '#')
            {
                const float cx = glyph.getLeft() + glyph.getBounds().getWidth() * 0.5f;
                const float cy = glyph.getBaselineY() - font.getAscent() * 0.5f;
                transform = juce::AffineTransform::translation(-cx, -cy)
                                .scaled(kSharpScale)
                                .sheared(kSharpShear, 0.0f)
                                .translated(cx - pullLeft, cy - font.getHeight() * kSharpRise)
                                .followedBy(place);
            }
            glyph.draw(g, transform);
        }
    }
};
