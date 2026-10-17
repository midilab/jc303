// Factory preset tests against the real JC303 processor and its parameter layout.

#include "JC303.h"
#include "dfl_FactoryPresets.h"

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

static void testPresetValuesInRange (JC303& p)
{
    // convertTo0to1 clamps silently, so an out-of-range value or choice index would load as something else
    for (auto& preset : dfl::factoryPresets())
        for (auto& [id, v] : preset.values)
            if (auto* param = find (p, id))
            {
                const float loaded = param->convertFrom0to1 (param->convertTo0to1 (v));
                CHECK (std::abs (loaded - v) < 1e-3f, "preset '%s': %s = %g loads as %g", preset.name, id, v, loaded);
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

// every preset makes clean sound: finite, audible, under full scale, and level-matched to the bank
static void testEveryPresetRenders (JC303& p, const char* wavDir)
{
    std::vector<std::pair<const char*, double>> attacks;
    for (auto& preset : dfl::factoryPresets())
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
        std::printf ("  %-12s attack %6.1f dBFS, held %6.1f dBFS, peak %6.1f dBFS\n", preset.name, attackDb, heldDb,
                     20.0 * std::log10 (peak + 1e-30));
        CHECK (finite, "preset '%s' renders NaN/inf", preset.name);
        CHECK (attackDb > -60.0, "preset '%s' is silent (%.1f dBFS)", preset.name, attackDb);
        CHECK (peak < 1.0f, "preset '%s' clips (peak %.2f)", preset.name, peak);
        attacks.push_back ({ preset.name, attackDb });

        if (wavDir != nullptr)
            writeWav (out, juce::File (wavDir).getChildFile (juce::String (preset.name).replace (" ", "_") + ".wav"));
    }

    auto sorted = attacks;
    std::sort (sorted.begin(), sorted.end(), [] (auto& a, auto& b) { return a.second < b.second; });
    const double median = sorted[sorted.size() / 2].second;
    for (auto& [name, db] : attacks)
        CHECK (std::abs (db - median) < 15.0, "preset '%s' attack %.1f dB is far from the bank median %.1f dB", name, db, median);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    {
        JC303 p;
        testProgramList (p);
        testPresetIdsExist (p);
        testPresetValuesInRange (p);
        testInitResetsSoundOnly (p);
        testHoover (p);
        testHooverRenders (p, argc > 1 ? argv[1] : nullptr);
        testEveryPresetRenders (p, argc > 2 ? argv[2] : nullptr);
    }

    if (failures == 0)
        std::printf ("preset_test: all passed\n");
    return failures == 0 ? 0 : 1;
}
