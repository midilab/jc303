#pragma once

#include <JuceHeader.h>
#include "../../JC303.h"

/**
 * Compact [Reset] [Load] [scale name] row under the Tuning knob, plus knob
 * dim/disable while a custom .tun scale is active.
 * Reset and the scale name are visible only while custom tuning is on.
 * Long names truncate with ellipsis; full name is on the label tooltip.
 *
 * Single ChangeListener for all custom-scale UI (buttons, name, Tuning knob).
 */
class TuningFileControl : public juce::Component,
                          public juce::ChangeListener
{
public:
    TuningFileControl (JC303& processor, juce::Slider& tuningKnob)
        : processorRef (processor),
          tuningKnobRef (tuningKnob)
    {
        styleButton (resetButton, "RESET");
        styleButton (loadButton, "LOAD");

        scaleLabel.setJustificationType (juce::Justification::centredLeft);
        scaleLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.9f));
        scaleLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        scaleLabel.setFont (juce::Font (9.0f, juce::Font::bold));
        scaleLabel.setMinimumHorizontalScale (0.7f);
        // Allow hover so the full scale name tooltip works (label is non-interactive).
        scaleLabel.setInterceptsMouseClicks (true, false);

        resetButton.onClick = [this] { processorRef.resetTuningToDefault(); };
        loadButton.onClick  = [this] { browseAndLoad(); };

        // Load is always present; Reset + name only when a custom scale is active.
        addChildComponent (resetButton);
        addAndMakeVisible (loadButton);
        addChildComponent (scaleLabel);

        processorRef.addTuningChangeListener (this);
        refreshFromProcessor();
    }

    ~TuningFileControl() override
    {
        // Detach LookAndFeel before member destruction order tears it down
        loadButton.setLookAndFeel (nullptr);
        resetButton.setLookAndFeel (nullptr);
        processorRef.removeTuningChangeListener (this);
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        refreshFromProcessor();
    }

    void resized() override
    {
        layoutRow();
    }

private:
    void styleButton (juce::TextButton& b, const juce::String& text)
    {
        b.setButtonText (text);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2a2a));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff444444));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        b.setLookAndFeel (&buttonLookAndFeel);
    }

    /** Load centered; Reset left; name right. Called from resized() and after visibility changes. */
    void layoutRow()
    {
        auto area = getLocalBounds().reduced (1, 1);

        const int buttonW = 46;
        const int buttonH = juce::jmin (16, area.getHeight());
        const int gap = 4;
        const int rowY = area.getY() + (area.getHeight() - buttonH) / 2;

        const int loadX = area.getX() + (area.getWidth() - buttonW) / 2;
        loadButton.setBounds (loadX, rowY, buttonW, buttonH);

        if (resetButton.isVisible())
            resetButton.setBounds (loadX - gap - buttonW, rowY, buttonW, buttonH);

        if (scaleLabel.isVisible())
        {
            const int nameX = loadX + buttonW + gap;
            const int nameW = juce::jmax (0, area.getRight() - nameX);
            scaleLabel.setBounds (nameX, rowY, nameW, buttonH);
        }
    }

    void updateTuningKnobAppearance (bool custom)
    {
        tuningKnobRef.setEnabled (! custom);
        // Custom KnobLookAndFeel ignores default disabled greying — dim via alpha instead.
        tuningKnobRef.setAlpha (custom ? 0.4f : 1.0f);
        tuningKnobRef.setTooltip (custom
            ? "Tuning knob inactive while a custom scale is loaded — click Reset to re-enable"
            : juce::String());
    }

    void refreshFromProcessor()
    {
        const auto name = processorRef.getActiveTuningName();
        const bool custom = processorRef.isCustomTuningActive();

        if (custom)
        {
            loadButton.setTooltip ("Load another AnaMark .tun scale");
            scaleLabel.setText (name, juce::dontSendNotification);
            scaleLabel.setTooltip ("Scale: " + name);
        }
        else
        {
            loadButton.setTooltip ("Load AnaMark .tun scale");
            scaleLabel.setText ({}, juce::dontSendNotification);
            scaleLabel.setTooltip ({});
        }

        resetButton.setVisible (custom);
        scaleLabel.setVisible (custom);
        resetButton.setTooltip (custom ? ("Reset to equal temperament (was: " + name + ")")
                                       : juce::String());

        updateTuningKnobAppearance (custom);
        layoutRow();
    }

    void applyLoadedFile (const juce::File& file)
    {
        if (! file.existsAsFile())
            return;

        juce::String error;
        if (! processorRef.loadTuningFile (file, &error))
        {
            juce::String body = "Could not load \"" + file.getFileName() + "\".";
            if (error.isNotEmpty())
                body += "\n\n" + error;
            else
                body += "\n\nUnknown error.";

            juce::AlertWindow::showMessageBoxAsync (
                juce::AlertWindow::WarningIcon,
                "Tuning load failed",
                body);
        }
        // Success: ChangeListener refresh is enough
    }

    void browseAndLoad()
    {
        auto chooser = std::make_shared<juce::FileChooser> (
            "Load AnaMark TUN file",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
            "*.tun");

        constexpr auto flags = juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectFiles;

        // SafePointer: editor may be destroyed while the async dialog is open.
        chooser->launchAsync (flags,
            [safeThis = juce::Component::SafePointer<TuningFileControl> (this), chooser]
            (const juce::FileChooser& fc)
            {
                if (safeThis == nullptr)
                    return;

                safeThis->applyLoadedFile (fc.getResult());
            });
    }

    struct SmallButtonLookAndFeel : juce::LookAndFeel_V4
    {
        juce::Font getTextButtonFont (juce::TextButton&, int) override
        {
            return juce::Font (9.0f, juce::Font::bold);
        }
    };

    // LookAndFeel must outlive buttons that reference it (declare first).
    SmallButtonLookAndFeel buttonLookAndFeel;

    JC303& processorRef;
    juce::Slider& tuningKnobRef;
    juce::TextButton resetButton;
    juce::TextButton loadButton;
    juce::Label scaleLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuningFileControl)
};
