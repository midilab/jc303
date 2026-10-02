#pragma once

/*
 * ScaleMask.h
 * Pitch-class masks for snapping generated notes to a scale.
 * JUCE-free so it can be unit tested standalone (see tests/).
 *
 * A mask is 12 bits; bit n set = pitch class n is allowed. fix_tones[] in
 * acidRandomize is derived from it: each entry is the (<= 0) semitone offset
 * that moves a pitch class down onto the nearest allowed one.
 */

#include <cstdint>
#include <initializer_list>

namespace scalemask
{

constexpr uint16_t kAllNotes = 0x0FFF;

constexpr uint16_t bits (std::initializer_list<int> degrees)
{
    uint16_t m = 0;
    for (int d : degrees)
        m = static_cast<uint16_t> (m | (1u << d));
    return m;
}

// New scales are appended so saved seqScale indices keep their meaning.
// Index 0 = Chromatic (all notes, TONES grid applies). Masks are relative to the root (degree 0).
constexpr int kNumScaleChoices = 20;
constexpr int kNumRoots        = 12;

constexpr const char* kScaleNames[kNumScaleChoices] =
{
    "Chromatic", "Ionian", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Aeolian",
    "Locrian", "Harm.Minor", "Mel.Minor", "Maj Pent", "Min Pent", "Blues", "Whole Tone",
    "Dorian b2", "Lydian Aug", "Lydian Dom", "Mixolydian b6", "Locrian n2", "Altered",
};

constexpr const char* kRootNames[kNumRoots] =
{
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
};

// Step patterns: W = whole tone, h = half tone, 3 = minor third.
constexpr uint16_t kScaleMasks[kNumScaleChoices] =
{
    kAllNotes,
    bits ({ 0, 2, 4, 5, 7, 9, 11 }),    // Ionian: W W h W W W h
    bits ({ 0, 2, 3, 5, 7, 9, 10 }),    // Dorian: W h W W W h W
    bits ({ 0, 1, 3, 5, 7, 8, 10 }),    // Phrygian: h W W W h W W
    bits ({ 0, 2, 4, 6, 7, 9, 11 }),    // Lydian: W W W h W W h
    bits ({ 0, 2, 4, 5, 7, 9, 10 }),    // Mixolydian: W W h W W h W
    bits ({ 0, 2, 3, 5, 7, 8, 10 }),    // Aeolian: W h W W h W W
    bits ({ 0, 1, 3, 5, 6, 8, 10 }),    // Locrian: h W W h W W W
    bits ({ 0, 2, 3, 5, 7, 8, 11 }),    // Harmonic minor: W h W W h 3 h
    bits ({ 0, 2, 3, 5, 7, 9, 11 }),    // Melodic minor: W h W W W W h
    bits ({ 0, 2, 4, 7, 9 }),           // Major pentatonic: W W 3 W 3
    bits ({ 0, 3, 5, 7, 10 }),          // Minor pentatonic: 3 W W 3 W
    bits ({ 0, 3, 5, 6, 7, 10 }),       // Blues: 3 W h h 3 W
    bits ({ 0, 2, 4, 6, 8, 10 }),       // Whole tone: W W W W W W
    bits ({ 0, 1, 3, 5, 7, 9, 10 }),    // Dorian b2 (Phrygian natural 6): h W W W W h W
    bits ({ 0, 2, 4, 6, 8, 9, 11 }),    // Lydian augmented: W W W W h W h
    bits ({ 0, 2, 4, 6, 7, 9, 10 }),    // Lydian dominant: W W W h W h W
    bits ({ 0, 2, 4, 5, 7, 8, 10 }),    // Mixolydian b6: W W h W h W W
    bits ({ 0, 2, 3, 5, 6, 8, 10 }),    // Locrian natural 2: W h W h W W W
    bits ({ 0, 1, 3, 4, 6, 8, 10 }),    // Altered (Super Locrian): h W h W W W W
};

constexpr uint16_t rotate (uint16_t mask, int semitones)
{
    const int s = ((semitones % 12) + 12) % 12;
    return static_cast<uint16_t> (((mask << s) | (mask >> (12 - s))) & kAllNotes);
}

// Evenly spaced grid used by the TONES knob: pitch classes 0, 12/N, 2*12/N...
constexpr uint16_t toneGridMask (int numberOfTones)
{
    if (numberOfTones <= 0)
        return kAllNotes;
    const int step = 12 / numberOfTones;
    uint16_t m = 0;
    for (int pc = 0; pc < 12; pc += step)
        m = static_cast<uint16_t> (m | (1u << pc));
    return m;
}

// A selected scale (rotated to root) replaces the TONES grid; with Scale Chromatic the
// grid applies, which reproduces the original fix_tones[] behaviour.
constexpr uint16_t allowedMask (int root, int scaleId, int numberOfTones)
{
    if (scaleId <= 0 || scaleId >= kNumScaleChoices)
        return toneGridMask (numberOfTones);
    return rotate (kScaleMasks[scaleId], root);
}

// Notes the GUI keyboard allows: the whole keyboard for Chromatic (TONES is a
// generator-only setting), otherwise the selected scale rotated to the root.
constexpr uint16_t playableMask (int root, int scaleId)
{
    return allowedMask (root, scaleId, 12);
}

constexpr bool isPlayable (uint16_t mask, int midiNote)
{
    return ((mask >> (midiNote % 12)) & 1) != 0;
}

// out[pc] = offset (<= 0) down to the nearest allowed pitch class, wrapping
// through the octave. mask must be non-zero.
inline void buildFixTones (uint16_t mask, int8_t out[12])
{
    for (int pc = 0; pc < 12; ++pc)
    {
        int d = 0;
        while (! ((mask >> (((pc - d) % 12 + 12) % 12)) & 1))
            ++d;
        out[pc] = static_cast<int8_t> (-d);
    }
}

inline int snapNote (int note, const int8_t fixTones[12])
{
    const int snapped = note + fixTones[note % 12];
    return snapped < 0 ? 0 : (snapped > 127 ? 127 : snapped);
}

} // namespace scalemask
