#pragma once

#include "Gui.h"

// Compact prev/next selector for the "filterType" AudioParameterChoice
// (TeeBee / Diode / Diode Octave). Mirrors OverdriveModelSelect, but sized
// to overlay inside the MODIFICATIONS label cell.
class FilterModelSelect : public juce::Component,
                          public juce::AudioProcessorValueTreeState::Listener
{
public:
    FilterModelSelect(juce::AudioProcessorValueTreeState& vts)
        : valueTreeState(vts)
    {
        customFont = juce::Font(juce::Typeface::createSystemTypefaceFor(BinaryData::ErbosDraco1StOpenNbpRegularl5wX_ttf, BinaryData::ErbosDraco1StOpenNbpRegularl5wX_ttfSize));
        customFont.setHeight(10.0f);

        imageLeftArrow = juce::ImageCache::getFromMemory(BinaryData::leftarrowpresets_png, BinaryData::leftarrowpresets_pngSize);
        imageRightArrow = juce::ImageCache::getFromMemory(BinaryData::rightarrowpresets_png, BinaryData::rightarrowpresets_pngSize);

        addAndMakeVisible(modelName);
        addAndMakeVisible(prevButton);
        addAndMakeVisible(nextButton);

        modelName.setFont(customFont);
        modelName.setJustificationType(juce::Justification::centred);
        modelName.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        modelName.setInterceptsMouseClicks(false, false);

        prevButton.onClick = [this] { changeIndex(-1); };
        nextButton.onClick = [this] { changeIndex(1); };

        // Initialise displayed name from the current parameter value
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(valueTreeState.getParameter("filterType")))
            setModelName(param->choices[param->getIndex()]);

        valueTreeState.addParameterListener("filterType", this);
    }

    ~FilterModelSelect() override
    {
        valueTreeState.removeParameterListener("filterType", this);
    }

    void changeIndex(int delta)
    {
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(valueTreeState.getParameter("filterType")))
        {
            int numChoices = param->choices.size();
            // Wrap around so the selector is circular: left from the first
            // choice lands on the last, right from the last lands on the first.
            int newValue = (param->getIndex() + delta + numChoices) % numChoices;
            param->beginChangeGesture();
            *param = newValue;
            param->endChangeGesture();
        }
    }

    void parameterChanged(const juce::String& parameterID, float newValue) override
    {
        if (parameterID == "filterType")
        {
            if (auto* param = dynamic_cast<juce::AudioParameterChoice*>(valueTreeState.getParameter("filterType")))
                setModelName(param->choices[static_cast<int>(newValue)]);
        }
    }

    void setModelName(const juce::String& name)
    {
        modelName.setText(name, juce::dontSendNotification);
    }

    void resized() override
    {
        const int arrowWidth = 7;
        prevButton.setBounds(0, 0, arrowWidth, getHeight());
        nextButton.setBounds(getWidth() - arrowWidth, 0, arrowWidth, getHeight());
        modelName.setBounds(arrowWidth + 2, 0, getWidth() - 2 * (arrowWidth + 2), getHeight());
    }

    void paint(juce::Graphics& g) override
    {
        paintImageButton(g, prevButton, imageLeftArrow, prevButton.isMouseOver());
        paintImageButton(g, nextButton, imageRightArrow, nextButton.isMouseOver());
    }

private:
    // Draw the arrow image, choosing the normal/hover half of the vertically-split png
    void paintImageButton(juce::Graphics& g, juce::ImageButton& button, const juce::Image& image, bool isHovered)
    {
        int frameHeight = image.getHeight() / 2;
        int sourceY = isHovered ? frameHeight : 0;
        g.drawImage(image, button.getBounds().getX(), button.getBounds().getY(), button.getWidth(), button.getHeight(),
                    0, sourceY, image.getWidth(), frameHeight, false);
    }

    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::ImageButton prevButton;
    juce::ImageButton nextButton;
    juce::Label modelName;

    juce::Font customFont;
    juce::Image imageLeftArrow;
    juce::Image imageRightArrow;
};
