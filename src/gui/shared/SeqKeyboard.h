#pragma once

#include <JuceHeader.h>

// Single-octave playable keyboard, shared across GUI themes (same pattern as
// MenuPage.h). Pure view/controller: it owns a MidiKeyboardState and reports
// key presses through pluggable callbacks so a theme can later route them into
// the JC303 engine / sequencer without any per-theme rework.
class SeqKeyboard  : public juce::Component,
                     public juce::MidiKeyboardStateListener
{
public:
    // startNote is the lowest MIDI note (chromatic); the keyboard spans the 12
    // semitones above it, i.e. exactly one octave.
    SeqKeyboard (int startNote = 36)
        : keyboard (keyboardState,
                    juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        keyboardState.addListener (this);

        keyboard.setAvailableRange (startNote, startNote + 11);
        keyboard.setKeyWidth (getLocalBounds().getWidth() > 0
                                  ? (float) getLocalBounds().getWidth() / 7.0f
                                  : 30.0f);
        keyboard.setScrollButtonsVisible (false);
        keyboard.setOctaveForMiddleC (5);
        addAndMakeVisible (keyboard);
    }

    ~SeqKeyboard() override
    {
        keyboardState.removeListener (this);
    }

    // Public API — themes wire these up to the sequencer / JC303 engine later.
    std::function<void (int midiNote, float velocity)> onNoteOn;
    std::function<void (int midiNote, float velocity)> onNoteOff;

    juce::MidiKeyboardState&     getKeyboardState()       { return keyboardState; }
    juce::MidiKeyboardComponent& getKeyboardComponent()   { return keyboard; }

private:
    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel, int midiNote, float velocity) override
    {
        juce::ignoreUnused (midiChannel);
        if (onNoteOn)
            onNoteOn (midiNote, velocity);
    }

    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNote, float velocity) override
    {
        juce::ignoreUnused (midiChannel);
        if (onNoteOff)
            onNoteOff (midiNote, velocity);
    }

    void resized() override
    {
        keyboard.setBounds (getLocalBounds());
        keyboard.setKeyWidth (getWidth() > 0 ? (float) getWidth() / 7.0f : 30.0f);
    }

    juce::MidiKeyboardState keyboardState;
    juce::MidiKeyboardComponent keyboard;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqKeyboard)
};
