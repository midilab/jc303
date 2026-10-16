#include "JC303.h"
#include GUI_THEME_HEADER

namespace
{
    // Time mappings shared by the parameter text and setParameter, so the menu shows what you hear
    double decayMs (double v)  { return linToLin (v, 0.0, 1.0, 30.0, 3000.0); }
    double attackMs (double v) { return linToExp (v, 0.0, 1.0, 0.3, 3000.0); }
    double slideMs (double v)  { return linToLin (v, 0.0, 1.0, 2.0, 360.0); }
    double lfoRateHz (double v) { return linToExp (v, 0.0, 1.0, 0.1, 20.0); }
    double feedbackHpfHz (double v) { return linToExp (v, 0.0, 1.0, 350.0, 100.0); }

    juce::AudioParameterFloatAttributes unitAttributes (double (*toUnits) (double), const char* unit)
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([toUnits, unit] (float v, int)
            {
                const double x = std::round (toUnits (v) * 10.0) / 10.0;  // max 1 decimal place
                return juce::String (x, x == std::floor (x) ? 0 : 1) + unit;
            });
    }

    // order matches rosic::ModSource
    juce::StringArray modSourceNames()
    {
        return { "Off", "LFO", "Env", "Accent", "Key", "Random", "Cutoff" };
    }

    // order matches rosic::ModDestination
    juce::StringArray modDestinationNames()
    {
        return { "Cutoff", "Volume", "Pitch", "Pulse Width", "Resonance", "Overdrive", "Filter FM",
                 "Env Mod", "Decay", "Accent", "Waveform", "LFO Rate", "LFO Depth", "Filter Morph" };
    }

    // -1..+1 params read -100%..+100% in the menu
    juce::AudioParameterFloatAttributes bipolarAttributes()
    {
        return juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction ([] (float v, int)
            {
                const int pct = juce::roundToInt (v * 100.0f);
                return (pct > 0 ? "+" : "") + juce::String (pct) + "%";
            });
    }
}

//==============================================================================
JC303::JC303()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       parameters (*this, nullptr, juce::Identifier("APVTS"), {
            std::make_unique<juce::AudioParameterFloat> ("waveform",
                                                        "Waveform",
                                                        0.0f,
                                                        1.0f,
                                                        1.0f),
            std::make_unique<juce::AudioParameterFloat> ("tuning",
                                                        "Tuning",
                                                        0.0f,
                                                        1.0f,
                                                        0.5f),
            std::make_unique<juce::AudioParameterFloat> ("cutoff",
                                                        "Cutoff",
                                                        0.0f,
                                                        1.0f,
                                                        0.0f),
            std::make_unique<juce::AudioParameterFloat> ("resonance",
                                                        "Resonance",
                                                        0.0f,
                                                        1.0f,
                                                        0.92f),
            std::make_unique<juce::AudioParameterFloat> ("envmod",
                                                        "EnvMod",
                                                        0.0f,
                                                        1.0f,
                                                        0.0f),
            std::make_unique<juce::AudioParameterFloat> ("decay",
                                                        "Decay",
                                                        0.0f,
                                                        1.0f,
                                                        0.29f),
            std::make_unique<juce::AudioParameterFloat> ("accent",
                                                        "Accent",
                                                        0.0f,
                                                        1.0f,
                                                        0.78f),
            std::make_unique<juce::AudioParameterFloat> ("volume",
                                                        "Volume",
                                                        0.0f,
                                                        1.0f,
                                                        0.75f),
            // MODs parameters
            std::make_unique<juce::AudioParameterFloat> ("normalDecay",
                                                        "Normal Decay",
                                                        juce::NormalisableRange<float> (0.0f, 1.0f),
                                                        0.3f,
                                                        unitAttributes (decayMs, " ms")),
            std::make_unique<juce::AudioParameterFloat> ("accentDecay",
                                                        "Accent Decay",
                                                        juce::NormalisableRange<float> (0.0f, 1.0f),
                                                        0.03f,
                                                        unitAttributes (decayMs, " ms")),
            std::make_unique<juce::AudioParameterFloat> ("accentSweep",
                                                        "Accent Sweep",
                                                        juce::NormalisableRange<float> (0.0f, 1.0f),
                                                        0.0f,   // 0 = stock 303
                                                        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                                            [] (float v, int) -> juce::String
                                                            {
                                                                const double ms = accentSweepMs (v);
                                                                return ms < 1000.0 ? juce::String (juce::roundToInt (ms)) + " ms"
                                                                                   : juce::String (ms / 1000.0, 2) + " s";
                                                            })),
            std::make_unique<juce::AudioParameterFloat> ("feedbackFilter",
                                                        "Filt. FeedBack",
                                                        juce::NormalisableRange<float> (0.0f, 1.0f),
                                                        0.63f,
                                                        unitAttributes (feedbackHpfHz, " Hz")),
            std::make_unique<juce::AudioParameterFloat> ("softAttack",
                                                        "Soft Attack",
                                                        juce::NormalisableRange<float> (0.0f, 1.0f),
                                                        0.26f,
                                                        unitAttributes (attackMs, " ms")),
            std::make_unique<juce::AudioParameterFloat> ("slideTime",
                                                        "Slide time",
                                                        juce::NormalisableRange<float> (0.0f, 1.0f),
                                                        0.33f,
                                                        unitAttributes (slideMs, " ms")),
            std::make_unique<juce::AudioParameterFloat> ("sqrDriver",
                                                        "Square Drive",
                                                        0.0f,
                                                        1.0f,
                                                        0.25f),
            std::make_unique<juce::AudioParameterBool> ("switchModState",
                                                        "Switch Mod",
                                                        false),
            // LFO parameters
            std::make_unique<juce::AudioParameterChoice> ("lfoWaveform",
                                                        "LFO Wave",
                                                        juce::StringArray{ "Triangle", "Saw Up", "Saw Down", "Square", "Random", "Pink Noise" },
                                                        0),
            std::make_unique<juce::AudioParameterFloat> ("lfoRate",
                                                        "LFO Rate",
                                                        juce::NormalisableRange<float> (0.0f, 1.0f),
                                                        0.25f,
                                                        unitAttributes (lfoRateHz, " Hz")),
            std::make_unique<juce::AudioParameterFloat> ("lfoDepth",
                                                        "LFO Depth",
                                                        0.0f,
                                                        1.0f,
                                                        0.0f),
            std::make_unique<juce::AudioParameterChoice> ("lfoDestination",
                                                        "LFO Destination",
                                                        modDestinationNames(),
                                                        0),
            std::make_unique<juce::AudioParameterFloat> ("lfoPhase",
                                                        "LFO Phase",
                                                        0.0f,
                                                        1.0f,
                                                        0.0f),   // start phase on key-sync: 0..1 = 0..360 deg
            std::make_unique<juce::AudioParameterBool> ("lfoSync",
                                                        "LFO Key Sync",
                                                        false),
            std::make_unique<juce::AudioParameterFloat> ("lfoContour",
                                                        "LFO Contour",
                                                        juce::NormalisableRange<float> (-1.0f, 1.0f),
                                                        0.0f,
                                                        bipolarAttributes()),   // lag/slew (<0) or edge/sag shaping (>0), see dfl::LFO
            std::make_unique<juce::AudioParameterChoice> ("modSlot2Dest",
                                                        "Mod Slot 2 Destination",
                                                        modDestinationNames(),
                                                        0),
            std::make_unique<juce::AudioParameterFloat> ("modSlot2Amount",
                                                        "Mod Slot 2 Amount",
                                                        juce::NormalisableRange<float> (-1.0f, 1.0f),
                                                        0.0f,
                                                        bipolarAttributes()),
            std::make_unique<juce::AudioParameterChoice> ("modSlot3Source",
                                                        "Mod Slot 3 Source",
                                                        modSourceNames(),
                                                        0),
            std::make_unique<juce::AudioParameterChoice> ("modSlot3Dest",
                                                        "Mod Slot 3 Destination",
                                                        modDestinationNames(),
                                                        0),
            std::make_unique<juce::AudioParameterFloat> ("modSlot3Amount",
                                                        "Mod Slot 3 Amount",
                                                        juce::NormalisableRange<float> (-1.0f, 1.0f),
                                                        0.0f,
                                                        bipolarAttributes()),
            std::make_unique<juce::AudioParameterChoice> ("modSlot4Source",
                                                        "Mod Slot 4 Source",
                                                        modSourceNames(),
                                                        0),
            std::make_unique<juce::AudioParameterChoice> ("modSlot4Dest",
                                                        "Mod Slot 4 Destination",
                                                        modDestinationNames(),
                                                        0),
            std::make_unique<juce::AudioParameterFloat> ("modSlot4Amount",
                                                        "Mod Slot 4 Amount",
                                                        juce::NormalisableRange<float> (-1.0f, 1.0f),
                                                        0.0f,
                                                        bipolarAttributes()),
            // overdrive
            std::make_unique<juce::AudioParameterInt> ("overdriveModelIndex",
                                                        "Overdrive Model Index",
                                                        0,
                                                        loadOverdriveTones() - 1,
                                                        1),
            std::make_unique<juce::AudioParameterFloat> ("overdriveLevel",
                                                        "Drive",
                                                        0.0f,
                                                        1.0f,
                                                        0.15f),
            std::make_unique<juce::AudioParameterFloat> ("overdriveDryWet",
                                                        "Dry/Wet",
                                                        0.0f,
                                                        1.0f,
                                                        0.35f),
            std::make_unique<juce::AudioParameterBool> ("switchOverdriveState",
                                                        "Switch Overdrive Mod",
                                                        false),
            std::make_unique<juce::AudioParameterFloat> ("overdriveDisperse",
                                                        "Overdrive Disperse",
                                                        juce::NormalisableRange<float> (0.0f, 1.0f),
                                                        0.0f,   // allpass before the overdrive: 0 = off
                                                        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                                            [] (float v, int) -> juce::String
                                                            {
                                                                return v <= 0.0f ? juce::String ("Off")
                                                                                 : juce::String (juce::roundToInt (Disperser::cornerFor (v))) + " Hz";
                                                            })),
            std::make_unique<juce::AudioParameterBool> ("overdrivePreVca",
                                                        "Overdrive Pre-VCA",
                                                        false),
            // filter model selection
            std::make_unique<juce::AudioParameterChoice> ("filterType",
                                                        "Filter Model",
                                                        juce::StringArray{ "TeeBee", "Spicy", "MissBehave", "JuneBug", "Wasabi", "Hornet", "SemiSweet", "Oddity" },
                                                        FILTER_TEEBEE),
            // square wave model: Open303's wavetable or the TB-303 circuit model
            std::make_unique<juce::AudioParameterChoice> ("squareModel",
                                                        "Square Model",
                                                        juce::StringArray{ "Open303", "Circuit" },
                                                        SQUARE_CIRCUIT),
            std::make_unique<juce::AudioParameterFloat> ("filterDrive",
                                                        "Filter Saturation",
                                                        0.0f,
                                                        1.0f,
                                                        0.0f),   // 0 dB into the diode saturator: the real 303's level (6.5 Vpp saw, see docs/spicy-filter.md)
            std::make_unique<juce::AudioParameterFloat> ("filterMorph",
                                                        "Filter Morph",
                                                        0.0f,
                                                        1.0f,
                                                        0.0f),   // Diode: 0 = lowpass, 0.5 = bandpass, 1 = highpass
            std::make_unique<juce::AudioParameterFloat> ("bassComp",
                                                        "Bass Comp",
                                                        0.0f,
                                                        1.0f,
                                                        0.0f),   // Diode: 0 = the real circuit's resonance bass loss
            std::make_unique<juce::AudioParameterFloat> ("filterFm",
                                                        "Filter FM",
                                                        0.0f,
                                                        1.0f,
                                                        0.0f),   // Devilfish audio-rate cutoff FM, off by default
            std::make_unique<juce::AudioParameterFloat> ("resTrim",
                                                        "Res Trim",
                                                        0.0f,
                                                        1.0f,
                                                        0.5f),   // diode unit trim, 0.5 = nominal unit
            std::make_unique<juce::AudioParameterChoice> ("filterQuality",
                                                        "Filter Quality",
                                                        juce::StringArray{ "Standard", "Hi-Fi" },
                                                        0),      // Diode: Standard (default) only Q12 / Q21 tanh; Hi-Fi solves every diode pair's tanh
            std::make_unique<juce::AudioParameterFloat> ("filterTension",
                                                        "Filter Tension",
                                                        juce::NormalisableRange<float> (-1.0f, 1.0f),
                                                        0.0f,    // Hi-Fi diode rungs: 0 = the circuit, - floppy, + stiff
                                                        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                                            [] (float v, int) { return signedPercent (bipolarDetent (v)); })),
            std::make_unique<juce::AudioParameterFloat> ("filterBias",
                                                        "Saturation Bias",
                                                        0.0f,
                                                        1.0f,
                                                        0.5f),   // Diode: Q12's asymmetry and driven swell; MS-20: its input stage; Wasp: OTA1's divider; ARP 4075: its limiter pair; not on JuneBug / SemiSweet; acts above 0 dB Saturation (0 = symmetric)
            std::make_unique<juce::AudioParameterFloat> ("filterRail",
                                                        "Saturation Rail",
                                                        0.0f,
                                                        1.0f,
                                                        1.0f),   // Diode Hi-Fi: ceiling on the ladder's stage voltages; 1 = unlimited (the full crunch), lower = cleaner; MS-20 / IR3109: resonance limiter, Wasp / SEM: supply sag, 1 = the circuit
            // generative sequencer parameters
            std::make_unique<juce::AudioParameterFloat> ("seqGenerativeFill",
                                                    "Seq Generative Fill",
                                                    0.0f,
                                                    100.0f,
                                                    80.0f),
            std::make_unique<juce::AudioParameterFloat> ("seqGenerativeAccentProbability",
                                                    "Seq Generative Accent Prob",
                                                    0.0f,
                                                    100.0f,
                                                    50.0f),
            std::make_unique<juce::AudioParameterFloat> ("seqGenerativeSlideProbability",
                                                    "Seq Generative Slide Prob",
                                                    0.0f,
                                                    100.0f,
                                                    30.0f),
            std::make_unique<juce::AudioParameterFloat> ("seqGenerativeTieProbability",
                                                    "Seq Generative Tie Prob",
                                                    0.0f,
                                                    100.0f,
                                                    100.0f),
            std::make_unique<juce::AudioParameterFloat> ("numberOfTones",
                                                    "Number Of Tones",
                                                    1.0f,
                                                    12.0f,
                                                    4.0f),
            std::make_unique<juce::AudioParameterFloat> ("lowerNote",
                                                    "Lower Note",
                                                    0.0f,
                                                    127.0f,
                                                    20.0f),
            std::make_unique<juce::AudioParameterFloat> ("rangeNote",
                                                    "Range Note",
                                                    0.0f,
                                                    127.0f,
                                                    48.0f),
            //std::make_unique<juce::AudioParameterFloat> ("seqHarmonizer",
            //                                            "Seq Harmonizer",
            //                                            0.0f,
            //                                            13.0f,
            //                                            0.0f),
            std::make_unique<juce::AudioParameterChoice> ("seqSyncMode",
                                                        "Seq Sync Mode",
                                                        juce::StringArray{ "Internal", "Host", "Midi Clock" },
                                                        0),
            std::make_unique<juce::AudioParameterChoice> ("seqStartMode",
                                                        "Seq Start Mode",
                                                        juce::StringArray{ "Transport", "Note Trigger" },
                                                        0),
            std::make_unique<juce::AudioParameterInt> ("seqLength",
                                                        "Seq Length",
                                                        1,
                                                        16,
16),
            std::make_unique<juce::AudioParameterInt> ("seqTempo",
                                                        "Seq Tempo BPM",
                                                        20,
                                                        300,
                                                        120),
            std::make_unique<juce::AudioParameterBool> ("seqPlayState",
                                                        "Seq Play State",
                                                        false),
            std::make_unique<juce::AudioParameterBool> ("seqGenerate",
                                                        "Seq Generate",
                                                        false),
            std::make_unique<juce::AudioParameterBool> ("seqClear",
                                                        "Seq Clear",
                                                        false),
            std::make_unique<juce::AudioParameterFloat> ("subOscGain",
                                                        "Sub Osc",
                                                        0.0f,
                                                        1.0f,
                                                        0.0f),
            std::make_unique<juce::AudioParameterFloat> ("subOscBlend",
                                                        "Sub Octave -2/-1",
                                                        0.0f,    // -2 octaves (25% pulse)
                                                        1.0f,    // -1 octave (square)
                                                        1.0f)
        })
{
    // assign a pointer to use it around for each parameter
    waveForm = parameters.getRawParameterValue("waveform");
    tuning = parameters.getRawParameterValue("tuning");
    cutoffFreq = parameters.getRawParameterValue("cutoff");
    resonance = parameters.getRawParameterValue("resonance");
    envelopMod = parameters.getRawParameterValue("envmod");
    decay = parameters.getRawParameterValue("decay");
    accent = parameters.getRawParameterValue("accent");
    volume = parameters.getRawParameterValue("volume");
    // MODs parameters
    switchModState = parameters.getRawParameterValue("switchModState");
    normalDecay = parameters.getRawParameterValue("normalDecay");
    accentDecay = parameters.getRawParameterValue("accentDecay");
    accentSweep = parameters.getRawParameterValue("accentSweep");
    feedbackFilter = parameters.getRawParameterValue("feedbackFilter");
    softAttack = parameters.getRawParameterValue("softAttack");
    slideTime = parameters.getRawParameterValue("slideTime");
    sqrDriver = parameters.getRawParameterValue("sqrDriver");
    // LFO parameters
    lfoWaveform = parameters.getRawParameterValue("lfoWaveform");
    lfoRate = parameters.getRawParameterValue("lfoRate");
    lfoDepth = parameters.getRawParameterValue("lfoDepth");
    lfoDestination = parameters.getRawParameterValue("lfoDestination");
    lfoPhase = parameters.getRawParameterValue("lfoPhase");
    lfoSync = parameters.getRawParameterValue("lfoSync");
    lfoContour = parameters.getRawParameterValue("lfoContour");
    modSlotDest[0] = parameters.getRawParameterValue("modSlot2Dest");
    modSlotAmount[0] = parameters.getRawParameterValue("modSlot2Amount");
    modSlotSource[1] = parameters.getRawParameterValue("modSlot3Source");
    modSlotDest[1] = parameters.getRawParameterValue("modSlot3Dest");
    modSlotAmount[1] = parameters.getRawParameterValue("modSlot3Amount");
    modSlotSource[2] = parameters.getRawParameterValue("modSlot4Source");
    modSlotDest[2] = parameters.getRawParameterValue("modSlot4Dest");
    modSlotAmount[2] = parameters.getRawParameterValue("modSlot4Amount");
    // overdrive parameters
    overdriveModelIndex = parameters.getRawParameterValue("overdriveModelIndex");
    switchOverdriveState = parameters.getRawParameterValue("switchOverdriveState");
    overdriveLevel = parameters.getRawParameterValue("overdriveLevel");
    overdriveDryWet = parameters.getRawParameterValue("overdriveDryWet");
    overdriveDisperse = parameters.getRawParameterValue("overdriveDisperse");
    overdrivePreVca = parameters.getRawParameterValue("overdrivePreVca");
    // filter model
    filterType = parameters.getRawParameterValue("filterType");
    squareModel = parameters.getRawParameterValue("squareModel");
    filterDrive = parameters.getRawParameterValue("filterDrive");
    filterMorph = parameters.getRawParameterValue("filterMorph");
    bassComp = parameters.getRawParameterValue("bassComp");
    filterFm = parameters.getRawParameterValue("filterFm");
    resTrim = parameters.getRawParameterValue("resTrim");
    filterQuality = parameters.getRawParameterValue("filterQuality");
    filterTension = parameters.getRawParameterValue("filterTension");
    filterBias = parameters.getRawParameterValue("filterBias");
    filterRail = parameters.getRawParameterValue("filterRail");
    // generative sequencer parameters
    seqGenerativeFill = parameters.getRawParameterValue("seqGenerativeFill");
    seqGenerativeAccentProbability = parameters.getRawParameterValue("seqGenerativeAccentProbability");
    seqGenerativeSlideProbability = parameters.getRawParameterValue("seqGenerativeSlideProbability");
    seqGenerativeTieProbability = parameters.getRawParameterValue("seqGenerativeTieProbability");
    numberOfTones = parameters.getRawParameterValue("numberOfTones");
    lowerNote = parameters.getRawParameterValue("lowerNote");
    rangeNote = parameters.getRawParameterValue("rangeNote");
    //seqHarmonizer = parameters.getRawParameterValue("seqHarmonizer");
    seqLength = parameters.getRawParameterValue("seqLength");
    seqSyncMode = parameters.getRawParameterValue("seqSyncMode");
    seqStartMode = parameters.getRawParameterValue("seqStartMode");
    seqTempo = parameters.getRawParameterValue("seqTempo");
    seqPlayState = parameters.getRawParameterValue("seqPlayState");
    seqGenerate = parameters.getRawParameterValue("seqGenerate");
    seqClear = parameters.getRawParameterValue("seqClear");
    subOscGain = parameters.getRawParameterValue("subOscGain");
    subOscBlend = parameters.getRawParameterValue("subOscBlend");

    // force initial user values(some hosts migth not do it using value tree state)
    setParameter(WAVEFORM, *waveForm);
    setParameter(TUNING, *tuning);
    setParameter(CUTOFF, *cutoffFreq);
    setParameter(RESONANCE, *resonance);
    setParameter(ENVMOD, *envelopMod);
    setParameter(DECAY, *decay);
    setParameter(ACCENT, *accent);
    setParameter(VOLUME, *volume);
    // MOD/LFO/sub-osc values are applied only through setDevilMod so a mod-off
    // startup never overwrites the restored core defaults (they are all loaded
    // from the params inside setDevilMod(true)).
    setDevilMod(*switchModState);
    // overdrive parameters
    setParameter(OVERDRIVE_LEVEL, *overdriveLevel);
    setParameter(OVERDRIVE_DRY_WET, *overdriveDryWet);
    setParameter(OVERDRIVE_MODEL_INDEX, *overdriveModelIndex);
    setParameter(OVERDRIVE_DISPERSE, *overdriveDisperse);

    // presets and overdrive models
    setupDataDirectories();
    //installTones();
    //loadOverdriveTones();

    // Add parameter listeners
    parameters.addParameterListener("waveform", this);
    parameters.addParameterListener("tuning", this);
    parameters.addParameterListener("cutoff", this);
    parameters.addParameterListener("resonance", this);
    parameters.addParameterListener("envmod", this);
    parameters.addParameterListener("decay", this);
    parameters.addParameterListener("accent", this);
    parameters.addParameterListener("volume", this);
    parameters.addParameterListener("normalDecay", this);
    parameters.addParameterListener("accentDecay", this);
    parameters.addParameterListener("accentSweep", this);
    parameters.addParameterListener("feedbackFilter", this);
    parameters.addParameterListener("softAttack", this);
    parameters.addParameterListener("slideTime", this);
    parameters.addParameterListener("sqrDriver", this);
    parameters.addParameterListener("switchModState", this);
    // LFO parameter listeners
    parameters.addParameterListener("lfoWaveform", this);
    parameters.addParameterListener("lfoRate", this);
    parameters.addParameterListener("lfoDepth", this);
    parameters.addParameterListener("lfoDestination", this);
    parameters.addParameterListener("lfoPhase", this);
    parameters.addParameterListener("lfoSync", this);
    parameters.addParameterListener("lfoContour", this);
    parameters.addParameterListener("modSlot2Dest", this);
    parameters.addParameterListener("modSlot2Amount", this);
    parameters.addParameterListener("modSlot3Source", this);
    parameters.addParameterListener("modSlot3Dest", this);
    parameters.addParameterListener("modSlot3Amount", this);
    parameters.addParameterListener("modSlot4Source", this);
    parameters.addParameterListener("modSlot4Dest", this);
    parameters.addParameterListener("modSlot4Amount", this);
    // overdrive parameter listeners
    parameters.addParameterListener("overdriveLevel", this);
    parameters.addParameterListener("overdriveDryWet", this);
    parameters.addParameterListener("overdriveModelIndex", this);
    parameters.addParameterListener("overdriveDisperse", this);
    parameters.addParameterListener("switchOverdriveState", this);
    parameters.addParameterListener("filterType", this);
    parameters.addParameterListener("squareModel", this);
    parameters.addParameterListener("filterDrive", this);
    parameters.addParameterListener("filterMorph", this);
    parameters.addParameterListener("bassComp", this);
    parameters.addParameterListener("filterFm", this);
    parameters.addParameterListener("resTrim", this);
    parameters.addParameterListener("filterQuality", this);
    parameters.addParameterListener("filterTension", this);
    parameters.addParameterListener("filterBias", this);
    parameters.addParameterListener("filterRail", this);
    // generative sequencer parameter listener
    parameters.addParameterListener("seqPlayState", this);
    parameters.addParameterListener("seqGenerate", this);
    parameters.addParameterListener("seqClear", this);
    //parameters.addParameterListener("seqHarmonizer", this);
    parameters.addParameterListener("seqLength", this);
    parameters.addParameterListener("seqSyncMode", this);
    parameters.addParameterListener("seqStartMode", this);
    parameters.addParameterListener("seqTempo", this);
    parameters.addParameterListener("subOscGain", this);
    parameters.addParameterListener("subOscBlend", this);

    // ── Sequencer callback ────────────────────────────────────────────────────
    // Runs on the audio thread. Stores each event into _pendingNotes[] so
    // processBlock can deliver it to Open303 at the correct sample offset
    // inside the existing render loop.
    _sequencer.onNoteEvent = [this] (const Acid303Event& ev)
    {
        if (_pendingCount < kPendingMax)
            _pendingNotes[_pendingCount++] = { ev.type, ev.note, ev.velocity, ev.sampleOffset };
    };

    // Reset held-note tracking whenever the sequencer silences its note stack
    // (stop, mute, clearTrack, acidRandomize) so no stale gate is left open.
    _sequencer.onAllNotesOff = [this]
    {
        _heldNote         = -1;
        _lastStepHadSlide = false;
    };

    // Sequence defaults are driven by the APVTS params (seqSyncMode/seqStartMode/seqTempo)
    // and the AcidSeq303 child (stepLength) on state restore.
    _sequencer.setTrackLength (16);
}

JC303::~JC303()
{
    parameters.removeParameterListener("waveform", this);
    parameters.removeParameterListener("tuning", this);
    parameters.removeParameterListener("cutoff", this);
    parameters.removeParameterListener("resonance", this);
    parameters.removeParameterListener("envmod", this);
    parameters.removeParameterListener("decay", this);
    parameters.removeParameterListener("accent", this);
    parameters.removeParameterListener("volume", this);
    parameters.removeParameterListener("normalDecay", this);
    parameters.removeParameterListener("accentDecay", this);
    parameters.removeParameterListener("accentSweep", this);
    parameters.removeParameterListener("feedbackFilter", this);
    parameters.removeParameterListener("softAttack", this);
    parameters.removeParameterListener("slideTime", this);
    parameters.removeParameterListener("sqrDriver", this);
    parameters.removeParameterListener("switchModState", this);
    // LFO parameter listeners
    parameters.removeParameterListener("lfoWaveform", this);
    parameters.removeParameterListener("lfoRate", this);
    parameters.removeParameterListener("lfoDepth", this);
    parameters.removeParameterListener("lfoDestination", this);
    parameters.removeParameterListener("lfoPhase", this);
    parameters.removeParameterListener("lfoSync", this);
    parameters.removeParameterListener("lfoContour", this);
    parameters.removeParameterListener("modSlot2Dest", this);
    parameters.removeParameterListener("modSlot2Amount", this);
    parameters.removeParameterListener("modSlot3Source", this);
    parameters.removeParameterListener("modSlot3Dest", this);
    parameters.removeParameterListener("modSlot3Amount", this);
    parameters.removeParameterListener("modSlot4Source", this);
    parameters.removeParameterListener("modSlot4Dest", this);
    parameters.removeParameterListener("modSlot4Amount", this);
    // overdrive parameter listeners
    parameters.removeParameterListener("overdriveLevel", this);
    parameters.removeParameterListener("overdriveDryWet", this);
    parameters.removeParameterListener("overdriveModelIndex", this);
    parameters.removeParameterListener("overdriveDisperse", this);
    parameters.removeParameterListener("switchOverdriveState", this);
    parameters.removeParameterListener("filterType", this);
    parameters.removeParameterListener("squareModel", this);
    parameters.removeParameterListener("filterDrive", this);
    parameters.removeParameterListener("filterMorph", this);
    parameters.removeParameterListener("bassComp", this);
    parameters.removeParameterListener("filterFm", this);
    parameters.removeParameterListener("resTrim", this);
    parameters.removeParameterListener("filterQuality", this);
    parameters.removeParameterListener("filterTension", this);
    parameters.removeParameterListener("filterBias", this);
    parameters.removeParameterListener("filterRail", this);
    // generative sequencer
    parameters.removeParameterListener("seqPlayState", this);
    parameters.removeParameterListener("seqGenerate", this);
    parameters.removeParameterListener("seqClear", this);
    //parameters.removeParameterListener("seqHarmonizer", this);
    parameters.removeParameterListener("seqLength", this);
    parameters.removeParameterListener("seqSyncMode", this);
    parameters.removeParameterListener("seqStartMode", this);
    parameters.removeParameterListener("seqTempo", this);
    parameters.removeParameterListener("subOscGain", this);
    parameters.removeParameterListener("subOscBlend", this);
}

// Parameter change callback. Runs on the message/automation thread, NOT the audio thread, so it may
// only touch cheap flags here - the actual open303 setters (which rewrite filter/envelope
// coefficients the audio thread reads inside getSample) are deferred to updateOpen303Parameters(),
// applied at the top of processBlock. This closes the torn-read race Fable flagged.
void JC303::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Exception: loading an overdrive model allocates and touches the filesystem, which must happen
    // off the audio thread. Keep it here on the message thread rather than deferring it.
    if (parameterID == "overdriveModelIndex") {
        setParameter(OVERDRIVE_MODEL_INDEX, newValue);
        return;
    }

    // Sequencer transport/edit commands bypass the deferred DSP path: play/stop/generate/clear are
    // queued through _seqCommand and consumed on the audio thread by applySeqCommands().
    if (parameterID == "seqPlayState") {
        _seqCommand.store (newValue > 0.5f ? static_cast<int>(SeqCommand::Play)
                                           : static_cast<int>(SeqCommand::Stop),
                           std::memory_order_release);
        return;
    }
    else if (parameterID == "seqGenerate") {
        _seqCommand.store (static_cast<int>(SeqCommand::Generate), std::memory_order_release);
        return;
    }
    else if (parameterID == "seqClear") {
        _seqCommand.store (static_cast<int>(SeqCommand::Clear), std::memory_order_release);
        return;
    }
    //else if (parameterID == "seqHarmonizer") {
    //    uint8_t seqHarmony = static_cast<uint8_t>(*seqHarmonizer);
    //    if (seqHarmony == 0) {
    //        _sequencer.setTune(0);
    //    } else {
    //        _sequencer.setTemperament(seqHarmony - 1);
    //        _sequencer.setTune(1);
    //    }
    //}
    else if (parameterID == "seqLength") {
        if (auto* p = dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter("seqLength")))
            _sequencer.setTrackLength(static_cast<uint8_t>(p->get()));
        return;
    }
    else if (parameterID == "seqSyncMode") {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(parameters.getParameter("seqSyncMode")))
            _sequencer.setSyncMode((AcidSequencer303::SyncMode) p->getIndex());
        return;
    }
    else if (parameterID == "seqStartMode") {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*>(parameters.getParameter("seqStartMode")))
            _sequencer.setStartMode((AcidSequencer303::StartMode) p->getIndex());
        return;
    }
    else if (parameterID == "seqTempo") {
        if (auto* p = dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter("seqTempo")))
            _sequencer.setTempo((float) p->get());
        return;
    }

    // Everything else (the main knobs, the devil mod toggle + its pots, filter/square model, LFO,
    // sub-osc, and overdrive level/mix) maps to cheap DSP setters. Flag them so the
    // audio thread re-applies the current APVTS values in updateOpen303Parameters().
    parametersNeedUpdate.store(true);
}

// Consumes a sequencer command queued by the UI thread.  Must run on the audio
// thread, after _pendingCount has been reset, so all Open303 calls and sequencer
// emits stay on the audio thread while the device is rendering.
void JC303::applySeqCommands()
{
    const int cmd = _seqCommand.exchange (static_cast<int>(SeqCommand::None),
                                          std::memory_order_acq_rel);
    if (cmd == static_cast<int>(SeqCommand::None))
        return;

    switch (static_cast<SeqCommand>(cmd))
    {
    case SeqCommand::Play:
        _heldNote         = -1;
        _lastStepHadSlide = false;
        _sequencer.start();
        break;

    case SeqCommand::Stop:
        _heldNote         = -1;
        _lastStepHadSlide = false;
        _sequencer.stop();
        break;

    case SeqCommand::Clear:
        _sequencer.clearTrack();
        break;

    case SeqCommand::Generate:
        _sequencerMuted.store (true, std::memory_order_release);

        for (int i = 0; i <= 127; i++)
            open303Core.noteOn (i, 0, 0);
        _heldNote         = -1;
        _lastStepHadSlide = false;

        _sequencer.acidRandomize(
            static_cast<uint8_t>(*seqGenerativeFill),
            static_cast<uint8_t>(*seqGenerativeAccentProbability),
            static_cast<uint8_t>(*seqGenerativeSlideProbability),
            static_cast<uint8_t>(*seqGenerativeTieProbability),
            static_cast<uint8_t>(*numberOfTones),
            static_cast<uint8_t>(*lowerNote),
            static_cast<uint8_t>(*rangeNote)
        );

        _sequencerMuted.store (false, std::memory_order_release);
        break;

    case SeqCommand::None:
    default:
        break;
    }
}

// Applies every cheap DSP parameter from the current APVTS atomic values. Audio-thread only.
void JC303::updateOpen303Parameters()
{
    setParameter(WAVEFORM, *waveForm);
    setParameter(TUNING, *tuning);
    setParameter(CUTOFF, *cutoffFreq);
    setParameter(RESONANCE, *resonance);
    setParameter(ENVMOD, *envelopMod);
    setParameter(ACCENT, *accent);
    setParameter(VOLUME, *volume);
    // setDevilMod adjusts decayMin/decayMax and (re)applies the mod pots (or restores fixed 303
    // values), so it must run before DECAY, which reads that range.
    setDevilMod(*switchModState > 0.5f);
    setParameter(DECAY, *decay);
    // Overdrive gain/mix/disperse are cheap; the model index is loaded on the message thread (see above).
    setParameter(OVERDRIVE_LEVEL, *overdriveLevel);
    setParameter(OVERDRIVE_DRY_WET, *overdriveDryWet);
    setParameter(OVERDRIVE_DISPERSE, *overdriveDisperse);
}

void JC303::setParameter (Open303Parameters index, float value)
{
  if( index < 0 || index >= OPEN303_NUM_PARAMETERS )
    return;

	switch(index)
	{
    case WAVEFORM:
        open303Core.setWaveform(
            linToLin(value, 0.0, 1.0,   0.0,      1.0)
        );
        break;
    case TUNING:
        open303Core.setTuning(
            linToLin(value, 0.0, 1.0,  400.0,    480.0)
        );
        break;
    case CUTOFF:
        open303Core.setCutoff(
            linToExp(value, 0.0, 1.0, rosic::Open303::CUTOFF_KNOB_MIN, rosic::Open303::CUTOFF_KNOB_MAX)
        );
        break;
    case RESONANCE:
        open303Core.setResonance(
            linToLin(value, 0.0, 1.0,   0.0,    100.0)
        );
        break;
    case ENVMOD:
        open303Core.setEnvMod(
            linToLin(value, 0.0, 1.0,    0.0,   100.0)
        );
        break;
    case DECAY:
        open303Core.setDecay(
            linToExp(value, 0.0, 1.0,  decayMin,  decayMax)
        );
        break;
    case ACCENT:
        open303Core.setAccent(
            linToLin(value, 0.0, 1.0,   0.0,    100.0)
        );
        break;
    case VOLUME:
        open303Core.setVolume(
            linToLin(value, 0.0, 1.0, -60.0,      0.0)
        );
        break;

    // Overdrive - By GuitarML BYOD implementation
    case OVERDRIVE_LEVEL:
        // conditioned param or gain if model is not conditioned
        guitarML.setDriver(value);
        preVcaOverdrive.amp.setDriver(value);
        break;
    case OVERDRIVE_DRY_WET:
        overdriveMix.setWetMixProportion(value);
        preVcaOverdrive.mix.setWetMixProportion(value);
        break;
    case OVERDRIVE_MODEL_INDEX:
        // load new model
        //guitarML.loadModel(value);
        guitarML.loadUserModel(value);
        preVcaOverdrive.amp.loadUserModel(value);
        break;
    case OVERDRIVE_DISPERSE:
        overdriveDisperser.setAmount(value);
        preVcaOverdrive.disperser.setAmount(value);
        break;

    //
    // MODS (mostly based on devilfish mod)
    // BUT DONT! dont expect a devilfish clone sound or mail me about!
    // https://www.firstpr.com.au/rwi/dfish/Devil-Fish-Manual.pdf
    //
    case NORMAL_DECAY:
        /*
        Background (Devil Fish): on non-accented notes the TB-303's Main Envelope Generator (MEG) had
        a decay time between 200 ms and 2 s (the Decay pot); on accented notes it was fixed to 200 ms.
        The Devil Fish adds Normal Decay and Accent Decay pots, each 30 ms - 3 s.

        NOTE: unlike the Devil Fish "Normal Decay" (which retimes the MEG / filter envelope), this
        control drives the *amplitude* envelope decay via setAmpDecay(). The MEG decay is still set by
        the main DECAY parameter. Kept as-is to preserve the current voicing; treat the name as
        "normal (amp) decay" rather than a literal Devil Fish MEG-decay clone.
        */
        open303Core.setAmpDecay(decayMs(value));
        break;
    case ACCENT_DECAY:
        open303Core.setAccentDecay(decayMs(value));
        break;
    case ACCENT_SWEEP:
        // accent capacitor time constant: 15 ms is the stock 303, slower builds up over consecutive accents
        open303Core.setAccentSweep(accentSweepMs(value));
        break;
    case FEEDBACK_HPF:
        open303Core.setFeedbackHighpass(feedbackHpfHz(value));
        break;
    case SOFT_ATTACK:
        open303Core.setNormalAttack(attackMs(value));
        break;
    case SLIDE_TIME:
        open303Core.setSlideTime(slideMs(value));
        break;
    case TANH_SHAPER_DRIVE:
        // one knob, two meanings, same direction (up = cleaner square): edge drive of the Open303 square and
        // waveshaper sag of the Circuit square (default 0.25 = the measured circuit); see Square Model
        open303Core.setTanhShaperDrive(
            linToLin(value, 0.0, 1.0,   25.0,     80.0)
        );
        open303Core.setPulseSag(rosic::Open303::sagForSquareDrive(value));
        break;
    case FILTER_DRIVE:
        // 0..1 -> 0..+16 dB into the diode ladder's saturating input stage. 0 dB (the default)
        // is the real 303's level; above it is grit. Level is compensated across the range (see setInputDrive), so the knob is
        // not a volume control. The ladder is stability-tested up to +16 dB.
        open303Core.setFilterDrive(
            linToLin(value, 0.0, 1.0,   0.0,     16.0)
        );
        break;
    case FILTER_MORPH:
        // 0..1 diode response mode: 0 = lowpass, 0.5 = bandpass, 1 = highpass
        open303Core.setFilterMode(value);
        break;
    case BASS_COMP:
        // 0..1 diode bass comp
        open303Core.setPassbandCompensation(value);
        break;
    case FILTER_FM:
        // 0..1 Devilfish-style audio-rate cutoff FM depth, applied directly
        open303Core.setFilterFmDepth(value);
        break;
    case RES_TRIM:
        // 0..1 -> x0.85..x1.15 loop gain for the diode models (component-tolerance spread; above x1 it may self-oscillate)
        open303Core.setResonanceTrim(linToLin(value, 0.0, 1.0, 0.85, 1.15));
        break;
    case FILTER_QUALITY:
        // 0 = Standard (Q12 and Q21 tanh only), 1 = Hi-Fi (every diode pair's tanh, solved per sample)
        open303Core.setFilterHiFi(value > 0.5f);
        break;
    case FILTER_TENSION:
        // -1..1 with a centre detent: how the Hi-Fi ladder's resonance moves with level (0 = the circuit's diodes)
        open303Core.setFilterTension(bipolarDetent(value));
        break;
    case FILTER_BIAS:
        // 0..1: Q12's asymmetry and driven swell, ramping in above 0 dB Saturation (DiodeLadderFilter::setSaturationBias;
        // the DT level tables are fitted at the default 0.5; higher = more swell and crunch near full resonance). MS-20:
        // the same offset on its input stage, ramping in the same way (Open303::updateCircuitCharacter)
        open303Core.setFilterBias(value);
        break;
    case FILTER_RAIL:
        // 0..1: a ceiling on the Hi-Fi ladder's stage voltages (DiodeLadderFilter::setRail); 1 = unlimited, the crunch
        // at high Saturation and Bias in full; lower = cleaner, toward what a real ladder can swing. MS-20 / IR3109: the
        // resonance limiter's threshold (their Clip), 1 = the circuit, lower = earlier. Wasp: its supply, sagging
        open303Core.setFilterRail(value);
        break;

    // LFO parameters
    case LFO_WAVEFORM:
        open303Core.setLfoWaveform((int) value);
        break;
    case LFO_RATE:
        open303Core.setLfoRate(lfoRateHz(value));
        break;
    case LFO_DEPTH:
        open303Core.setLfoDepth(value);
        break;
    case LFO_DESTINATION:
        open303Core.setLfoDestination((int) value);
        break;
    // sub oscillator
    case SUB_OSC_GAIN:
        open303Core.setSubOscGain(value);
        break;
    case SUB_OSC_BLEND:
        // 0 = -2 octaves (25% pulse), 1 = -1 octave (square)
        open303Core.setSubOscBlend(value);
        break;
    case LFO_PHASE:
        open303Core.setLfoPhase(value);
        break;
    case LFO_SYNC:
        open303Core.setLfoKeySync(value > 0.5f);
        break;
    case LFO_CONTOUR:
        open303Core.setLfoContour(value);
        break;
	}
}

// toogle/restore 303 original and mod modes
void JC303::setDevilMod(bool mode)
{
    if (mode == true) {
        decayMin = 30.0;
        decayMax = 3000.0;
        // main DECAY knob re-maps into the mod range right away
        setParameter(DECAY, *decay);
        setParameter(NORMAL_DECAY, *normalDecay);
        setParameter(ACCENT_DECAY, *accentDecay);
        setParameter(ACCENT_SWEEP, *accentSweep);
        setParameter(FEEDBACK_HPF, *feedbackFilter);
        setParameter(SOFT_ATTACK, *softAttack);
        setParameter(SLIDE_TIME, *slideTime);
        setParameter(TANH_SHAPER_DRIVE, *sqrDriver);
        open303Core.setFilterType(static_cast<FilterType>((int) *filterType));
        open303Core.setSquareModel(static_cast<SquareModel>((int) *squareModel));
        setParameter(FILTER_DRIVE, *filterDrive);
        setParameter(FILTER_MORPH, *filterMorph);
        setParameter(BASS_COMP, *bassComp);
        setParameter(FILTER_FM, *filterFm);
        setParameter(RES_TRIM, *resTrim);
        setParameter(FILTER_QUALITY, *filterQuality);
        setParameter(FILTER_TENSION, *filterTension);
        setParameter(FILTER_BIAS, *filterBias);
        setParameter(FILTER_RAIL, *filterRail);
        setParameter(SUB_OSC_GAIN, *subOscGain);
        setParameter(SUB_OSC_BLEND, *subOscBlend);
        setParameter(LFO_WAVEFORM, *lfoWaveform);
        setParameter(LFO_RATE, *lfoRate);
        setParameter(LFO_DEPTH, *lfoDepth);
        setParameter(LFO_DESTINATION, *lfoDestination);
        setParameter(LFO_PHASE, *lfoPhase);
        setParameter(LFO_SYNC, *lfoSync);
        setParameter(LFO_CONTOUR, *lfoContour);
        for (int i = 0; i < 3; ++i)
            updateModSlot(i);
        open303Core.setLfoOn(true);
        open303Core.setCircuitPulseShape(true);
    } else if (mode == false) {
        open303Core.setFilterType(FILTER_TEEBEE);
        open303Core.setSquareModel(SQUARE_OPEN303);
        open303Core.setLfoOn(false);   // hard-gates LFO; its internal values are never read while off
        open303Core.setCircuitPulseShape(false);   // moot here: Mods off is the Open303 square
        decayMin = 200.0;
        decayMax = 2000.0;
        setParameter(DECAY, *decay);   // main DECAY knob re-maps into the core range right away
        open303Core.setAmpDecay(1230.0);
        open303Core.setAccentDecay(200.0);
        open303Core.setAccentSweep(15.0);
        open303Core.setFeedbackHighpass(150.0);
        open303Core.setNormalAttack(3.0);
        open303Core.setSlideTime(60.0);
        open303Core.setTanhShaperDrive(36.9);
        open303Core.setSubOscGain(0.0); // core default; blend is irrelevant at gain 0
    }
}

//==============================================================================
const juce::String JC303::getName() const
{
    return JucePlugin_Name;
}

bool JC303::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool JC303::producesMidi() const
{
    // The sequencer always outputs MIDI so the DAW can route it to other instruments.
    return true;
}

bool JC303::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double JC303::getTailLengthSeconds() const
{
    return 0.0;
}

int JC303::getNumPrograms()
{
    return 1;
}

int JC303::getCurrentProgram()
{
    return 0;
}

void JC303::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String JC303::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void JC303::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void JC303::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // init open303
    open303Core.setSampleRate(sampleRate);
    // init guitarML
    guitarML.prepareProcessing(sampleRate, samplesPerBlock);
    // init overdrive dry/wet processor
    overdriveMix.prepare ({ sampleRate, (uint32_t) samplesPerBlock, 2 });
    overdriveMix.setMixingRule (juce::dsp::DryWetMixingRule::sin3dB);
    preVcaOverdrive.prepare (sampleRate);
    overdriveDisperser.prepare (sampleRate);

    // ── Sequencer ─────────────────────────────────────────────────────────────
    // 960 PPQN is the JUCE default and is compatible with all major DAWs.
    // The sequencer adapts all pulse maths to this resolution.
    _sequencer.prepare (sampleRate, 960.0);
    _pendingCount     = 0;
    _wasHostPlaying   = false;
    _heldNote         = -1;
    _lastStepHadSlide = false;
    _recHeldNote      = -1;
    _sustainArmed     = true;
    _recWasOn         = false;
}

void JC303::releaseResources()
{
    // Stop sequencer and flush any held notes when playback stops
    _sequencer.stop();
}

bool JC303::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void JC303::render303(juce::AudioBuffer<float>& buffer, int beginSample, int endSample)
{
    auto* monoChannel = buffer.getWritePointer(0);
    const bool preVca = *switchOverdriveState && *overdrivePreVca;
    open303Core.setPreVcaInsert(preVca ? &preVcaOverdrive : nullptr);

    double block[Open303::maxBlockSize];
    for (auto sample = beginSample; sample < endSample; sample += Open303::maxBlockSize)
    {
        const int n = juce::jmin(Open303::maxBlockSize, endSample - sample);
        open303Core.renderBlock(block, n);
        for (int i = 0; i < n; ++i)
            monoChannel[sample + i] = (float) block[i];
    }
}

void JC303::renderMidi (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const int numSamples = buffer.getNumSamples();
    int currentSample = 0;

    // ── Gather host transport info ────────────────────────────────────────────
    double ppqAtStart    = 0.0;
    double hostBpm       = static_cast<double> (_sequencer.getTempo());
    bool   hostIsPlaying = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())         hostBpm      = *bpm;
            if (auto ppq = pos->getPpqPosition()) ppqAtStart   = *ppq;
            hostIsPlaying = pos->getIsPlaying();
        }
    }

    // ── Detect host play start / stop edge (Host sync mode only) ─────────────
    // Auto-start on the host play edge only in TransportStart mode; in
    // NoteTriggered the first incoming MIDI note owns the start.
    if (_sequencer.getSyncMode() == AcidSequencer303::SyncMode::Host)
    {
        const bool autoStart = _sequencer.getStartMode()
                               == AcidSequencer303::StartMode::TransportStart;
        if (hostIsPlaying && ! _wasHostPlaying && autoStart)
            _sequencer.start();
        else if (! hostIsPlaying && _wasHostPlaying)
        {
            _sequencer.stop();
            _heldNote         = -1;
            _lastStepHadSlide = false;
        }
    }
    _wasHostPlaying = hostIsPlaying;

    // ── Tick sequencer — populates _pendingNotes[] via the callback ───────────
    _pendingCount = 0;

    // Apply any sequencer command queued from the UI thread (play/stop/
    // generate/clear).  Running here — after the pending queue was reset and
    // before the clock tick — keeps every note event the sequencer emits
    // (including the note-alls-off flush on generate/clear) on the audio
    // thread and inside this buffer's event queue.
    applySeqCommands();

    _sequencer.processBlock (midiMessages,
                             numSamples,
                             hostIsPlaying,
                             ppqAtStart,
                             hostBpm,
                             /*shufflePulses=*/ 0);

    // ── Sort pending sequencer events by sample position ──────────────────────
    // Typically 0–2 events per buffer; insertion sort is ideal here.
    for (int i = 1; i < _pendingCount; ++i)
    {
        PendingNote key = _pendingNotes[i];
        int j = i - 1;
        while (j >= 0 && _pendingNotes[j].sampleOffset > key.sampleOffset)
        {
            _pendingNotes[j + 1] = _pendingNotes[j];
            --j;
        }
        _pendingNotes[j + 1] = key;
    }

    // ── dispatchSeqNote ───────────────────────────────────────────────────────
    //
    // Bridges sequencer note events to Open303.
    //
    // TIE (rest+tie steps):
    //   The sequencer never emits a NoteOn for a tied rest — it just extends
    //   the gate countdown of the previous note.  Ties are fully transparent
    //   here; no special handling is needed.
    //
    // SLIDE:
    //   The slide flag lives on the step that *sends* the glide, not the one
    //   that receives it.  We save it in _lastStepHadSlide after each NoteOn
    //   so the next NoteOn can pass slide=1 to Open303, which performs the
    //   portamento without retriggering the envelope.
    //
    // WRAP-AROUND TIE (same pitch re-firing at pattern boundary):
    //   If the last step(s) of a pattern are rest+tie, the sequencer extends
    //   the gate past the pattern end.  When step 0 then fires with the same
    //   pitch still ringing, we skip the re-trigger so there is no click.
    //   _lastStepHadSlide is preserved so the glide carry still works.
    //
    // NoteOff:
    //   Always forwarded unconditionally.  This is the key fix — suppressing
    //   NoteOffs to handle slide caused stuck notes because it prevented the
    //   gate from closing.  Open303 handles note-stealing internally; we just
    //   need to close every gate we opened.
    //
    // MIDI output buffer — built up during rendering, replaces midiMessages
    // at the end so the DAW receives sample-accurate note events on the
    // plugin's MIDI output port.
    juce::MidiBuffer midiOut;

    auto dispatchSeqNote = [&] (const PendingNote& ev)
    {
        const int samplePos = juce::jlimit (0, numSamples - 1, ev.sampleOffset);

        if (ev.type == Acid303EventType::NoteOn)
        {
            if (_sequencerMuted.load(std::memory_order_acquire))
                return;

            if (_heldNote >= 0 && ev.note == static_cast<uint8_t>(_heldNote))
            {
                // Wrap-around tie: same pitch already ringing from a gate
                // extension across the pattern boundary — do not re-trigger.
                // Keep _lastStepHadSlide as-is so slide carry is unaffected.
            }
            else
            {
                // Normal new note, or slide into a different pitch.
                // _lastStepHadSlide was set by the previous NoteOn dispatch.
                const int slide = _lastStepHadSlide ? 1 : 0;
                open303Core.noteOn (ev.note, ev.velocity, slide);
                _heldNote = static_cast<int>(ev.note);

                // MIDI out: NoteOn
                midiOut.addEvent (juce::MidiMessage::noteOn  (1, ev.note, static_cast<uint8_t>(ev.velocity)),
                                  samplePos);
            }

            // Save the slide flag of the step we just played so the *next*
            // NoteOn dispatch knows whether to use slide=1.
            _lastStepHadSlide = _sequencer.slideOn (_sequencer.getCurrentStep());
        }
        else // NoteOff — always forward, never drop
        {
            open303Core.noteOn (ev.note, 0, 0);

            // MIDI out: NoteOff
            midiOut.addEvent (juce::MidiMessage::noteOff (1, ev.note),
                              samplePos);

            if (ev.note == static_cast<uint8_t>(_heldNote))
            {
                _heldNote         = -1;
                _lastStepHadSlide = false;
            }
        }
    };

    // ── Interleaved render: sequencer events + external MIDI ──────────────────
    // Walk sequencer events and incoming MIDI together in time order.
    // Render an audio sub-block before each event, then dispatch to Open303.
    // External MIDI note messages are forwarded only when the sequencer is
    // not running, so keyboard play still works when the sequencer is off.

    int seqIdx = 0;

    for (const auto midiMetadata : midiMessages)
    {
        const auto message        = midiMetadata.getMessage();
        const auto samplePosition = midiMetadata.samplePosition;

        if (samplePosition < currentSample || samplePosition >= numSamples)
            continue;

        // Flush sequencer events that fall before this MIDI message
        while (seqIdx < _pendingCount
               && _pendingNotes[seqIdx].sampleOffset <= samplePosition)
        {
            const PendingNote& ev = _pendingNotes[seqIdx];
            const int evPos = juce::jlimit (0, numSamples - 1, ev.sampleOffset);

            if (evPos > currentSample)
            {
                render303 (buffer, currentSample, evPos);
                currentSample = evPos;
            }

            dispatchSeqNote (ev);
            ++seqIdx;
        }

        // Render audio up to this external MIDI message
        if (samplePosition > currentSample)
        {
            render303 (buffer, currentSample, samplePosition);
            currentSample = samplePosition;
        }

        // ── Rec mode (works whether or not the sequencer is playing) ─────────
        // Reset per-press state each time rec is (re)enabled so a stale held
        // note or sustain latch from a previous session can't leak in.
        const bool recOnNow = _sequencer.isRecOn();
        if (recOnNow && ! _recWasOn)
        {
            _recHeldNote   = -1;
            _sustainArmed  = true;
        }
        _recWasOn = recOnNow;

        if (recOnNow)
        {
            if (message.isNoteOn())
            {
                // A second note struck while the previous one is still held
                // records with slide (legato) — the record step is set below
                // _recHeldNote, i.e. the newly pressed note carries the slide.
                const bool legato = (_recHeldNote >= 0
                                     && _recHeldNote != message.getNoteNumber());
                _sequencer.recNote (static_cast<uint8_t> (message.getNoteNumber()),
                                    message.getVelocity() >= SEQ303_ACCENT_VELOCITY_THRESHOLD,
                                    legato);
                _recHeldNote = message.getNoteNumber();
            }
            else if (message.isNoteOff() && message.getNoteNumber() == _recHeldNote)
            {
                _recHeldNote = -1;
            }
            else if (message.isController() && message.getControllerNumber() == 64)
            {
                // Sustain pedal: a 127 tap records one rest, then waits for a
                // 0 to re-arm before the next 127 can record another rest.
                if (message.getControllerValue() == 127 && _sustainArmed)
                {
                    _sequencer.recRest();
                    _sustainArmed = false;
                }
                else if (message.getControllerValue() == 0)
                {
                    _sustainArmed = true;
                }
            }
        }

        // External MIDI note handling (sequencer not running only)
        if (! _sequencer.isRunning())
        {
            if (message.isNoteOn())
            {
                open303Core.noteOn (message.getNoteNumber(), message.getVelocity(), 0);
            }
            else if (message.isNoteOff())
            {
                open303Core.noteOn (message.getNoteNumber(), 0, 0);
            }
            else if (message.isAllNotesOff())
            {
                for (int i = 0; i <= 127; i++)
                    open303Core.noteOn (i, 0, 0);
            }
        }
    }

    // Flush any remaining sequencer events after the last MIDI message
    while (seqIdx < _pendingCount)
    {
        const PendingNote& ev = _pendingNotes[seqIdx];
        const int evPos = juce::jlimit (0, numSamples - 1, ev.sampleOffset);

        if (evPos > currentSample)
        {
            render303 (buffer, currentSample, evPos);
            currentSample = evPos;
        }

        dispatchSeqNote (ev);
        ++seqIdx;
    }

    // Render remaining samples
    render303 (buffer, currentSample, numSamples);

    // Replace the incoming MIDI buffer with the sequencer's output so the
    // DAW receives sample-accurate NoteOn/NoteOff on the plugin's MIDI output.
    // Incoming transport/clock messages are intentionally not forwarded.
    midiMessages.swapWith (midiOut);
}

void JC303::updateModSlot(int index)
{
    // UI slot 2 (index 0) is hardwired to the envelope source and has no source parameter
    open303Core.setModSlot(index + 1,
                           index == 0 ? rosic::MOD_SRC_ENV : (int) *modSlotSource[index],
                           (int) *modSlotDest[index],
                           *modSlotAmount[index]);
}

void JC303::processBlock (juce::AudioBuffer<float>& buffer,
                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();

    // Apply any pending parameter changes here on the audio thread, before rendering, so open303's
    // coefficients are never rewritten concurrently with getSample() reading them.
    if (parametersNeedUpdate.exchange(false))
        updateOpen303Parameters();

    // the LFO moves the overdrive dry/wet mix (both insert points) once per block
    if (*switchOverdriveState) {
        const float wet = juce::jlimit(0.0f, 1.0f, *overdriveDryWet + (float) open303Core.getLfoOverdriveMod());
        overdriveMix.setWetMixProportion(wet);
        preVcaOverdrive.mix.setWetMixProportion(wet);
    }

    // clear buffer
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    // sequencer tick + MIDI handling + sample-accurate audio render
    renderMidi (buffer, midiMessages);

    // GuitarML overdrive (pre-VCA mode runs inside render303)
    if (*switchOverdriveState && ! *overdrivePreVca) {
        overdriveDisperser.process(buffer.getWritePointer(0), numSamples);
        overdriveMix.pushDrySamples(buffer);
        guitarML.processAudioBlock(buffer);
        overdriveMix.mixWetSamples(buffer);
    }

    // copy mono channel to stereo
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom(ch, 0, buffer, 0, 0, numSamples);
}

int JC303::loadOverdriveTones()
{
    setupDataDirectories();
    if (userAppDataDirectory_tones.isDirectory())
    {
        juce::Array<juce::File> results;
        juce::Array<juce::File> modelFileList;
        juce::StringArray modelListNames;
        userAppDataDirectory_tones.findChildFiles(results, juce::File::findFiles, true, "*.json");
        for (int i = 0; i < results.size(); i++) {
            modelFileList.add(results.getReference(i).getFullPathName());
            juce::String fileName = modelFileList[i].getFileNameWithoutExtension();
            fileName = fileName.replace("_", " ");
            modelListNames.add(fileName);
        }
        guitarML.setModelList(modelFileList, modelListNames);
        preVcaOverdrive.amp.setModelList(modelFileList, modelListNames);
        return guitarML.getModelListSize();
    }
    return 1; // to avoid -1 at overdrive indexes startup
}

void JC303::setupDataDirectories()
{
    File userAppDataTempFile = userAppDataDirectory.getChildFile("tmp.pdl");
    File userAppDataTempFile_tones = userAppDataDirectory_tones.getChildFile("tmp.pdl");

    if (!userAppDataDirectory.exists()) {
        userAppDataTempFile.create();
    }
    if (userAppDataTempFile.existsAsFile()) {
        userAppDataTempFile.deleteFile();
    }

    if (!userAppDataDirectory_tones.exists()) {
        userAppDataTempFile_tones.create();
    }
    if (userAppDataTempFile_tones.existsAsFile()) {
        userAppDataTempFile_tones.deleteFile();
    }
}

void JC303::installTones()
{
    // (see original for implementation when binary data is available)
}

//==============================================================================
bool JC303::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* JC303::createEditor()
{
    return new JC303Editor (*this, parameters);
}

//==============================================================================
// The pattern is serialised as its own XML child; keep it out of the APVTS tree
// so replaceState/copyState round trips don't stack stale copies.
static void stripSequencerState (juce::ValueTree& state)
{
    for (auto child = state.getChildWithName ("AcidSeq303"); child.isValid(); child = state.getChildWithName ("AcidSeq303"))
        state.removeChild (child, nullptr);
}

void JC303::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    stripSequencerState (state);
    auto xml = state.createXml();

    // ── Persist sequencer state as a child element ────────────────────────────
    auto seqXml = std::make_unique<juce::XmlElement> ("AcidSeq303");

    // Snapshot the pattern under _dataLock: the audio thread can be writing
    // these fields right now (MIDI rec, generate, live UI edits) while the host
    // saves state — Ardour snapshots on every parameter change.
    TrackData303 td;
    _sequencer.copyTrackData (td);

    seqXml->setAttribute ("stepLength",  td.stepLength);
    seqXml->setAttribute ("transpose",   td.transpose);
    //seqXml->setAttribute ("tune",        _sequencer.getTune());
    //seqXml->setAttribute ("temperament", _sequencer.getTemperamentId());

    for (int i = 0; i < SEQ303_STEP_MAX; ++i)
    {
        auto stepXml = std::make_unique<juce::XmlElement> ("Step");
        stepXml->setAttribute ("i",      i);
        stepXml->setAttribute ("note",   td.step[i].note);
        stepXml->setAttribute ("rest",   td.step[i].rest   ? 1 : 0);
        stepXml->setAttribute ("accent", td.step[i].accent ? 1 : 0);
        stepXml->setAttribute ("slide",  td.step[i].slide  ? 1 : 0);
        stepXml->setAttribute ("tie",    td.step[i].tie    ? 1 : 0);
        seqXml->addChildElement (stepXml.release());
    }

    xml->addChildElement (seqXml.release());
    copyXmlToBinary (*xml, destData);
}

void JC303::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName (parameters.state.getType()))
        {
            auto newState = juce::ValueTree::fromXml (*xmlState);
            stripSequencerState (newState);
            parameters.replaceState (newState);

            // ── Restore sequencer sync state ──────────────────────────────────
            // SyncMode/StartMode/Tempo live in the APVTS params only; replaceState
            // does not fire parameterChanged, so push them into the engine here.
            if (auto* sm = dynamic_cast<juce::AudioParameterChoice*>(parameters.getParameter("seqSyncMode")))
                _sequencer.setSyncMode((AcidSequencer303::SyncMode) sm->getIndex());
            if (auto* sm = dynamic_cast<juce::AudioParameterChoice*>(parameters.getParameter("seqStartMode")))
                _sequencer.setStartMode((AcidSequencer303::StartMode) sm->getIndex());
            if (auto* tp = dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter("seqTempo")))
                _sequencer.setTempo((float) tp->get());

            // ── Restore sequencer state ───────────────────────────────────────
            // Older saves accumulated one AcidSeq303 child per save; the last is the newest.
            juce::XmlElement* seqXml = nullptr;
            for (auto* child : xmlState->getChildWithTagNameIterator ("AcidSeq303"))
                seqXml = child;
            if (seqXml != nullptr)
            {
                _sequencer.setTrackLength (static_cast<uint8_t>
                                            (seqXml->getIntAttribute ("stepLength", SEQ303_STEP_MAX)));

                // Mirror the AcidSeq303 child (source of truth for length) into the
                // APVTS params so the UI, LEN knob and the saved param state agree.
                if (auto* lp = dynamic_cast<juce::AudioParameterInt*>(parameters.getParameter("seqLength")))
                    *lp = _sequencer.getTrackLength();

                _sequencer.setTranspose (static_cast<int8_t>
                                            (seqXml->getIntAttribute ("transpose", 0)));
                //_sequencer.setTune      (static_cast<uint8_t>
                //                            (seqXml->getIntAttribute ("tune",      0)));
                //_sequencer.setTemperament (static_cast<uint8_t>
                //                            (seqXml->getIntAttribute ("temperament", 1)));

                for (auto* stepXml : seqXml->getChildIterator())
                {
                    const int i = stepXml->getIntAttribute ("i", -1);
                    if (i >= 0 && i < SEQ303_STEP_MAX)
                    {
                        _sequencer.setStepData (i, static_cast<uint8_t>
                                                    (stepXml->getIntAttribute ("note",   SEQ303_DEFAULT_NOTE)));
                        _sequencer.setRest   (i, stepXml->getIntAttribute ("rest",   1) != 0);
                        _sequencer.setAccent (i, stepXml->getIntAttribute ("accent", 0) != 0);
                        _sequencer.setSlide  (i, stepXml->getIntAttribute ("slide",  0) != 0);
                        _sequencer.setTie    (i, stepXml->getIntAttribute ("tie",    0) != 0);
                    }
                }
            }
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JC303();
}
