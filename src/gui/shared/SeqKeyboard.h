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
        this->startNote = juce::jlimit (0, 108, startNote);
        keyboardState.addListener (this);

        keyboard.setAvailableRange (startNote, startNote + 11);
        keyboard.setKeyWidth (getLocalBounds().getWidth() > 0
                                  ? (float) getLocalBounds().getWidth() / 7.0f
                                  : 30.0f);
        keyboard.setScrollButtonsVisible (false);
        keyboard.setOctaveForMiddleC (5);
        keyboard.onWheel = [this] (const juce::MouseWheelDetails& wheel)
        {
            // Some hosts/trackpads deliver sub-integer deltas per tick, so the
            // magnitude can't be trusted — use its sign, one octave per scroll.
            const float delta = wheel.deltaX != 0.0f
                                    ? wheel.deltaX
                                    : (wheel.isReversed ? -wheel.deltaY : wheel.deltaY);
            if (delta > -0.05f && delta < 0.05f)
                return;

            const int semitones = delta > 0.0f ? 12 : -12;
            setStartNote (this->startNote + semitones);
            if (onOctaveScroll)
                onOctaveScroll (semitones);
        };
        addAndMakeVisible (keyboard);
    }

    ~SeqKeyboard() override
    {
        keyboardState.removeListener (this);
    }

    // Public API — themes wire these up to the sequencer / JC303 engine later.
    std::function<void (int midiNote, float velocity)> onNoteOn;
    std::function<void (int midiNote, float velocity)> onNoteOff;

    // Fired when the mouse wheel transposes the keyboard: reports the size of the
    // jump in semitones (an octave, signed) so a theme can mirror it into the
    // note of the selected sequencer step. The keyboard octave has already moved
    // by the time this is called.
    std::function<void (int deltaSemitones)> onOctaveScroll;

    int  getStartNote() const { return startNote; }

    // Moves the whole keyboard up/down by whole octaves. startNote is always a C
    // (multiple of 12), so the visible layout never shifts — only the octave of
    // the displayed notes changes.
    void setStartNote (int note)
    {
        const int oldStart = startNote;
        startNote = juce::jlimit (0, 108, note);   // 108 + 11 = 119 < 128
        if (startNote == oldStart)
            return;
        keyboard.setAvailableRange (startNote, startNote + 11);
        if (shownNote >= 0)
        {
            const int relative = ((shownNote - oldStart) % 12 + 12) % 12;
            showNote (startNote + relative);
        }
        keyboard.repaint();
    }

    juce::MidiKeyboardState&     getKeyboardState()       { return keyboardState; }
    juce::MidiKeyboardComponent& getKeyboardComponent()   { return keyboard; }

    // Show (and hold) a single key, e.g. the note of the selected sequencer step.
    // Only one key is held at a time; programmatic highlights do not re-fire the
    // onNote callbacks, so they never write back into the sequencer.

    void showNote(int midiNote)
    {
        _suppressCallbacks = true;
        if (midiNote == shownNote)
        {
            // Re-assertsthe selected key after a physical release.
            keyboardState.noteOn (1, midiNote, 0.75f);
        }
        else
        {
            if (shownNote >= 0)
                keyboardState.noteOff(1, shownNote, 0.0f);
            keyboardState.noteOn (1, midiNote, 0.75f);
        }
        _suppressCallbacks = false;
        shownNote = midiNote;

        keyboard.repaint();
    }

private:
    // MidiKeyboardComponent's base class consumes wheel events to scroll the
    // visible keys horizontally; this subclass swallows that and hands the wheel
    // to the parent instead, so scrolling transposes the octave.
    class Keyboard  : public juce::MidiKeyboardComponent
    {
    public:
        using juce::MidiKeyboardComponent::MidiKeyboardComponent;

        std::function<void (const juce::MouseWheelDetails&)> onWheel;

    private:
        void mouseWheelMove (const juce::MouseEvent&,
                             const juce::MouseWheelDetails& wheel) override
        {
            if (onWheel)
                onWheel (wheel);
        }
    };

    void handleNoteOn (juce::MidiKeyboardState*, int midiChannel, int midiNote, float velocity) override
    {
        juce::ignoreUnused (midiChannel);
        if (! _suppressCallbacks && onNoteOn)
            onNoteOn (midiNote, velocity);
    }

    void handleNoteOff (juce::MidiKeyboardState*, int midiChannel, int midiNote, float velocity) override
    {
        juce::ignoreUnused (midiChannel);
        if (! _suppressCallbacks && onNoteOff)
            onNoteOff (midiNote, velocity);
    }

    void resized() override
    {
        keyboard.setBounds (getLocalBounds());
        keyboard.setKeyWidth (getWidth() > 0 ? (float) getWidth() / 7.0f : 30.0f);
    }

    juce::MidiKeyboardState keyboardState;
    Keyboard keyboard;

    int startNote { 36 };
    int shownNote = -1;
    bool _suppressCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SeqKeyboard)
};
