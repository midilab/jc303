// Factory preset tests against the real JC303 processor and its parameter layout.

#include "JC303.h"
#include "Presets.h"
#include "gui/shared/MenuPage.h"

#include <algorithm>
#include <cstdio>

static int failures = 0;

#define CHECK(cond, ...)                                      \
    do {                                                      \
        if (! (cond)) {                                       \
            ++failures;                                       \
            std::printf ("FAIL %s:%d: ", __FILE__, __LINE__); \
            std::printf (__VA_ARGS__);                        \
            std::printf ("\n");                               \
        }                                                     \
    } while (0)

static juce::RangedAudioParameter* find (JC303& p, const juce::String& id)
{
    for (auto* param : p.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param); ranged != nullptr && ranged->getParameterID() == id)
            return ranged;
    return nullptr;
}

static float valueOf (JC303& p, const char* id)
{
    auto* param = find (p, id);
    return param->convertFrom0to1 (param->getValue());
}

static void setParam (JC303& p, const char* id, float v)
{
    auto* param = find (p, id);
    param->setValueNotifyingHost (param->convertTo0to1 (v));
}

static float defaultOf (JC303& p, const char* id)
{
    auto* param = find (p, id);
    return param->convertFrom0to1 (param->getDefaultValue());
}

static int programIndex (JC303& p, const juce::String& name)
{
    for (int i = 0; i < p.getNumPrograms(); ++i)
        if (p.getProgramName (i) == name)
            return i;
    return -1;
}

static bool hasParamValue (JC303& p, int index, const char* id, float want)
{
    const auto& preset = factoryPresets()[(size_t) index];
    for (auto& [pid, v] : preset.values)
        if (pid == id && std::abs (v - want) < 1e-3f)
            return true;
    return false;
}

static void testProgramList (JC303& p)
{
    CHECK (p.getNumPrograms() >= 2, "only %d programs", p.getNumPrograms());
    CHECK (p.getProgramName (0) == "Init", "first program is '%s'", p.getProgramName (0).toRawUTF8());
    CHECK (programIndex (p, "Hoover") > 0, "no Hoover program");
}

static void testPresetIdsExist (JC303& p)
{
    for (auto& preset : factoryPresets())
        for (auto& [id, v] : preset.values)
            CHECK (find (p, id) != nullptr, "preset '%s' sets unknown parameter '%s'", preset.name.toRawUTF8(), id.toRawUTF8());
}

static void testPresetValuesInRange (JC303& p)
{
    // convertTo0to1 clamps silently, so an out-of-range value or choice index would load as something else
    for (auto& preset : factoryPresets())
        for (auto& [id, v] : preset.values)
            if (auto* param = find (p, id))
            {
                const float loaded = param->convertFrom0to1 (param->convertTo0to1 (v));
                CHECK (std::abs (loaded - v) < 1e-3f, "preset '%s': %s = %g loads as %g", preset.name.toRawUTF8(), id.toRawUTF8(), v, loaded);
            }
}

static void testEveryPresetHasVolume (JC303& p)
{
    for (auto& preset : factoryPresets())
    {
        bool hasVolume = false;
        for (auto& [id, v] : preset.values)
            hasVolume = hasVolume || (id == "volume" && std::abs (v - 0.75f) < 1e-4f);
        CHECK (hasVolume, "preset '%s' does not recall volume 0.75", preset.name.toRawUTF8());
    }
}

static void testInitResetsSoundOnly (JC303& p)
{
    setParam (p, "cutoff", 0.9f);
    setParam (p, "oscModel", 2.0f);
    setParam (p, "volume", 0.4f);
    setParam (p, "tuning", 0.3f);
    setParam (p, "seqTempo", 140.0f);
    setParam (p, "seqSwing", 60.0f);

    p.setCurrentProgram (0);

    CHECK (valueOf (p, "cutoff") == defaultOf (p, "cutoff"), "Init left cutoff at %g", valueOf (p, "cutoff"));
    CHECK (valueOf (p, "oscModel") == 0.0f, "Init left oscModel at %g", valueOf (p, "oscModel"));
    // Init recalls its own volume (0.75) like any preset
    CHECK (std::abs (valueOf (p, "volume") - defaultOf (p, "volume")) < 1e-4f, "Init left volume at %g", valueOf (p, "volume"));
    CHECK (std::abs (valueOf (p, "tuning") - 0.3f) < 1e-4f, "Init changed the tuning to %g", valueOf (p, "tuning"));
    CHECK (valueOf (p, "seqTempo") == 140.0f, "Init changed the sequencer tempo to %g", valueOf (p, "seqTempo"));
    CHECK (std::abs (valueOf (p, "seqSwing") - 60.0f) < 1e-3f, "Init changed the swing to %g", valueOf (p, "seqSwing"));
    CHECK (p.getCurrentProgram() == 0, "current program %d", p.getCurrentProgram());
}

static void testHoover (JC303& p)
{
    const int hoover = programIndex (p, "Hoover");
    setParam (p, "lfoWaveform", 3.0f);   // not part of the preset: must come back to its default
    p.setCurrentProgram (hoover);

    CHECK (valueOf (p, "switchModState") > 0.5f, "Mods off");
    CHECK (valueOf (p, "oscModel") == 1.0f, "oscModel %g, want Alpha Juno", valueOf (p, "oscModel"));
    CHECK (valueOf (p, "junoWave") == 2.0f, "junoWave %g, want 3 PW", valueOf (p, "junoWave"));
    CHECK (valueOf (p, "superDetune") > 0.0f, "no supersaw detune");
    CHECK (valueOf (p, "lfoDestination") == 3.0f, "lfoDestination %g, want Pulse Width", valueOf (p, "lfoDestination"));
    CHECK (valueOf (p, "lfoDepth") > 0.0f, "no PWM depth");
    CHECK (valueOf (p, "lfoWaveform") == defaultOf (p, "lfoWaveform"), "lfoWaveform %g not reset", valueOf (p, "lfoWaveform"));
    CHECK (p.getCurrentProgram() == hoover, "current program %d", p.getCurrentProgram());
}

// Two slid notes through processBlock; returns the rendered stereo buffer.
static juce::AudioBuffer<float> render (JC303& p, const juce::String& program)
{
    const double sr = 44100.0;
    const int block = 512, blocks = (int) (3.0 * sr / block);
    p.setCurrentProgram (programIndex (p, program));
    setParam (p, "tuning", 0.0f);   // presets keep the tuning; an earlier test detuned it
    p.setPlayConfigDetails (0, 2, sr, block);
    p.prepareToPlay (sr, block);

    juce::AudioBuffer<float> out (2, block * blocks);
    juce::AudioBuffer<float> buffer (2, block);
    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0)                         midi.addEvent (juce::MidiMessage::noteOn (1, 33, (juce::uint8) 100), 0);
        if (b == blocks / 3)                midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);   // overlapping: slide
        if (b == blocks / 3 + 1)            midi.addEvent (juce::MidiMessage::noteOff (1, 33), 0);
        if (b == 2 * blocks / 3)            midi.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
        buffer.clear();
        p.processBlock (buffer, midi);
        for (int c = 0; c < 2; ++c)
            out.copyFrom (c, b * block, buffer, c, 0, block);
    }
    p.releaseResources();
    return out;
}

static void writeWav (const juce::AudioBuffer<float>& out, juce::File file)
{
    file.deleteFile();
    juce::WavAudioFormat wav;
    if (auto stream = file.createOutputStream())
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), 44100.0, 2, 24, {}, 0)))
        {
            stream.release();
            writer->writeFromAudioSampleBuffer (out, 0, out.getNumSamples());
        }
}

static double rmsDb (const juce::AudioBuffer<float>& out, int from, int to)
{
    double sum = 0.0;
    for (int i = from; i < to; ++i)
        sum += out.getSample (0, i) * out.getSample (0, i);
    return 10.0 * std::log10 (sum / (to - from) + 1e-30);
}

// Hoover holds its level while the note is held (Init, like a stock 303, dies away)
static void testHooverRenders (JC303& p, const char* wavPath)
{
    const double sr = 44100.0;
    const auto init = render (p, "Init");
    const auto out = render (p, "Hoover");
    const int attack = (int) (0.15 * sr), held = (int) (1.9 * sr);
    const double hooverDb = rmsDb (out, 0, attack), initDb = rmsDb (init, 0, attack);
    const double hooverLateDb = rmsDb (out, held - attack, held), initLateDb = rmsDb (init, held - attack, held);
    std::printf ("attack: Hoover %.1f dBFS, Init %.1f dBFS; end of held note: Hoover %.1f, Init %.1f\n",
                 hooverDb, initDb, hooverLateDb, initLateDb);

    bool finite = true;
    for (int i = 0; i < out.getNumSamples(); ++i)
        finite = finite && std::isfinite (out.getSample (0, i));
    CHECK (finite, "hoover render has NaN/inf");
    CHECK (hooverLateDb > hooverDb - 9.0, "Hoover dies away while held (%.1f -> %.1f dB)", hooverDb, hooverLateDb);

    if (wavPath != nullptr)
        writeWav (out, juce::File::getCurrentWorkingDirectory().getChildFile (wavPath));
}

static void testModKnobs()
{
    {
        JC303 p;
        CHECK (p.getModKnob (0) == "lfoRate" && p.getModKnob (1) == "lfoDepth",
               "default knobs %s / %s", p.getModKnob (0).toRawUTF8(), p.getModKnob (1).toRawUTF8());

        p.setCurrentProgram (programIndex (p, "Super JP"));
        CHECK (p.getModKnob (0) == "superDetune" && p.getModKnob (1) == "superMix",
               "Super JP knobs %s / %s", p.getModKnob (0).toRawUTF8(), p.getModKnob (1).toRawUTF8());
        p.setCurrentProgram (0);
        CHECK (p.getModKnob (0) == "lfoRate", "Init knob 1 %s", p.getModKnob (0).toRawUTF8());

        // an assignment made on the knobs is part of the saved state
        p.setModKnob (0, "filterMorph");
        juce::MemoryBlock state;
        p.getStateInformation (state);
        JC303 restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (restored.getModKnob (0) == "filterMorph", "restored knob 1 %s", restored.getModKnob (0).toRawUTF8());
        CHECK (restored.getModKnob (1) == "lfoDepth", "restored knob 2 %s", restored.getModKnob (1).toRawUTF8());
    }
}

static void testPresetKnobsExist (JC303& p)
{
    for (auto& preset : factoryPresets())
        for (const auto& id : { preset.modKnob1, preset.modKnob2 })
            CHECK (find (p, id) != nullptr, "preset '%s' assigns unknown parameter '%s' to a knob",
                   preset.name.toRawUTF8(), id.toRawUTF8());
}

// The Presets page is a list of factory presets (like the Overdrive drivers) followed by the
// Save/New/Rename/Delete rows, and every preset's mod knobs must be MOD-page parameters.
static void testPresetPage()
{
    const auto pages = MenuPage::buildPages (juce::StringArray {});
    CHECK (pages.size() == 4, "%d pages", pages.size());
    CHECK (pages[0].title == "Presets", "first page is '%s'", pages[0].title.toRawUTF8());
    CHECK (pages[0].items.size() == 6, "preset page has %d rows", pages[0].items.size());

    const auto& presetRow = pages[0].items.getReference (0);
    CHECK (presetRow.options.size() == (int) factoryPresets().size(),
           "preset options %d, want %d", presetRow.options.size(), (int) factoryPresets().size());
    for (int i = 0; i < presetRow.options.size(); ++i)
        CHECK (presetRow.options[i] == factoryPresets()[(size_t) i].name,
               "preset option %d is '%s'", i, presetRow.options[i].toRawUTF8());
    CHECK (pages[0].items.getReference (1).label == "Bank", "preset page row 1 is '%s'",
           pages[0].items.getReference (1).label.toRawUTF8());
    const juce::StringArray saveRows { "Save", "New", "Rename", "Delete" };
    for (int i = 0; i < 4; ++i)
        CHECK (pages[0].items.getReference (2 + i).label == saveRows[i],
               "preset page row %d is '%s'", 2 + i, pages[0].items.getReference (2 + i).label.toRawUTF8());

    // the two mod knobs may only drive MOD-page parameters
    juce::StringArray modIds;
    bool found = false;
    for (auto& page : pages)
        for (auto& item : page.items)
            if (item.id == "lfoRate")   // only the MOD page lists LFO rate
            {
                for (auto& it : page.items)
                    modIds.add (it.id);
                found = true;
                break;
            }
    CHECK (found, "MOD page not found");
    for (auto& preset : factoryPresets())
        for (const auto& id : { preset.modKnob1, preset.modKnob2 })
            CHECK (modIds.contains (id), "preset '%s' assigns non-mod parameter '%s' to a knob",
                   preset.name.toRawUTF8(), id.toRawUTF8());
}

static void testBankSwitch()
{
    const int factoryCount = (int) factoryPresets().size();
    const juce::String factoryBank = currentPresetBank();

    // a user-created bank: a sibling folder of "factory" holding its own *.json presets
    const juce::File bank = factoryPresetsDirectory().getParentDirectory().getChildFile ("testbank");
    bank.deleteRecursively();
    bank.createDirectory();
    bank.getChildFile ("one.json").replaceWithText (
        "{\"name\":\"Bank One\",\"modKnob1\":\"lfoRate\",\"modKnob2\":\"lfoDepth\",\"values\":{}}");
    bank.getChildFile ("two.json").replaceWithText (
        "{\"name\":\"Bank Two\",\"modKnob1\":\"lfoRate\",\"modKnob2\":\"lfoDepth\",\"values\":{}}");

    CHECK (presetBanks().contains ("testbank"), "presetBanks does not list 'testbank'");
    CHECK (presetBanks()[0] == "factory", "the default bank is not first");

    loadPresetBank ("testbank");
    CHECK (currentPresetBank() == "testbank", "current bank is '%s'", currentPresetBank().toRawUTF8());
    CHECK ((int) factoryPresets().size() == 2, "testbank loaded %d presets", (int) factoryPresets().size());
    CHECK (factoryPresets().front().name == "Bank One", "first testbank preset is '%s'", factoryPresets().front().name.toRawUTF8());

    // switching banks forgets the previous one entirely
    loadPresetBank ("testbank");
    CHECK ((int) factoryPresets().size() == 2, "reload same bank %d", (int) factoryPresets().size());

    loadPresetBank ("factory");
    CHECK (currentPresetBank() == "factory", "back to '%s'", currentPresetBank().toRawUTF8());
    CHECK ((int) factoryPresets().size() == factoryCount, "factory restored %d, want %d",
           (int) factoryPresets().size(), factoryCount);

    // a missing bank falls back to Init so the device is never left without a program
    loadPresetBank ("does_not_exist");
    CHECK ((int) factoryPresets().size() == 1, "missing bank has %d presets, want 1", (int) factoryPresets().size());
    CHECK (factoryPresets()[0].name == "Init", "missing bank preset is '%s'", factoryPresets()[0].name.toRawUTF8());

    // an empty folder the user created is a valid starting point: it loads Init (defaults, mods off)
    const juce::File blank = factoryPresetsDirectory().getParentDirectory().getChildFile ("blankbank");
    blank.deleteRecursively();
    blank.createDirectory();
    loadPresetBank ("blankbank");
    CHECK (currentPresetBank() == "blankbank", "blank bank not selected");
    CHECK ((int) factoryPresets().size() == 1, "blank bank has %d presets, want 1", (int) factoryPresets().size());
    CHECK (factoryPresets()[0].name == "Init", "blank bank preset is '%s'", factoryPresets()[0].name.toRawUTF8());
    CHECK (factoryPresets()[0].modKnob1 == "lfoRate" && factoryPresets()[0].modKnob2 == "lfoDepth",
           "blank bank knobs '%s'/'%s'", factoryPresets()[0].modKnob1.toRawUTF8(), factoryPresets()[0].modKnob2.toRawUTF8());

    loadPresetBank (factoryBank);
    bank.deleteRecursively();
    blank.deleteRecursively();
}

// The Presets page file operations: Save overwrites, New/Rename disambiguate OS-style and always
// resync the bank in memory from the folder's .json files, Delete removes the file and selects the
// preset that moved into the freed slot.
static void testSaveRenameDelete (JC303& p)
{
    const juce::String factoryBank = currentPresetBank();
    const int factoryCount = (int) factoryPresets().size();
    const juce::File root = factoryPresetsDirectory().getParentDirectory();
    const juce::File bank = root.getChildFile ("srdtest");
    bank.deleteRecursively();
    bank.createDirectory();
    bank.getChildFile ("one.json").replaceWithText (
        "{\"name\":\"Alpha\",\"modKnob1\":\"lfoRate\",\"modKnob2\":\"lfoDepth\",\"values\":{}}");

    loadPresetBank ("srdtest");
    CHECK (currentPresetBank() == "srdtest", "srdtest bank not selected");
    CHECK ((int) factoryPresets().size() == 1, "srdtest has %d presets, want 1", (int) factoryPresets().size());
    p.setCurrentProgram (0);
    CHECK (p.getProgramName (0) == "Alpha", "first srdtest preset is '%s'", p.getProgramName (0).toRawUTF8());

    // Save: overwrites the preset's own file, the memory picks up the new values
    setParam (p, "cutoff", 0.5f);
    CHECK (p.saveCurrentPreset(), "save failed");
    CHECK (p.getCurrentProgram() == 0, "save moved the program to %d", p.getCurrentProgram());
    CHECK (valueOf (p, "cutoff") == 0.5f, "saved cutoff not reloaded (%g)", valueOf (p, "cutoff"));
    CHECK ((int) factoryPresets().size() == 1, "save duplicated a preset (%d)", (int) factoryPresets().size());
    CHECK (hasParamValue (p, 0, "cutoff", 0.5f), "save wrote cutoff to the .json");

    // New: writes a fresh file with a sanitized name and returns its index
    const int created = p.createNewPreset ("One Two");
    CHECK (created == 1, "create returned %d", created);
    CHECK (p.getProgramName (created) == "One Two", "created preset is '%s'", p.getProgramName (created).toRawUTF8());
    CHECK (bank.getChildFile ("one_two.json").exists(), "created file not written");
    CHECK ((int) factoryPresets().size() == 2, "bank has %d presets after create", (int) factoryPresets().size());

    // New with a duplicate name: OS-style counter on both the name and the file
    const int dup = p.createNewPreset ("One Two");
    CHECK (dup >= 0, "duplicate create failed %d", dup);
    CHECK (p.getProgramName (dup) == "One Two (1)", "duplicate is '%s'", p.getProgramName (dup).toRawUTF8());
    CHECK (bank.getChildFile ("one_two_1.json").exists(), "duplicate file not written");
    CHECK (bank.getChildFile ("one_two.json").exists(), "original file lost on duplicate");

    // New with a blank name must not create anything
    CHECK (p.createNewPreset ("   ") == -1, "blank name created a preset");
    CHECK ((int) factoryPresets().size() == 3, "blank name changed the bank size to %d", (int) factoryPresets().size());

    // Rename: renames the file, keeps the sound, reloads memory (the selection follows the new index,
    // exactly like the GUI re-selects the renamed preset after the "Renamed" blink)
    p.setCurrentProgram (created);
    const int renamed = p.renameCurrentPreset ("renamed patch");
    CHECK (renamed >= 0, "rename returned %d", renamed);
    CHECK (p.getProgramName (renamed) == "renamed patch", "renamed preset is '%s'", p.getProgramName (renamed).toRawUTF8());
    CHECK (bank.getChildFile ("renamed_patch.json").exists(), "renamed file not written");
    CHECK (! bank.getChildFile ("one_two.json").exists(), "old file still present after rename");
    CHECK (hasParamValue (p, renamed, "cutoff", 0.5f), "rename lost the sound");

    // Renaming to a name that exists disambiguates like New does
    p.setCurrentProgram (renamed);
    const int dupRename = p.renameCurrentPreset ("Alpha");
    CHECK (dupRename >= 0, "duplicate rename returned %d", dupRename);
    CHECK (p.getProgramName (dupRename) == "Alpha (1)", "renamed duplicate is '%s'", p.getProgramName (dupRename).toRawUTF8());
    CHECK (bank.getChildFile ("alpha_1.json").exists(), "disambiguated file not written");
    CHECK (! bank.getChildFile ("renamed_patch.json").exists(), "old renamed file still present");
    CHECK ((int) factoryPresets().size() == 3, "rename changed the bank size to %d", (int) factoryPresets().size());

    // Renaming to itself is a no-op that keeps its selection
    p.setCurrentProgram (dupRename);
    const int selfName = p.renameCurrentPreset ("Alpha (1)");
    CHECK (selfName == dupRename, "self-rename changed the index to %d", selfName);
    CHECK ((int) factoryPresets().size() == 3, "self-rename changed the bank size to %d", (int) factoryPresets().size());

    // Delete: removes the file, and selection moves to the preset that filled the slot
    CHECK (p.getProgramName (0) == "Alpha", "row 0 is '%s' before delete", p.getProgramName (0).toRawUTF8());
    p.setCurrentProgram (0);                     // delete "Alpha"
    const int next = p.deleteCurrentPreset();
    CHECK (next >= 0, "delete returned %d", next);
    CHECK (! bank.getChildFile ("alpha.json").exists(), "deleted file still present");
    CHECK ((int) factoryPresets().size() == 2, "bank has %d presets after delete", (int) factoryPresets().size());
    CHECK (p.getProgramName (next) == "Alpha (1)", "preset next to the deleted slot is '%s'", p.getProgramName (next).toRawUTF8());
    CHECK (next == 0, "selection moved to %d, want 0", next);   // "Alpha (1)" fills Alpha's old slot

    // in-memory presets mirror the folder
    CHECK ((int) bank.findChildFiles (juce::File::findFiles, false, "*.json").size() == 2,
           "folder has %d json after delete", (int) bank.findChildFiles (juce::File::findFiles, false, "*.json").size());
    (void) p;

    loadPresetBank (factoryBank);
    CHECK ((int) factoryPresets().size() == factoryCount, "factory restored %d, want %d",
           (int) factoryPresets().size(), factoryCount);
    bank.deleteRecursively();
}

// fundamental by autocorrelation over 50..500 Hz
static double fundamental (const juce::AudioBuffer<float>& x, int from, int len)
{
    double best = -1e300; int bestLag = 0;
    for (int lag = 88; lag < 882; ++lag)
    {
        double acc = 0.0;
        for (int i = from; i < from + len; ++i)
            acc += x.getSample (0, i) * x.getSample (0, i + lag);
        if (acc > best) { best = acc; bestLag = lag; }
    }
    return 44100.0 / bestLag;
}

// MIDI pitch bend spans an octave each way
static void testPitchBend (JC303& p)
{
    auto play = [&] (int wheel)
    {
        p.setCurrentProgram (0);
        setParam (p, "cutoff", 1.0f);
        setParam (p, "tuning", 0.0f);
        p.setPlayConfigDetails (0, 2, 44100.0, 512);
        p.prepareToPlay (44100.0, 512);
        juce::AudioBuffer<float> out (2, 512 * 40), buffer (2, 512);
        for (int b = 0; b < 40; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
            {
                midi.addEvent (juce::MidiMessage::pitchWheel (1, wheel), 0);
                midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 64), 0);
            }
            buffer.clear();
            p.processBlock (buffer, midi);
            out.copyFrom (0, b * 512, buffer, 0, 0, 512);
        }
        juce::MidiBuffer reset;
        reset.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 0);
        reset.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
        juce::AudioBuffer<float> scratch (2, 512);
        p.processBlock (scratch, reset);
        p.releaseResources();
        return fundamental (out, 4096, 4096);
    };
    const double centre = play (8192), up = play (16383), down = play (0);
    std::printf ("pitch bend: centre %.1f Hz, up %.1f Hz, down %.1f Hz\n", centre, up, down);
    CHECK (std::abs (centre - 110.0) < 2.0, "centre %.1f Hz", centre);
    CHECK (std::abs (up - 220.0) < 4.0, "full bend up %.1f Hz, want an octave up", up);
    CHECK (std::abs (down - 55.0) < 1.5, "full bend down %.1f Hz, want an octave down", down);
}

// the mod wheel (CC 1) drives mod knob 1's parameter: LFO rate on Init, the preset's own knob otherwise
static void testModWheelDrivesKnobOne (JC303& p)
{
    auto wheel = [&] (int value)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 1, value), 0);
        juce::AudioBuffer<float> buffer (2, 512);
        p.setPlayConfigDetails (0, 2, 44100.0, 512);
        p.prepareToPlay (44100.0, 512);
        p.processBlock (buffer, midi);
        p.releaseResources();
        p.applyPendingControllers();   // the message thread's side
    };
    auto normalised = [&] (const char* id) { return find (p, id)->getValue(); };

    p.setCurrentProgram (0);
    CHECK (p.getModKnob (0) == "lfoRate", "Init knob 1 is %s", p.getModKnob (0).toRawUTF8());
    wheel (127);
    CHECK (std::abs (normalised ("lfoRate") - 1.0f) < 1e-4f, "wheel 127 left lfoRate at %g", normalised ("lfoRate"));
    wheel (0);
    CHECK (normalised ("lfoRate") < 1e-4f, "wheel 0 left lfoRate at %g", normalised ("lfoRate"));

    p.setCurrentProgram (programIndex (p, "Super JP"));
    wheel (64);
    CHECK (std::abs (normalised ("superDetune") - 64.0f / 127.0f) < 0.006f,   // the parameter snaps to 0.01 steps
           "wheel 64 set superDetune %g", normalised ("superDetune"));
}

// every preset makes clean sound: finite, audible, under full scale, and level-matched to the bank
static void testEveryPresetRenders (JC303& p, const char* wavDir)
{
    std::vector<std::pair<juce::String, double>> attacks;
    for (auto& preset : factoryPresets())
    {
        const auto out = render (p, preset.name);
        bool finite = true;
        float peak = 0.0f;
        for (int c = 0; c < out.getNumChannels(); ++c)
            for (int i = 0; i < out.getNumSamples(); ++i)
            {
                finite = finite && std::isfinite (out.getSample (c, i));
                peak = std::max (peak, std::abs (out.getSample (c, i)));
            }
        const double attackDb = rmsDb (out, 0, (int) (0.15 * 44100.0));
        const double heldDb = rmsDb (out, (int) (1.75 * 44100.0), (int) (1.9 * 44100.0));
        std::printf ("  %-12s attack %6.1f dBFS, held %6.1f dBFS, peak %6.1f dBFS\n",
                     preset.name.toRawUTF8(), attackDb, heldDb,
                     20.0 * std::log10 (peak + 1e-30));
        CHECK (finite, "preset '%s' renders NaN/inf", preset.name.toRawUTF8());
        CHECK (attackDb > -60.0, "preset '%s' is silent (%.1f dBFS)", preset.name.toRawUTF8(), attackDb);
        CHECK (peak < 1.0f, "preset '%s' clips (peak %.2f)", preset.name.toRawUTF8(), peak);
        attacks.push_back ({ preset.name, attackDb });

        if (wavDir != nullptr)
            writeWav (out, juce::File (wavDir).getChildFile (juce::String (preset.name).replace (" ", "_") + ".wav"));
    }

    auto sorted = attacks;
    std::sort (sorted.begin(), sorted.end(), [] (auto& a, auto& b) { return a.second < b.second; });
    const double median = sorted[sorted.size() / 2].second;
    for (auto& [name, db] : attacks)
        CHECK (std::abs (db - median) < 15.0, "preset '%s' attack %.1f dB is far from the bank median %.1f dB",
               name.toRawUTF8(), db, median);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    {
        JC303 p;
        testProgramList (p);
        testPresetIdsExist (p);
        testPresetValuesInRange (p);
        testPresetKnobsExist (p);
        testPresetPage();
        testBankSwitch();
        testSaveRenameDelete (p);
        testEveryPresetHasVolume (p);
        testInitResetsSoundOnly (p);
        testHoover (p);
        testHooverRenders (p, argc > 1 ? argv[1] : nullptr);
        testEveryPresetRenders (p, argc > 2 ? argv[2] : nullptr);
        testPitchBend (p);
        testModWheelDrivesKnobOne (p);
    }
    testModKnobs();

    if (failures == 0)
        std::printf ("preset_test: all passed\n");
    return failures == 0 ? 0 : 1;
}
