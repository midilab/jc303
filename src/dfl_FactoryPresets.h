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
