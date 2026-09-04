#pragma once

#include <JuceHeader.h>

// Small LED used to visualise the sequencer step state; clicking it selects the
// step to edit. The image is a 3-frame vertical strip: OFF (top), ON (middle),
// and playing (bottom). setState() repaints only when the state actually changes.
class SequencerStepSelector : public juce::Component
{
public:
    enum class Mode
    {
        Toggle,
        Press
    };

    explicit SequencerStepSelector(Mode mode = Mode::Toggle, const juce::String& labelText = "")
        : buttonMode(mode)
    {
        imageLed = juce::ImageCache::getFromMemory(BinaryData::sequencer_step_selector_png, BinaryData::sequencer_step_selector_pngSize);
        setState(0);

        if (labelText.isNotEmpty())
        {
            label = std::make_unique<juce::Label>();
            addAndMakeVisible(label.get());
            label->setText(labelText, juce::dontSendNotification);
            label->setJustificationType(juce::Justification::centred);
            label->setFont(juce::Font(12.0f));
            label->setColour(juce::Label::textColourId, juce::Colours::black);
            label->setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
            label->setEditable(false);
            label->setInterceptsMouseClicks(false, false);
        }
    }

    void resized() override
    {
        if (label != nullptr)
            label->setBounds(0, getHeight() - 14, getWidth(), 14);
    }

    void setState(int state)
    {
        if (ledState == state) return;
        ledState = state;
        repaint();
    }

    int getState() const { return ledState; }

    // Toggle mode: clicking flips between state 0 (off) and 1 (on) before onClick.
    void setClickTogglesState(bool enabled) { clickToggles = enabled; }

    // Clicking an LED selects the corresponding sequencer step (wired by the editor).
    std::function<void()> onClick;

    // Press mode: fires onPress while the mouse is down (state 1 highlight).
    std::function<void()> onPress;

    void mouseDown(const juce::MouseEvent&) override
    {
        if (buttonMode == Mode::Press)
        {
            setState(1);
            if (onPress != nullptr)
                onPress();
            return;
        }

        if (clickToggles)
            setState(ledState == 0 ? 1 : 0);
        if (onClick != nullptr)
            onClick();
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        if (buttonMode == Mode::Press)
            setState(0);
    }

    void paint(juce::Graphics& g) override
    {
        if (imageLed.isValid())
        {
            const int frameHeight = imageLed.getHeight() / 3;
            const int sourceY = ledState * frameHeight;
            const int drawHeight = (label != nullptr) ? jmax(0, getHeight() - 16) : getHeight();

            g.drawImage(imageLed, 0,  0, getWidth(), drawHeight,
                        0, sourceY, imageLed.getWidth(), frameHeight,
                        false);
        }
    }

private:
    juce::Image imageLed;
    std::unique_ptr<juce::Label> label;
    int ledState = 0;
    bool clickToggles = false;
    Mode buttonMode = Mode::Toggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerStepSelector)
};