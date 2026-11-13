#include "Presets.h"

#include "FactoryPresetData.h"

#include <algorithm>

namespace
{
    std::vector<FactoryPreset> presets;
    juce::String currentBank = "factory";
    bool loaded = false;

    juce::File presetsDir()
    {
        // same Documents root the overdrive models use (see JC303.h), so users find both in one place
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                     .getChildFile (JucePlugin_Manufacturer).getChildFile (JucePlugin_Name)
                     .getChildFile ("presets").getChildFile ("factory");
    }

    juce::File banksRoot()
    {
        return presetsDir().getParentDirectory();
    }

    // The directory the currently selected bank's presets live in.
    juce::File currentBankDir()
    {
        return currentBank == "factory" ? presetsDir() : banksRoot().getChildFile (currentBank);
    }

    // First run: copy the JSON files embedded in the binary into the user's presets directory.
    void seedFromBinaryData (const juce::File& dir)
    {
        if (dir.createDirectory().failed())
            return;

        for (int i = 0; i < FactoryPresetData::namedResourceListSize; ++i)
        {
            const char* resource = FactoryPresetData::namedResourceList[i];
            int size = 0;
            const char* data = FactoryPresetData::getNamedResource (resource, size);
            const juce::String filename = juce::File (
                juce::String (FactoryPresetData::getNamedResourceOriginalFilename (resource))).getFileName();

            if (data == nullptr || size <= 0 || filename.isEmpty())
                continue;

            dir.getChildFile (filename).replaceWithText (juce::String::fromUTF8 (data, size));
        }
    }

    bool parsePreset (const juce::File& file, FactoryPreset& preset)
    {
        const auto parsed = juce::JSON::parse (file.loadFileAsString());
        const auto* obj = parsed.getDynamicObject();
        if (obj == nullptr)
            return false;

        preset.name = obj->hasProperty ("name") ? obj->getProperty ("name").toString()
                                                : file.getFileNameWithoutExtension();
        preset.modKnob1 = obj->hasProperty ("modKnob1") ? obj->getProperty ("modKnob1").toString() : "filterMorph";
        preset.modKnob2 = obj->hasProperty ("modKnob2") ? obj->getProperty ("modKnob2").toString() : "filterFm";

        preset.values.clear();
        if (const auto* values = obj->getProperty ("values").getDynamicObject())
            for (auto& [id, value] : values->getProperties())
                preset.values.emplace_back (id.toString(), value);

        preset.file = file;
        return true;
    }

    void loadFromDisk (const juce::File& dir)
    {
        presets.clear();
        for (auto& file : dir.findChildFiles (juce::File::findFiles, false, "*.json"))
        {
            FactoryPreset preset;
            if (parsePreset (file, preset))
                presets.push_back (std::move (preset));
        }

        // init.json stays first (it is the default program) — keyed on the file name, not the display
        // name, so a renamed preset ("Open303 Clean") and older seeded copies still sort first; the
        // rest in natural file-name order, so a bank is reordered by renaming its files
        // ("2_zulu.json" before "10_alpha.json").
        std::stable_sort (presets.begin(), presets.end(), [] (const FactoryPreset& a, const FactoryPreset& b)
        {
            const bool initA = a.file.getFileNameWithoutExtension() == "init";
            const bool initB = b.file.getFileNameWithoutExtension() == "init";
            if (initA)  return ! initB;
            if (initB)  return false;
            return a.file.getFileNameWithoutExtension().compareNatural (b.file.getFileNameWithoutExtension()) < 0;
        });
    }

    // Re-read one bank from disk so in-memory presets always mirror the folder's .json files.
    void reloadBank (const juce::File& dir, bool seedFactoryIfMissing)
    {
        loaded = true;
        presets.clear();   // never mix banks: every switch forgets the previous bank's presets

        if (seedFactoryIfMissing && dir == presetsDir())
            if (! dir.isDirectory())
                seedFromBinaryData (dir);

        loadFromDisk (dir);

        // An empty bank (or one whose presets all failed to parse) falls back to the default
        // program: the "Open303 Clean" sound, so a user who creates a blank folder can start
        // building a bank from there.
        if (presets.empty())
            presets.push_back ({ "Open303 Clean", {}, "filterMorph", "filterFm" });
    }

    // The .json file a preset should be written to: its own file, or the name-derived file in the
    // current bank for the fallback Init of a blank bank (which has no source file).
    juce::File targetFile (const FactoryPreset& preset)
    {
        if (preset.file.exists())
            return preset.file;
        return currentBankDir().getChildFile (presetFileName (preset.name));
    }

    bool writePresetJson (const FactoryPreset& preset, juce::File tempFile)
    {
        auto* values = new juce::DynamicObject();
        for (auto& [id, v] : preset.values)
            values->setProperty (id, v);

        auto* root = new juce::DynamicObject();
        root->setProperty ("name", preset.name);
        root->setProperty ("modKnob1", preset.modKnob1);
        root->setProperty ("modKnob2", preset.modKnob2);
        root->setProperty ("values", juce::var (values));

        // .json.tmp is swapped over the target when the write succeeded: a crash or a full disk
        // can never leave a half-written .json that would silently fail to parse next load.
        tempFile.replaceWithText (juce::JSON::toString (juce::var (root)));
        if (! tempFile.exists() || tempFile.getSize() == 0)
        {
            tempFile.deleteFile();
            return false;
        }
        return true;
    }

    bool nameTaken (const juce::String& name, const juce::File& dir)
    {
        for (auto& p : presets)
            if (p.name == name)
                return true;
        return dir.getChildFile (presetFileName (name)).exists();
    }

    // OS-style disambiguation: "Name", "Name (1)", "Name (2)", ... like saving a copy in a file manager.
    juce::String uniqueName (const juce::String& base, const juce::File& dir)
    {
        if (! nameTaken (base, dir))
            return base;
        for (int n = 1;; ++n)
        {
            const juce::String candidate = base + " (" + juce::String (n) + ")";
            if (! nameTaken (candidate, dir))
                return candidate;
        }
    }
}

bool ensureFactoryPresets()
{
    loadPresetBank ("factory");
    return ! presets.empty();
}

juce::StringArray presetBanks()
{
    juce::StringArray banks;
    banks.add ("factory");
    juce::StringArray others;
    for (auto& dir : banksRoot().findChildFiles (juce::File::findDirectories, false))
    {
        const juce::String name = dir.getFileName();
        if (name != "factory" && ! name.startsWith ("."))
            others.add (name);
    }
    others.sort (false);   // the drawn bank list, alphabetically after the default factory bank
    for (auto& b : others)
        banks.add (b);
    return banks;
}

void loadPresetBank (const juce::String& name)
{
    if (name == currentBank && loaded)
        return;
    currentBank = name;
    reloadBank (currentBankDir(), true);
}

juce::String currentPresetBank()
{
    return currentBank;
}

const std::vector<FactoryPreset>& factoryPresets()
{
    if (! loaded)
        ensureFactoryPresets();

    return presets;
}

juce::File factoryPresetsDirectory()
{
    return presetsDir();
}

juce::String presetFileName (const juce::String& name)
{
    juce::String file = name.toLowerCase();
    for (int i = 0; i < file.length(); ++i)
    {
        const juce::juce_wchar c = file[i];
        const bool ok = c == '_' || c == '-'
                     || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (! ok)
            file = file.replaceCharacter (c, '_');
    }
    while (file.startsWith ("_"))
        file = file.fromFirstOccurrenceOf ("_", false, false);
    while (file.endsWith ("_"))
        file = file.dropLastCharacters (1);

    // "My Cool Patch (1)" would collapse the space/parens into "my_cool_patch__1":
    // squeeze runs of underscores down to one, like a file-manager slug.
    while (file.contains ("__"))
        file = file.replace ("__", "_");

    if (file.isEmpty())
        file = "preset";
    return file + ".json";
}

bool savePresetToFile (const FactoryPreset& preset, const juce::File& existingFile)
{
    juce::File target = existingFile.exists() ? existingFile : targetFile (preset);
    if (target.getParentDirectory().createDirectory().failed())
        return false;
    if (! writePresetJson (preset, target.withFileExtension (".json.tmp")))
        return false;
    if (target.withFileExtension (".json.tmp").moveFileTo (target))
    {
        reloadBank (currentBankDir(), false);
        return true;
    }
    return false;
}

int createNewPresetFile (const FactoryPreset& preset, juce::String& usedName)
{
    const juce::File dir = currentBankDir();
    usedName = uniqueName (preset.name, dir);

    FactoryPreset copy = preset;
    copy.name = usedName;

    juce::File target = dir.getChildFile (presetFileName (usedName));
    if (! writePresetJson (copy, target.withFileExtension (".json.tmp")))
        return -1;
    if (target.withFileExtension (".json.tmp").moveFileTo (target))
    {
        reloadBank (dir, false);
        for (size_t i = 0; i < presets.size(); ++i)
            if (presets[i].name == usedName)
                return (int) i;
    }
    return -1;
}

int renamePresetFile (const FactoryPreset& current, const juce::String& newName, juce::String& usedName)
{
    if (current.file == juce::File() || ! current.file.exists())
        return -1;

    const juce::File dir = current.file.getParentDirectory();
    usedName = uniqueName (newName, dir);

    FactoryPreset copy = current;
    copy.name = usedName;

    juce::File target = dir.getChildFile (presetFileName (usedName));
    if (! writePresetJson (copy, target.withFileExtension (".json.tmp")))
        return -1;
    if (! target.withFileExtension (".json.tmp").moveFileTo (target))
        return -1;

    if (target != current.file)
        current.file.deleteFile();

    reloadBank (dir, false);
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].name == usedName)
            return (int) i;
    return -1;
}

int deletePresetFile (const FactoryPreset& current)
{
    if (current.file == juce::File() || ! current.file.exists())
        return -1;   // the fallback Init of an empty bank has no file to delete

    int oldIndex = 0;
    for (size_t i = 0; i < presets.size(); ++i)
        if (presets[i].file == current.file)
        {
            oldIndex = (int) i;
            break;
        }

    current.file.deleteFile();
    reloadBank (current.file.getParentDirectory(), false);
    return juce::jmin (oldIndex, (int) presets.size() - 1);   // the preset that moved into the freed slot
}