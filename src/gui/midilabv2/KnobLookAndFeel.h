#pragma once

//==============================================================================
class KnobLookAndFeel : public LookAndFeel_V4
{
public:
    KnobLookAndFeel()
    {
        imageKnob = ImageCache::getFromMemory(BinaryData::knob_large_silver_png, BinaryData::knob_large_silver_pngSize);
    }

//==============================================================================
void drawRotarySlider(Graphics& g,
                                       int x, int y, int width, int height, float sliderPos,
                                       float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    
    if (imageKnob.isValid())
    {
        const double range = slider.getMaximum() - slider.getMinimum();
        const double rotation = range > 0.0
        ? (slider.getValue() - slider.getMinimum()) / range
        : 0.0;
        
        const int frames = imageKnob.getHeight() / imageKnob.getWidth();
        const int frameId = juce::jlimit (0, jmax (0, frames - 1),
                                          (int)ceil(rotation * ((double)frames - 1.0)));
        const float radius = jmin(width / 2.0f, height / 2.0f);
        const float centerX = x + width * 0.5f;
        const float centerY = y + height * 0.5f;
        const float rx = centerX - radius - 1.0f;
        const float ry = centerY - radius;
        
        g.setOpacity(slider.isEnabled() ? 1.0f : 0.35f);
        g.drawImage(imageKnob,(int)rx,(int)ry,2 * (int)radius,2 * (int)radius,0,frameId*imageKnob.getWidth(),imageKnob.getWidth(), imageKnob.getWidth());
    }

}
    juce::Image imageKnob;
    //==============================================================================
    
};
