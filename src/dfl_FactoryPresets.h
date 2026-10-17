#pragma once

#include <JuceHeader.h>

#include <utility>
#include <vector>

namespace dfl
{

// A factory sound: parameter ID -> value in the parameter's own units (choice index, 0/1 for bools,
// the raw value for floats). Every sound parameter not listed goes back to its default, so Init is empty.
struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets {
        { "Init", {} },

        // Alpha Juno "What The" / Mentasm: the PWM'd chopped saw (SAW03), thickened by the supersaw stack in place
        // of the Juno's chorus, a -1 octave sub, an open MS-20 filter, a long amp decay, the Env+ pitch dive on
        // every note and long slides for the swoops.
        { "Hoover", {
            { "switchModState",  1.0f },
            { "oscModel",        1.0f },    // Alpha Juno
            { "junoWave",        2.0f },    // 3 PW
            { "superDetune",     0.35f },
            { "waveform",        0.0f },
            { "lfoDestination",  3.0f },    // Pulse Width
            { "lfoWaveform",     0.0f },    // Triangle
            { "lfoRate",         0.4f },    // 0.8 Hz
            { "lfoDepth",        0.7f },
            { "subOscGain",      0.35f },
            { "subOscBlend",     1.0f },    // -1 octave
            { "filterType",      2.0f },    // MissBehave
            { "cutoff",          0.85f },
            { "resonance",       0.15f },
            { "envmod",          0.15f },
            { "decay",           0.45f },   // 240 ms: the pitch dive (and filter env) time
            { "accent",          0.15f },   // light: a strong accent transient would swamp the body
            { "normalDecay",     1.0f },    // 3 s
            { "accentDecay",     0.07f },   // 240 ms: accented notes dive at the same speed
            { "slideTime",       0.8f },    // 290 ms
            { "modSlot3Source",  7.0f },    // Env+
            { "modSlot3Dest",    2.0f },    // Pitch
            { "modSlot3Amount",  0.25f },   // +3 semitones at note-on, diving onto the note; deeper on accents
        } },

        // Squelch: stock-timed saw acid on the Spicy diode ladder; accents also push the resonance up.
        { "Squelch", {
            { "switchModState",  1.0f },
            { "filterType",      1.0f },    // Spicy
            { "oscModel",        0.0f },    // TB-303
            { "waveform",        0.0f },    // saw
            { "cutoff",          0.3f },    // 575 Hz
            { "resonance",       0.8f },
            { "envmod",          0.75f },
            { "decay",           0.5f },    // 300 ms
            { "accent",          0.9f },
            { "normalDecay",     0.4f },    // 1.2 s, the stock amp decay
            { "accentDecay",     0.06f },   // 210 ms, stock
            { "slideTime",       0.16f },   // 60 ms, stock
            { "softAttack",      0.38f },   // 10 ms
            { "modSlot3Source",  3.0f },    // Accent
            { "modSlot3Dest",    4.0f },    // Resonance
            { "modSlot3Amount",  0.2f },    // +10 points on accented notes
        } },

        // Mind Bender (fringeshift): Spicy at the LP/BP blend, the Cutoff knob also morphing it from 0.1 to 0.3,
        // so opening the filter slides it further into bandpass.
        { "Mind Bender", {
            { "switchModState",  1.0f },
            { "filterType",      1.0f },    // Spicy
            { "oscModel",        0.0f },
            { "waveform",        0.0f },    // saw
            { "filterMorph",     0.1f },
            { "cutoff",          0.4f },
            { "resonance",       0.75f },
            { "envmod",          0.6f },
            { "decay",           0.5f },    // 300 ms
            { "accent",          0.8f },
            { "normalDecay",     0.4f },    // 1.2 s
            { "accentDecay",     0.06f },   // 210 ms
            { "slideTime",       0.16f },   // 60 ms
            { "softAttack",      0.38f },   // 10 ms
            { "modSlot3Source",  6.0f },    // Cutoff (knob position 0..1)
            { "modSlot3Dest",    13.0f },   // Filter Morph
            { "modSlot3Amount",  0.4f },    // +0.2 morph at full cutoff: 0.1 -> 0.3
        } },

        // Spicy Devil: Devil Fish-style square on the diode ladder, filter FM, accent sweep that builds over
        // consecutive accents, long accent decay.
        { "Spicy Devil", {
            { "switchModState",  1.0f },
            { "filterType",      1.0f },    // Spicy
            { "oscModel",        0.0f },
            { "waveform",        1.0f },    // square (Circuit model, the default)
            { "cutoff",          0.35f },   // 640 Hz
            { "resonance",       0.9f },
            { "envmod",          0.6f },
            { "decay",           0.4f },    // 190 ms
            { "accent",          1.0f },
            { "filterDrive",     0.5f },    // +8 dB into the ladder
            { "filterBias",      0.65f },
            { "filterFm",        0.4f },
            { "bassComp",        0.3f },
            { "resTrim",         0.6f },    // x1.03 loop gain
            { "feedbackFilter",  1.0f },    // 100 Hz: more bass under the resonance
            { "accentSweep",     0.6f },    // 280 ms: accents stack up
            { "accentDecay",     0.3f },    // 920 ms long accent decay
            { "normalDecay",     0.6f },    // 1.8 s
            { "slideTime",       0.5f },    // 180 ms
            { "modSlot3Source",  3.0f },    // Accent
            { "modSlot3Dest",    6.0f },    // Filter FM
            { "modSlot3Amount",  0.6f },    // +0.3 FM depth on accents
        } },

        // Super JP: JP-8000 supersaw stack on the Juno-106 filter, half keytracked, slow cutoff breath.
        { "Super JP", {
            { "switchModState",  1.0f },
            { "oscModel",        1.0f },    // Alpha Juno
            { "junoWave",        0.0f },    // 1 Saw
            { "superDetune",     0.55f },
            { "superMix",        0.75f },
            { "waveform",        0.0f },
            { "filterType",      3.0f },    // JuneBug
            { "cutoff",          0.85f },
            { "resonance",       0.1f },
            { "envmod",          0.2f },
            { "decay",           0.7f },    // 750 ms
            { "accent",          0.3f },
            { "normalDecay",     1.0f },    // 3 s
            { "softAttack",      0.55f },   // 47 ms
            { "slideTime",       0.3f },    // 110 ms
            { "subOscGain",      0.2f },
            { "subOscBlend",     1.0f },    // -1 octave
            { "lfoDestination",  0.0f },    // Cutoff
            { "lfoWaveform",     0.0f },    // Triangle
            { "lfoRate",         0.3f },    // 0.5 Hz
            { "lfoDepth",        0.15f },   // +/- 0.3 oct
            { "modSlot3Source",  4.0f },    // Key
            { "modSlot3Dest",    0.0f },    // Cutoff
            { "modSlot3Amount",  0.5f },    // 50% keyboard tracking
        } },

        // Vowel Sync: Tri Mod formant sweep on every note, from full JP-8000 bite down to the default shape.
        // The env slot is bipolar (rests at -1), so shape 0.82 - 0.18 settles at 0.64, clear of the 0.4 null.
        { "Vowel Sync", {
            { "switchModState",  1.0f },
            { "oscModel",        2.0f },    // Phaseshaper
            { "shaperMode",      0.0f },    // Tri Mod
            { "waveform",        0.82f },   // shape
            { "modSlot2Dest",    10.0f },   // Env -> Waveform (shape)
            { "modSlot2Amount",  0.18f },   // 1.0 at note-on -> 0.64 at rest
            { "filterType",      6.0f },    // SemiSweet
            { "filterMorph",     0.25f },   // LP leaning to BP: nasal
            { "cutoff",          0.7f },    // 1.3 kHz
            { "resonance",       0.3f },
            { "envmod",          0.3f },
            { "decay",           0.6f },    // 475 ms: also the formant sweep time
            { "accentDecay",     0.15f },   // 475 ms, same sweep on accents
            { "accent",          0.5f },
            { "normalDecay",     0.5f },    // 1.5 s
            { "slideTime",       0.4f },    // 145 ms
            { "modSlot3Source",  4.0f },    // Key
            { "modSlot3Dest",    0.0f },    // Cutoff
            { "modSlot3Amount",  0.5f },
        } },

        // Ripple Lead: drifting Ripple Saw with the LFO sweeping the ripple, -2 oct sub, ARP 4075 filter.
        { "Ripple Lead", {
            { "switchModState",  1.0f },
            { "oscModel",        2.0f },    // Phaseshaper
            { "shaperMode",      1.0f },    // Ripple Saw
            { "waveform",        0.5f },    // mid ripple
            { "shapeDrift",      0.6f },
            { "filterType",      7.0f },    // Oddity
            { "cutoff",          0.75f },
            { "resonance",       0.35f },
            { "envmod",          0.25f },
            { "decay",           0.65f },   // 600 ms
            { "accent",          0.4f },
            { "normalDecay",     0.9f },    // 2.7 s
            { "softAttack",      0.5f },    // 30 ms
            { "slideTime",       0.5f },
            { "subOscGain",      0.3f },
            { "subOscBlend",     0.0f },    // -2 octaves (25% pulse)
            { "lfoDestination",  10.0f },   // Waveform (ripple amount)
            { "lfoWaveform",     0.0f },    // Triangle
            { "lfoRate",         0.25f },   // 0.4 Hz
            { "lfoDepth",        0.25f },   // shape 0.25..0.75
            { "modSlot3Source",  4.0f },    // Key
            { "modSlot3Dest",    8.0f },    // Decay
            { "modSlot3Amount",  -0.4f },   // high notes decay faster
        } },

        // Wasp Sting: EDP Wasp in bandpass on a PWM'd circuit square; accents drop back to lowpass.
        { "Wasp Sting", {
            { "switchModState",  1.0f },
            { "oscModel",        0.0f },
            { "waveform",        1.0f },    // square
            { "filterType",      4.0f },    // Wasabi
            { "filterMorph",     0.5f },    // bandpass
            { "cutoff",          0.45f },   // 785 Hz
            { "resonance",       0.7f },
            { "envmod",          0.6f },
            { "decay",           0.45f },
            { "accent",          0.8f },
            { "lfoDestination",  3.0f },    // Pulse Width
            { "lfoWaveform",     0.0f },
            { "lfoRate",         0.55f },   // 1.9 Hz
            { "lfoDepth",        0.6f },
            { "normalDecay",     0.35f },   // 1.1 s
            { "accentDecay",     0.1f },    // 330 ms
            { "slideTime",       0.3f },
            { "modSlot3Source",  3.0f },    // Accent
            { "modSlot3Dest",    13.0f },   // Filter Morph
            { "modSlot3Amount",  -0.5f },   // accents morph BP -> LP for weight
        } },

        // Miss Scream: MS-20 at full resonance driven hard, keytracked, per-note random cutoff, RAT before the VCA.
        { "Miss Scream", {
            { "switchModState",  1.0f },
            { "filterType",      2.0f },    // MissBehave
            { "oscModel",        0.0f },
            { "waveform",        0.3f },    // mostly saw
            { "cutoff",          0.0f },    // 314 Hz: keytrack, random and the envelope open it
            { "resonance",       1.0f },
            { "envmod",          0.5f },
            { "decay",           0.5f },
            { "accent",          0.9f },
            { "filterDrive",     0.44f },   // +7 dB
            { "filterBias",      0.1f },    // nearly symmetric
            { "filterRail",      1.0f },
            { "normalDecay",     0.5f },
            { "accentDecay",     0.1f },
            { "slideTime",       0.33f },
            { "modSlot3Source",  4.0f },    // Key
            { "modSlot3Dest",    0.0f },    // Cutoff
            { "modSlot3Amount",  0.6f },
            { "modSlot4Source",  5.0f },    // Random
            { "modSlot4Dest",    0.0f },    // Cutoff
            { "modSlot4Amount",  0.1f },    // +/- 0.2 oct per note
            { "switchOverdriveState", 1.0f },
            { "overdriveModelIndex",  4.0f },   // Pro Co RAT
            { "overdriveLevel",  0.5f },
            { "overdriveDryWet", 0.5f },
            { "overdriveDisperse", 0.3f },
            { "overdrivePreVca", 1.0f },    // pre-VCA: the RAT post-VCA runs ~14 dB hotter than the bank
        } },

        // Dice Acid: every note rolls its own cutoff and decay; an S&H LFO flickers the saw/square blend.
        { "Dice Acid", {
            { "switchModState",  1.0f },
            { "filterType",      1.0f },    // Spicy
            { "oscModel",        0.0f },
            { "waveform",        0.3f },    // LFO swings this 0..0.6
            { "cutoff",          0.4f },
            { "resonance",       0.75f },
            { "envmod",          0.65f },
            { "decay",           0.5f },
            { "accent",          0.85f },
            { "normalDecay",     0.4f },
            { "accentDecay",     0.06f },
            { "slideTime",       0.2f },    // 74 ms
            { "lfoWaveform",     4.0f },    // Random (S&H)
            { "lfoRate",         0.6f },    // 2.4 Hz
            { "lfoDestination",  10.0f },   // Waveform
            { "lfoDepth",        0.3f },
            { "modSlot3Source",  5.0f },    // Random
            { "modSlot3Dest",    0.0f },    // Cutoff
            { "modSlot3Amount",  0.25f },   // +/- 0.5 oct per note
            { "modSlot4Source",  5.0f },    // Random
            { "modSlot4Dest",    8.0f },    // Decay
            { "modSlot4Amount",  0.5f },    // +/- 1 octave of decay time per note
        } },

        // Sub Tracker: heavy -2 oct sub under a square, filter fully keytracked, decay shortens up the keyboard.
        { "Sub Tracker", {
            { "switchModState",  1.0f },
            { "oscModel",        0.0f },
            { "waveform",        1.0f },    // square
            { "filterType",      3.0f },    // JuneBug
            { "cutoff",          0.3f },
            { "resonance",       0.3f },
            { "envmod",          0.4f },
            { "decay",           0.55f },   // 380 ms
            { "accent",          0.5f },
            { "subOscGain",      0.8f },
            { "subOscBlend",     0.0f },    // -2 octaves
            { "normalDecay",     0.5f },
            { "accentDecay",     0.15f },
            { "softAttack",      0.3f },    // 5 ms
            { "slideTime",       0.4f },
            { "modSlot3Source",  4.0f },    // Key
            { "modSlot3Dest",    0.0f },    // Cutoff
            { "modSlot3Amount",  1.0f },    // 100% tracking (+/- 2 oct over +/- 2 oct)
            { "modSlot4Source",  4.0f },    // Key
            { "modSlot4Dest",    8.0f },    // Decay
            { "modSlot4Amount",  -0.5f },   // an octave up halves the decay
        } },

        // Muff Stomp: Big Muff fuzz before the VCA so the envelope shapes the fuzz; accents push the wet mix.
        { "Muff Stomp", {
            { "switchModState",  1.0f },
            { "filterType",      1.0f },    // Spicy
            { "oscModel",        0.0f },
            { "waveform",        0.0f },    // saw
            { "cutoff",          0.35f },
            { "resonance",       0.7f },
            { "envmod",          0.6f },
            { "decay",           0.5f },
            { "accent",          0.9f },
            { "bassComp",        0.4f },
            { "feedbackFilter",  0.8f },    // 128 Hz
            { "normalDecay",     0.45f },
            { "accentDecay",     0.08f },
            { "slideTime",       0.33f },
            { "switchOverdriveState", 1.0f },
            { "overdriveModelIndex",  36.0f },  // Big Muff V6
            { "overdriveLevel",  0.7f },
            { "overdriveDryWet", 0.6f },
            { "overdriveDisperse", 0.5f },
            { "overdrivePreVca", 1.0f },
            { "modSlot3Source",  3.0f },    // Accent
            { "modSlot3Dest",    5.0f },    // Overdrive (dry/wet)
            { "modSlot3Amount",  0.8f },    // +0.4 wet on accents (clamped at 1.0)
        } },

        // Hornet Morph: buzzy +3 oct chopped Juno saw, the Wasp A124 filter swept LP -> BP -> HP by the LFO.
        { "Hornet Morph", {
            { "switchModState",  1.0f },
            { "oscModel",        1.0f },    // Alpha Juno
            { "junoWave",        3.0f },    // 4 Hi Oct (SAW04)
            { "waveform",        0.0f },
            { "filterType",      5.0f },    // Hornet
            { "filterMorph",     0.5f },    // centre on BP so +/- 0.5 reaches LP and HP
            { "cutoff",          0.5f },    // 870 Hz
            { "resonance",       0.6f },
            { "envmod",          0.3f },
            { "decay",           0.5f },
            { "accent",          0.6f },
            { "lfoDestination",  13.0f },   // Filter Morph
            { "lfoWaveform",     0.0f },    // Triangle
            { "lfoRate",         0.35f },   // 0.6 Hz
            { "lfoDepth",        1.0f },    // full LP..HP sweep
            { "normalDecay",     0.5f },
            { "slideTime",       0.33f },
        } },
    };
    return presets;
}

// Presets are sounds: the sequencer, the generator and the output level and tuning stay as they are.
inline bool isPresetParameter (const juce::String& id)
{
    return ! id.startsWith ("seq") && id != "numberOfTones" && id != "lowerNote" && id != "rangeNote"
        && id != "volume" && id != "tuning";
}

inline void applyFactoryPreset (const juce::Array<juce::AudioProcessorParameter*>& parameters, const FactoryPreset& preset)
{
    for (auto* p : parameters)
    {
        auto* param = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (param == nullptr || ! isPresetParameter (param->getParameterID()))
            continue;

        float normalised = param->getDefaultValue();
        for (auto& [id, v] : preset.values)
            if (param->getParameterID() == id)
                normalised = param->convertTo0to1 (v);

        param->beginChangeGesture();
        param->setValueNotifyingHost (normalised);
        param->endChangeGesture();
    }
}

} // namespace dfl
