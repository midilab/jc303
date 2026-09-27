#pragma once

/*
 * Harmonizer.h
 * Ported from midilab/aciduino v2 - harmonizer.h/.cpp
 * Original: (c) 2022 Romulo Silva - MIT License
 *
 * Maps raw MIDI note numbers through a scale/temperament lookup table.
 * Pure stateless transform — no timing, no platform dependencies.
 * Safe to call from the audio thread (reads only) or UI thread (writes
 * are protected by the owning sequencer's spinlock).
 */

#include <cstdint>
#include <cmath>

// ---------------------------------------------------------------------------
// Temperament table
// Each entry encodes a scale as an 8-bit bitmask where:
//   bit set   = whole-tone step (+2 semitones)
//   bit clear = half-tone step  (+1 semitone)
// Bits are read from MSB→LSB, 7 intervals describe the 7-note scale.
// ---------------------------------------------------------------------------
struct HarmonyMode
{
    const char* name;
    uint8_t     temperament;
};

static constexpr HarmonyMode kHarmonyModeTable[] =
{
    { "Ionian",      0b11010101 },  // Major
    { "Dorian",      0b10110101 },
    { "Phrygian",    0b10101101 },
    { "Lydian",      0b11010110 },
    { "Mixolydian",  0b11010100 },
    { "Aeolian",     0b10101101 },  // Natural Minor
    { "Locrian",     0b10100110 },
    { "Harm.Minor",  0b10101110 },
    { "Mel.Minor",   0b10110110 },
    { "Pentatonic",  0b11011000 },
    { "Blues",       0b10010100 },
    { "Chromatic",   0b00000000 },
};

static constexpr int kNumTemperaments = (int)(sizeof(kHarmonyModeTable) / sizeof(kHarmonyModeTable[0]));


// ---------------------------------------------------------------------------
class Harmonizer
{
public:
    Harmonizer()
    {
        setTemperament (1); // Dorian as default (classic acid mode)
    }

    // Build the scale lookup from a temperament ID.
    // NOT audio-thread-safe for concurrent writes — caller must gate.
    void setTemperament (uint8_t temperamentId)
    {
        if (temperamentId >= kNumTemperaments)
            temperamentId = 0;

        _temperamentId = temperamentId;

        const uint8_t temperament = kHarmonyModeTable[temperamentId].temperament;

        uint8_t scalePitch = 0;
        uint8_t interval   = 0;

        // 7 intervals → 7 scale degrees (8th bit is discarded, matching original)
        for (int intervalCount = 7; intervalCount > 0; --intervalCount)
        {
            _scale[interval] = scalePitch;

            if (! (temperament & (1 << intervalCount)))
                scalePitch += 1; // half-tone
            else
                scalePitch += 2; // whole-tone

            ++interval;
        }
    }

    // Map a raw MIDI note through the current scale.
    // Called from audio thread — reads _scale[] which is written only via
    // setTemperament(); caller is responsible for lock ordering.
    uint8_t harmonize (uint8_t note) const
    {
        const uint8_t octave   = static_cast<uint8_t>(note / 12);
        const uint8_t interval = static_cast<uint8_t>((note % 12) / 1.5f);
        return _scale[interval] + (octave * 12);
    }

    uint8_t     getTemperamentId()                         const { return _temperamentId; }
    const char* getTemperamentName (uint8_t id)            const
    {
        if (id >= kNumTemperaments) id = 0;
        return kHarmonyModeTable[id].name;
    }
    int         getNumTemperaments()                       const { return kNumTemperaments; }

private:
    uint8_t _temperamentId { 1 };
    uint8_t _scale[7]      {};
};
