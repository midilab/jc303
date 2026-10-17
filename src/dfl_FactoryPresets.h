#pragma once

#include <JuceHeader.h>

#include <utility>
#include <vector>

namespace dfl
{

// A factory sound: parameter ID -> value in the parameter's own units (choice index, 0/1 for bools,
// the raw value for floats). Every sound parameter not listed goes back to its default, so Init is empty.
// knob1 / knob2 are the parameters on the two performance knobs; the mod wheel drives knob1.
struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
    const char* knob1 = "cutoff";
    const char* knob2 = "lfoDepth";
};

// the performance knob assignments, kept as properties of the plugin state
inline const juce::Identifier performanceKnobIds[2] { "perfKnob1", "perfKnob2" };
inline const char* const defaultPerformanceKnobs[2] { "cutoff", "lfoDepth" };

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets {
        { "Init", {} },

        // Alpha Juno "What The" / Mentasm: the PWM'd chopped saw (SAW03), thickened by the supersaw stack in place
        // of the Juno's chorus, a -1 octave sub, an open MS-20 filter, long decays and long slides for the swoops.
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
            { "decay",           1.0f },
            { "accent",          0.4f },
            { "normalDecay",     1.0f },    // 3 s
            { "accentDecay",     1.0f },
            { "slideTime",       0.8f },    // 290 ms
        }, "superDetune", "lfoDepth" },

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
            { "softAttack",      0.35f },   // 7.5 ms
            { "resTrim",         0.98f },   // a hot unit: more loop gain
            { "modSlot3Source",  3.0f },    // Accent
            { "modSlot3Dest",    4.0f },    // Resonance
            { "modSlot3Amount",  0.2f },    // +10 points on accented notes
        }, "cutoff", "accentSweep" },

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
        }, "cutoff", "modSlot3Amount" },

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
        }, "filterFm", "filterDrive" },

        // Stiff Spring: Hi-Fi diode ladder with stiff rungs: the resonance's pitch rises with its level, so loud
        // (accented) notes chirp upward. The mod wheel plays the tension.
        { "Stiff Spring", {
            { "switchModState",  1.0f },
            { "filterType",      1.0f },    // Spicy
            { "filterQuality",   1.0f },    // Hi-Fi
            { "filterTension",   0.7f },    // stiff: up to ~3 semitones higher when loud
            { "oscModel",        0.0f },
            { "waveform",        0.0f },    // saw
            { "cutoff",          0.35f },   // 640 Hz
            { "resonance",       0.88f },   // just below self-oscillation
            { "envmod",          0.65f },
            { "decay",           0.45f },   // 240 ms
            { "accent",          0.9f },
            { "filterDrive",     0.3f },    // +5 dB: enough level to bend the rungs
            { "normalDecay",     0.4f },    // 1.2 s
            { "accentDecay",     0.08f },   // 270 ms
            { "slideTime",       0.2f },    // 74 ms
            { "softAttack",      0.35f },   // 7.5 ms
        }, "filterTension", "resonance" },

        // Floppy Drop: Hi-Fi diode ladder with floppy rungs: the louder the resonance rings, the further its pitch
        // sags, a rubbery drop on every accent.
        { "Floppy Drop", {
            { "switchModState",  1.0f },
            { "filterType",      1.0f },    // Spicy
            { "filterQuality",   1.0f },    // Hi-Fi
            { "filterTension",   -0.8f },   // floppy: up to ~5 semitones lower when loud
            { "oscModel",        0.0f },
            { "waveform",        1.0f },    // square
            { "cutoff",          0.3f },    // 575 Hz
            { "resonance",       0.85f },
            { "envmod",          0.7f },
            { "decay",           0.55f },   // 380 ms
            { "accent",          1.0f },
            { "filterDrive",     0.35f },   // +6 dB
            { "normalDecay",     0.45f },   // 1.4 s
            { "accentDecay",     0.12f },   // 390 ms
            { "slideTime",       0.33f },   // 120 ms
        }, "filterTension", "cutoff" },

        // Rail Crusher: Hi-Fi diode ladder at full saturation (+16 dB) under a lowered rail: the stage voltages are
        // capped, so the drive turns into a dense, compressed crunch rather than runaway fizz.
        { "Rail Crusher", {
            { "switchModState",  1.0f },
            { "filterType",      1.0f },    // Spicy
            { "filterQuality",   1.0f },    // Hi-Fi
            { "filterDrive",     1.0f },    // +16 dB, full saturation
            { "filterRail",      0.4f },    // a low ceiling on the stage voltages
            { "filterBias",      1.0f },    // full asymmetry and driven swell
            { "filterTension",   0.0f },    // the circuit's rungs (knob 2 bends them)
            { "filterBias",      1.0f },    // full asymmetry and driven swell
            { "filterTension",   0.0f },    // the circuit's rungs
            { "oscModel",        0.0f },
            { "waveform",        0.0f },    // saw
            { "cutoff",          0.45f },   // 785 Hz
            { "resonance",       0.7f },
            { "envmod",          0.55f },
            { "decay",           0.5f },    // 300 ms
            { "accent",          0.8f },
            { "normalDecay",     0.4f },    // 1.2 s
            { "accentDecay",     0.08f },   // 270 ms
            { "slideTime",       0.16f },   // 60 ms
            { "softAttack",      0.35f },   // 7.5 ms
        }, "filterRail", "filterTension" },

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
        }, "superDetune", "superMix" },

        // Vowel Sync: Tri Mod formant sweep on every note, from the JP-8000 shape back down to a rooted sync bite.
        // Env+ rests at 0, so the Waveform knob is the resting shape.
        { "Vowel Sync", {
            { "switchModState",  1.0f },
            { "oscModel",        2.0f },    // Phaseshaper
            { "shaperMode",      0.0f },    // Tri Mod
            { "waveform",        0.2f },    // shape at rest: the root stays solid
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
            { "modSlot4Source",  7.0f },    // Env+
            { "modSlot4Dest",    10.0f },   // Waveform (shape)
            { "modSlot4Amount",  0.44f },   // 0.64 (JP-8000 default) at note-on, back to 0.2
        }, "waveform", "filterMorph" },

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
        }, "shapeDrift", "lfoDepth" },

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
        }, "filterMorph", "lfoDepth" },

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
        }, "filterDrive", "filterBias" },

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
        }, "modSlot3Amount", "modSlot4Amount" },

        // Sub Tracker: heavy -1 oct sub under a square, filter fully keytracked, decay shortens up the keyboard.
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
            { "subOscBlend",     1.0f },    // -1 octave (square)
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
        }, "subOscGain", "subOscBlend" },

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
        }, "cutoff", "modSlot3Amount" },

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
        }, "lfoRate", "filterMorph" },
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
