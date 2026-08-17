#pragma once

#include <JuceHeader.h>

class AcidSmile : public juce::Component
{
public:
    AcidSmile()
    {
        image = juce::ImageCache::getFromMemory(BinaryData::acidsmile_png, BinaryData::acidsmile_pngSize);

        //setSize(image.getWidth()/4, image.getHeight()/4);
        setSize(56.25, 77.5);
    }

    ~AcidSmile() override = default;

    void paint(juce::Graphics& g) override
    {
        g.drawImage(image,
            0, 0, getWidth(), getHeight(),  
            0, 0, image.getWidth(), image.getHeight(),
            false);
    }

private:
    juce::Image image;
};