#pragma once

#include <JuceHeader.h>

// Small LED used to visualise the sequencer step state; clicking it selects the
// step to edit. The image is a 3-frame vertical strip: OFF (top), ON (middle),
// and playing (bottom). setState() repaints only when the state actually changes.
// Tri-state buttons (accent/mute, slide/hammer) add kSecondary: the ON frame tinted blue.
class SequencerStepSelector : public juce::Component
{
public:
    enum class Mode
    {
        Toggle,
        Press
    };

    static constexpr int kSecondary = 3;

    explicit SequencerStepSelector(Mode mode = Mode::Toggle, const juce::String& labelText = "")
        : buttonMode(mode)
    {
        imageLed = juce::ImageCache::getFromMemory(BinaryData::sequencer_step_selector_png, BinaryData::sequencer_step_selector_pngSize);
        buildSecondaryImage();
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

    // Click toggles off/on; shift-click toggles off/secondary (mute, hammer).
    void setSecondaryEnabled(bool enabled) { hasSecondary = enabled; }

    // Clicking an LED selects the corresponding sequencer step (wired by the editor).
    std::function<void()> onClick;

    // Right-clicking an LED (wired by the editor, e.g. to set pattern length).
    std::function<void()> onRightClick;

    // Press mode: fires onPress while the mouse is down (state 1 highlight).
    std::function<void()> onPress;

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (buttonMode == Mode::Press)
        {
            setState(1);
            if (onPress != nullptr)
                onPress();
            return;
        }

        if (event.mods.isRightButtonDown())
        {
            if (onRightClick != nullptr)
                onRightClick();
            return;
        }

        if (clickToggles)
        {
            if (hasSecondary && event.mods.isShiftDown())
                setState(ledState == kSecondary ? 0 : kSecondary);
            else
                setState(ledState == 0 ? 1 : 0);
        }
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
            const bool secondary = (ledState == kSecondary && imageSecondary.isValid());
            const int sourceY = (ledState == kSecondary ? 1 : ledState) * frameHeight;
            const int drawHeight = (label != nullptr) ? jmax(0, getHeight() - 16) : getHeight();

            if (secondary)
                g.drawImage(imageSecondary, 0, 0, getWidth(), drawHeight,
                            0, 0, imageSecondary.getWidth(), imageSecondary.getHeight(),
                            false);
            else
                g.drawImage(imageLed, 0,  0, getWidth(), drawHeight,
                            0, sourceY, imageLed.getWidth(), frameHeight,
                            false);
        }
    }

private:
    // Secondary frame: the OFF frame plus only the light the ON frame adds (the
    // lit glow), recoloured blue, so the button body and edge stay untouched.
    void buildSecondaryImage()
    {
        if (! imageLed.isValid())
            return;

        const int w = imageLed.getWidth();
        const int h = imageLed.getHeight() / 3;
        const auto off = imageLed.getClippedImage({ 0, 0, w, h });
        const auto on  = imageLed.getClippedImage({ 0, h, w, h });
        imageSecondary = juce::Image(juce::Image::ARGB, w, h, true);

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const auto c0 = off.getPixelAt(x, y);
                const auto c1 = on.getPixelAt(x, y);
                const float added = juce::jmax(0.0f, lum(c1) - lum(c0));
                const auto lit = juce::Colour::fromFloatRGBA(
                    juce::jlimit(0.0f, 1.0f, c0.getFloatRed()   + added * 0.25f),
                    juce::jlimit(0.0f, 1.0f, c0.getFloatGreen() + added * 0.55f),
                    juce::jlimit(0.0f, 1.0f, c0.getFloatBlue()  + added * 1.0f),
                    c1.getFloatAlpha());
                imageSecondary.setPixelAt(x, y, lit);
            }
        }
    }

    static float lum(juce::Colour c)
    {
        return 0.299f * c.getFloatRed() + 0.587f * c.getFloatGreen() + 0.114f * c.getFloatBlue();
    }

    juce::Image imageLed;
    juce::Image imageSecondary;
    std::unique_ptr<juce::Label> label;
    int ledState = 0;
    bool clickToggles = false;
    bool hasSecondary = false;
    Mode buttonMode = Mode::Toggle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SequencerStepSelector)
};