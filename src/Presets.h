#pragma once

#include <JuceHeader.h>

#include <optional>
#include <utility>
#include <vector>

// A factory sound: parameter ID -> value: a choice name (or index), true / false for switches,
// the raw value for floats. Every sound parameter not listed goes back to its default.
// Presets (Init included) carry the volume too, so each sound recalls its own output level.
// modKnob1 / modKnob2 are the parameters on the two mod knobs; the mod wheel drives modKnob1.
// They must be MOD-page parameters, never a main-panel control such as Cutoff.
struct FactoryPreset
{
    juce::String name;
    std::vector<std::pair<juce::String, juce::var>> values;
    juce::String modKnob1 = "lfoRate";
    juce::String modKnob2 = "lfoDepth";
    juce::File file;   // the .json this preset was loaded from; empty for the fallback Init of an empty bank
};

// the mod knob assignments, kept as properties of the plugin state
inline const juce::Identifier modKnobIds[2] { "modKnob1", "modKnob2" };
inline const char* const defaultModKnobs[2] { "lfoRate", "lfoDepth" };

// Factory presets live as one JSON file per preset in the user's presets directory
// (see factoryPresetsDirectory()). On first use they are seeded from the copies embedded
// in the binary (FactoryPresetData), then always loaded from disk so users can edit them.
bool ensureFactoryPresets();                      // seed the directory if missing, (re)load the presets
const std::vector<FactoryPreset>& factoryPresets();  // loaded presets, Init first, then by file name
juce::File factoryPresetsDirectory();             // <Documents>/midilab/JC303/presets/factory

// Banks: the factory presets come from the "factory" subdirectory of the presets folder; any other
// subdirectory the user creates there (each holding its own *.json presets) is a selectable bank.
juce::StringArray presetBanks();                  // "factory" first, then the other directories, alphabetical
void loadPresetBank (const juce::String& name);   // clear presets from memory, then load the bank's *.json
juce::String currentPresetBank();                 // "factory" unless loadPresetBank was called

// User preset file operations. Every write happens atomically (a .json.tmp is moved over the target),
// and every operation reloads the current bank so in-memory presets mirror the folder's .json files.
juce::String presetFileName (const juce::String& name);   // "Dice Acid" -> "dice_acid.json" (lowercase, spaces -> _)
bool savePresetToFile (const FactoryPreset& preset, const juce::File& existingFile);
int createNewPresetFile (const FactoryPreset& preset, juce::String& usedName);   // OS-style "Name (1)" on collisions -> index
int renamePresetFile (const FactoryPreset& current, const juce::String& newName, juce::String& usedName);  // -> new index
int deletePresetFile (const FactoryPreset& current);   // -> the preset now occupying the same slot, or -1 when undeletable

// A preset value in the parameter's own units, or nothing if it does not fit the parameter
// (an unknown choice name, a name on a numeric parameter).
inline std::optional<float> resolvePresetValue (const juce::RangedAudioParameter& param, const juce::var& v)
{
    if (v.isString())
    {
        if (auto* choice = dynamic_cast<const juce::AudioParameterChoice*> (&param))
            if (const int index = choice->choices.indexOf (v.toString()); index >= 0)
                return (float) index;
        return std::nullopt;
    }
    if (v.isBool())
        return (bool) v ? 1.0f : 0.0f;
    if (v.isInt() || v.isInt64() || v.isDouble())
        return (float) (double) v;
    return std::nullopt;
}

// Presets are sounds: the sequencer, the generator and the output level and tuning stay as they are.
inline bool isPresetParameter (const juce::String& id)
{
    return ! id.startsWith ("seq") && id != "numberOfTones" && id != "lowerNote" && id != "rangeNote"
        && id != "tuning";
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
                if (const auto value = resolvePresetValue (*param, v))
                    normalised = param->convertTo0to1 (*value);

        param->beginChangeGesture();
        param->setValueNotifyingHost (normalised);
        param->endChangeGesture();
    }
}

// The inverse of applyFactoryPreset: snapshots every preset parameter's current value so the
// Save/New/Rename rows can write the audible state back to a .json file.
inline FactoryPreset captureFactoryPreset (const juce::Array<juce::AudioProcessorParameter*>& parameters)
{
    FactoryPreset preset;
    for (auto* p : parameters)
    {
        auto* param = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (param == nullptr || ! isPresetParameter (param->getParameterID()))
            continue;
        // choices by name and switches as true / false, like the factory files
        juce::var value = param->convertFrom0to1 (param->getValue());
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
            value = choice->getCurrentChoiceName();
        else if (auto* toggle = dynamic_cast<juce::AudioParameterBool*> (param))
            value = toggle->get();
        preset.values.emplace_back (param->getParameterID(), value);
    }
    return preset;
}
