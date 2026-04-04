#pragma once

//==============================================================================
class ModKnobLookAndFeel : public LookAndFeel_V4
{
public:
    ModKnobLookAndFeel()
    {
        imageKnob = ImageCache::getFromMemory(BinaryData::jc303modknob_png, BinaryData::jc303modknob_pngSize);
    }

//==============================================================================
void drawRotarySlider(Graphics& g,
                                       int x, int y, int width, int height, float sliderPos,
                                       float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{

    if (imageKnob.isValid())
    {
        const double rotation = (slider.getValue()
                                 - slider.getMinimum())
        / (slider.getMaximum()
           - slider.getMinimum());

        const int frames = imageKnob.getHeight() / imageKnob.getWidth();
        const int frameId = (int)ceil(rotation * ((double)frames - 1.0));
        const float radius = jmin(width / 2.0f, height / 2.0f);
        const float centerX = x + width * 0.5f;
        const float centerY = y + height * 0.5f;
        const float rx = centerX - radius - 1.0f;
        const float ry = centerY - radius;


        g.drawImage(imageKnob,(int)rx,(int)ry,2 * (int)radius,2 * (int)radius,0,frameId*imageKnob.getWidth(),imageKnob.getWidth(), imageKnob.getWidth());
    }

}
    juce::Image imageKnob;
    //==============================================================================

};

class AttachedLabel : public Label {
public:
    AttachedLabel(Justification justification = Justification::centredTop) :m_justification(justification) {
        setColour(juce::Label::textColourId, juce::Colour(28, 28, 28));
        setFont(juce::Font(12.0f));
        setMinimumHorizontalScale(1.0f);
        setBufferedToImage(false);
    }
    void setJustification(Justification justification) { m_justification = justification; }
    void componentMovedOrResized (Component& component, bool wasMoved, bool wasResized)
    {
        const Font f (getLookAndFeel().getLabelFont (*this));
        const int textWidth = f.getStringWidth(getText());

        setSize (textWidth+20, f.getHeight());

        setTopLeftPosition (component.getX() + (component.getWidth() - textWidth-20) / 2,
                        component.getY() - getHeight() + 2);
    }
private:
    Justification m_justification;
};
