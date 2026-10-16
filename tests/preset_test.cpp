// Factory preset tests against the real JC303 processor and its parameter layout.

#include "JC303.h"
#include "dfl_FactoryPresets.h"

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

static void testProgramList (JC303& p)
{
    CHECK (p.getNumPrograms() >= 2, "only %d programs", p.getNumPrograms());
    CHECK (p.getProgramName (0) == "Init", "first program is '%s'", p.getProgramName (0).toRawUTF8());
    CHECK (programIndex (p, "Hoover") > 0, "no Hoover program");
}

static void testPresetIdsExist (JC303& p)
{
    for (auto& preset : dfl::factoryPresets())
        for (auto& [id, v] : preset.values)
            CHECK (find (p, id) != nullptr, "preset '%s' sets unknown parameter '%s'", preset.name, id);
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
    CHECK (std::abs (valueOf (p, "volume") - 0.4f) < 1e-4f, "Init changed the volume to %g", valueOf (p, "volume"));
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

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    {
        JC303 p;
        testProgramList (p);
        testPresetIdsExist (p);
        testInitResetsSoundOnly (p);
        testHoover (p);
    }

    if (failures == 0)
        std::printf ("preset_test: all passed\n");
    return failures == 0 ? 0 : 1;
}
