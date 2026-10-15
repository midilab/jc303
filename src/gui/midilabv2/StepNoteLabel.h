#pragma once

#include <JuceHeader.h>

// Step pitch name (C2, F#6, ...) with the sharp drawn slanted. The pixel font has no
// italic and JUCE won't fake one, so the '#' glyph is sheared, shrunk and raised by hand at paint time.
class StepNoteLabel : public juce::Label
{
public:
    static constexpr float kSharpShear = -0.3f; // x shift per unit of y; negative leans right
    static constexpr float kSharpRise = 0.2f;   // lift above the baseline, as a fraction of font height
    static constexpr float kSharpScale = 0.8f;  // size relative to the other glyphs

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

        // centre the run, squeezing horizontally if it doesn't fit (like Label's minimum scale)
        const float scale = juce::jmin(1.0f, area.getWidth() / juce::jmax(1.0f, textBounds.getWidth()));
        const float x0 = area.getCentreX() - textBounds.getWidth() * scale * 0.5f;
        const float baseline = area.getCentreY() - textBounds.getCentreY();
        const auto place = juce::AffineTransform::scale(scale, 1.0f).translated(x0 - textBounds.getX() * scale, baseline);

        g.setColour(findColour(juce::Label::textColourId));
        for (int i = 0; i < glyphs.getNumGlyphs(); ++i)
        {
            const auto& glyph = glyphs.getGlyph(i);
            auto transform = place;
            if (glyph.getCharacter() == '#')
            {
                const float cx = glyph.getLeft() + glyph.getBounds().getWidth() * 0.5f;
                const float cy = glyph.getBaselineY() - font.getAscent() * 0.5f;
                transform = juce::AffineTransform::translation(-cx, -cy)
                                .scaled(kSharpScale)
                                .sheared(kSharpShear, 0.0f)
                                .translated(cx, cy - font.getHeight() * kSharpRise)
                                .followedBy(place);
            }
            glyph.draw(g, transform);
        }
    }
};
