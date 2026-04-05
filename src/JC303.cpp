#include "JC303.h"
#include GUI_THEME_HEADER

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
                                                        0.0f,
                                                        1.0f,
                                                        0.3f),
            std::make_unique<juce::AudioParameterFloat> ("accentDecay",
                                                        "Accent Decay",
                                                        0.0f,
                                                        1.0f,
                                                        0.03f),
            std::make_unique<juce::AudioParameterFloat> ("feedbackFilter",
                                                        "Filt. FeedBack",
                                                        0.0f,
                                                        1.0f,
                                                        0.63f),
            std::make_unique<juce::AudioParameterFloat> ("softAttack",
                                                        "Soft Attack",
                                                        0.0f,
                                                        1.0f,
                                                        0.26f),
            std::make_unique<juce::AudioParameterFloat> ("slideTime",
                                                        "Slide time",
                                                        0.0f,
                                                        1.0f,
                                                        0.33f),
            std::make_unique<juce::AudioParameterFloat> ("sqrDriver",
                                                        "Square Driver",
                                                        0.0f,
                                                        1.0f,
                                                        0.25f),
            std::make_unique<juce::AudioParameterBool> ("switchModState",
                                                        "Switch Mod",
                                                        false),
            // LFO parameters
            std::make_unique<juce::AudioParameterInt> ("lfoWaveform",
                                                        "LFO Wave",
                                                        0,
                                                        5,
                                                        0),
            std::make_unique<juce::AudioParameterFloat> ("lfoRate",
                                                        "LFO Rate",
                                                        0.0f,
                                                        1.0f,
                                                        0.25f),
            std::make_unique<juce::AudioParameterFloat> ("lfoDepth",
                                                        "LFO Depth",
                                                        0.0f,
                                                        1.0f,
                                                        0.0f),
            std::make_unique<juce::AudioParameterInt> ("lfoDestination",
                                                        "LFO Destination",
                                                        0,
                                                        2,
                                                        0),
            // overdrive
            std::make_unique<juce::AudioParameterInt> ("overdriveModelIndex",
                                                        "Overdrive Model Index",
                                                        0,
                                                        loadOverdriveTones() - 1,
                                                        0),
            std::make_unique<juce::AudioParameterFloat> ("overdriveLevel",
                                                        "Drive",
                                                        0.0f,
                                                        1.0f,
                                                        0.25f),
            std::make_unique<juce::AudioParameterFloat> ("overdriveDryWet",
                                                        "Dry/Wet",
                                                        0.0f,
                                                        1.0f,
                                                        0.25f),
             std::make_unique<juce::AudioParameterBool> ("switchOverdriveState",
                                                        "Switch Overdrive Mod",
                                                        false),
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
                                                        3.0f),
             std::make_unique<juce::AudioParameterFloat> ("lowerNote",
                                                        "Lower Note",
                                                        0.0f,
                                                        127.0f,
                                                        30.0f),
             std::make_unique<juce::AudioParameterFloat> ("rangeNote",
                                                        "Range Note",
                                                        0.0f,
                                                        127.0f,
                                                        48.0f),
           std::make_unique<juce::AudioParameterFloat> ("seqHarmonizer",
                                                        "Seq Harmonizer",
                                                        0.0f,
                                                        13.0f,
                                                        0.0f),
           std::make_unique<juce::AudioParameterFloat> ("seqLength",
                                                        "Seq Length",
                                                        0.0f,
                                                        16.0f,
                                                        16.0f),
           std::make_unique<juce::AudioParameterFloat> ("seqShift",
                                                        "Seq Shift",
                                                        0.0f,
                                                        16.0f,
                                                        0.0f),
             std::make_unique<juce::AudioParameterBool> ("seqPlayState",
                                                         "Seq Play State",
                                                         false),
             std::make_unique<juce::AudioParameterBool> ("seqGenerate",
                                                          "Seq Generate",
                                                          false)
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
    feedbackFilter = parameters.getRawParameterValue("feedbackFilter");
    softAttack = parameters.getRawParameterValue("softAttack");
    slideTime = parameters.getRawParameterValue("slideTime");
    sqrDriver = parameters.getRawParameterValue("sqrDriver");
    // LFO parameters
    lfoWaveform = parameters.getRawParameterValue("lfoWaveform");
    lfoRate = parameters.getRawParameterValue("lfoRate");
    lfoDepth = parameters.getRawParameterValue("lfoDepth");
    lfoDestination = parameters.getRawParameterValue("lfoDestination");
    // overdrive parameters
    overdriveModelIndex = parameters.getRawParameterValue("overdriveModelIndex");
    switchOverdriveState = parameters.getRawParameterValue("switchOverdriveState");
    overdriveLevel = parameters.getRawParameterValue("overdriveLevel");
    overdriveDryWet = parameters.getRawParameterValue("overdriveDryWet");
    // generative sequencer parameters
    seqGenerativeFill = parameters.getRawParameterValue("seqGenerativeFill");
    seqGenerativeAccentProbability = parameters.getRawParameterValue("seqGenerativeAccentProbability");
    seqGenerativeSlideProbability = parameters.getRawParameterValue("seqGenerativeSlideProbability");
    seqGenerativeTieProbability = parameters.getRawParameterValue("seqGenerativeTieProbability");
    numberOfTones = parameters.getRawParameterValue("numberOfTones");
    lowerNote = parameters.getRawParameterValue("lowerNote");
    rangeNote = parameters.getRawParameterValue("rangeNote");
    seqHarmonizer = parameters.getRawParameterValue("seqHarmonizer");
    seqLength = parameters.getRawParameterValue("seqLength");
    seqShift = parameters.getRawParameterValue("seqShift");
    seqPlayState = parameters.getRawParameterValue("seqPlayState");
    seqGenerate = parameters.getRawParameterValue("seqGenerate");

    // force initial user values(some hosts migth not do it using value tree state)
    setParameter(WAVEFORM, *waveForm);
    setParameter(TUNING, *tuning);
    setParameter(CUTOFF, *cutoffFreq);
    setParameter(RESONANCE, *resonance);
    setParameter(ENVMOD, *envelopMod);
    setParameter(DECAY, *decay);
    setParameter(ACCENT, *accent);
    setParameter(VOLUME, *volume);
    setDevilMod(*switchModState);
    setParameter(NORMAL_DECAY, *normalDecay);
    setParameter(ACCENT_DECAY, *accentDecay);
    setParameter(FEEDBACK_HPF, *feedbackFilter);
    setParameter(SOFT_ATTACK, *softAttack);
    setParameter(SLIDE_TIME, *slideTime);
    setParameter(TANH_SHAPER_DRIVE, *sqrDriver);
    // LFO parameters
    setParameter(LFO_WAVEFORM, *lfoWaveform);
    setParameter(LFO_RATE, *lfoRate);
    setParameter(LFO_DEPTH, *lfoDepth);
    setParameter(LFO_DESTINATION, *lfoDestination);
    // overdrive parameters
    setParameter(OVERDRIVE_LEVEL, *overdriveLevel);
    setParameter(OVERDRIVE_DRY_WET, *overdriveDryWet);
    setParameter(OVERDRIVE_MODEL_INDEX, *overdriveModelIndex);

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
    // overdrive parameter listeners
    parameters.addParameterListener("overdriveLevel", this);
    parameters.addParameterListener("overdriveDryWet", this);
    parameters.addParameterListener("overdriveModelIndex", this);
    parameters.addParameterListener("switchOverdriveState", this);
    // generative sequencer parameter listener
    parameters.addParameterListener("seqPlayState", this);
    parameters.addParameterListener("seqGenerate", this);
    parameters.addParameterListener("seqHarmonizer", this);
    parameters.addParameterListener("seqLength", this);
    parameters.addParameterListener("seqShift", this);

    // ── Sequencer callback ────────────────────────────────────────────────────
    // Runs on the audio thread. Stores each event into _pendingNotes[] so
    // processBlock can deliver it to Open303 at the correct sample offset
    // inside the existing render loop.
    _sequencer.onNoteEvent = [this] (const Acid303Event& ev)
    {
        if (_pendingCount < kPendingMax)
            _pendingNotes[_pendingCount++] = { ev.type, ev.note, ev.velocity, ev.sampleOffset };
    };

    // Just for tests... remove it later
    _sequencer.setSyncMode  (AcidSequencer303::SyncMode::Internal);
    _sequencer.setStartMode (AcidSequencer303::StartMode::TransportStart);
    _sequencer.setTempo     (120.0f);
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
    // overdrive parameter listeners
    parameters.removeParameterListener("overdriveLevel", this);
    parameters.removeParameterListener("overdriveDryWet", this);
    parameters.removeParameterListener("overdriveModelIndex", this);
    parameters.removeParameterListener("switchOverdriveState", this);
    // generative sequencer
    parameters.removeParameterListener("seqPlayState", this);
    parameters.removeParameterListener("seqGenerate", this);
    parameters.removeParameterListener("seqHarmonizer", this);
    parameters.removeParameterListener("seqLength", this);
    parameters.removeParameterListener("seqShift", this);
}

// Parameter change callback
void JC303::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Map parameter ID to enum and update immediately or set flag
    if (parameterID == "waveform") {
        setParameter(WAVEFORM, newValue);
    }
    else if (parameterID == "tuning") {
        setParameter(TUNING, newValue);
    }
    else if (parameterID == "cutoff") {
        setParameter(CUTOFF, newValue);
    }
    else if (parameterID == "resonance") {
        setParameter(RESONANCE, newValue);
    }
    else if (parameterID == "envmod") {
        setParameter(ENVMOD, newValue);
    }
    else if (parameterID == "decay") {
        setParameter(DECAY, newValue);
    }
    else if (parameterID == "accent") {
        setParameter(ACCENT, newValue);
    }
    else if (parameterID == "volume") {
        setParameter(VOLUME, newValue);
    }
    else if (parameterID == "switchModState") {
        setDevilMod(newValue > 0.5f);
    }
    else if (parameterID == "normalDecay" && *switchModState) {
        setParameter(NORMAL_DECAY, newValue);
    }
    else if (parameterID == "accentDecay" && *switchModState) {
        setParameter(ACCENT_DECAY, newValue);
    }
    else if (parameterID == "feedbackFilter" && *switchModState) {
        setParameter(FEEDBACK_HPF, newValue);
    }
    else if (parameterID == "softAttack" && *switchModState) {
        setParameter(SOFT_ATTACK, newValue);
    }
    else if (parameterID == "slideTime" && *switchModState) {
        setParameter(SLIDE_TIME, newValue);
    }
    else if (parameterID == "sqrDriver" && *switchModState) {
        setParameter(TANH_SHAPER_DRIVE, newValue);
    }
    // LFO parameters
    else if (parameterID == "lfoWaveform") {
        setParameter(LFO_WAVEFORM, newValue);
    }
    else if (parameterID == "lfoRate") {
        setParameter(LFO_RATE, newValue);
    }
    else if (parameterID == "lfoDepth") {
        setParameter(LFO_DEPTH, newValue);
    }
    else if (parameterID == "lfoDestination") {
        setParameter(LFO_DESTINATION, newValue);
    }
    // overdrive parameter
    else if (parameterID == "overdriveLevel") {
        setParameter(OVERDRIVE_LEVEL, newValue);
    }
    else if (parameterID == "overdriveDryWet") {
        setParameter(OVERDRIVE_DRY_WET, newValue);
    }
    else if (parameterID == "overdriveModelIndex") {
        setParameter(OVERDRIVE_MODEL_INDEX, newValue);
    }
    else if (parameterID == "seqPlayState") {
        if (newValue > 0.5f) {
            _sequencer.start();
        } else {
            _sequencer.stop();
        }
    }
    else if (parameterID == "seqGenerate") {
        _sequencerMuted.store(true, std::memory_order_release);

        if (_heldNote >= 0) {
            open303Core.noteOn(_heldNote, 0, 0);
            _heldNote = -1;
            _lastStepHadSlide = false;
        }

        _sequencer.acidRandomize(
            static_cast<uint8_t>(*seqGenerativeFill),
            static_cast<uint8_t>(*seqGenerativeAccentProbability),
            static_cast<uint8_t>(*seqGenerativeSlideProbability),
            static_cast<uint8_t>(*seqGenerativeTieProbability),
            static_cast<uint8_t>(*numberOfTones),
            static_cast<uint8_t>(*lowerNote),
            static_cast<uint8_t>(*rangeNote)
        );

        _sequencerMuted.store(false, std::memory_order_release);
    }
    else if (parameterID == "seqHarmonizer") {
        uint8_t seqHarmony = static_cast<uint8_t>(*seqHarmonizer);
        if (seqHarmony == 0) {
            _sequencer.setTune(0);
        } else {
            _sequencer.setTemperament(seqHarmony - 1);
            _sequencer.setTune(1);
        }
    }
    else if (parameterID == "seqLength") {
        _sequencer.setTrackLength(static_cast<uint8_t>(*seqLength));
    }
    else if (parameterID == "seqShift") {
        _sequencer.setShiftPos(static_cast<int8_t>(*seqShift));
    }
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
            linToExp(value, 0.0, 1.0, 314.0,    2394.0)
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
        break;
    case OVERDRIVE_DRY_WET:
        overdriveMix.setWetMixProportion(value);
        break;
    case OVERDRIVE_MODEL_INDEX:
        // load new model
        //guitarML.loadModel(value);
        guitarML.loadUserModel(value);
        break;

    //
    // MODS (mostly based on devilfish mod)
    // BUT DONT! dont expect a devilfish clone sound or mail me about!
    // https://www.firstpr.com.au/rwi/dfish/Devil-Fish-Manual.pdf
    //
    case NORMAL_DECAY:
        open303Core.setAmpDecay(
            linToLin(value, 0.0, 1.0, 30.0,      3000.0)
        );
        break;
    case ACCENT_DECAY:
        open303Core.setAccentDecay(
            linToLin(value, 0.0, 1.0, 30.0,      3000.0)
        );
        break;
    case FEEDBACK_HPF:
        open303Core.setFeedbackHighpass(
            linToExp(value, 0.0, 1.0,  350.0,    100.0)
        );
        break;
    case SOFT_ATTACK:
        open303Core.setNormalAttack(
            linToExp(value, 0.0, 1.0,  0.3,    3000.0)
        );
        break;
    case SLIDE_TIME:
        open303Core.setSlideTime(
            linToLin(value, 0.0, 1.0, 2.0, 360.0)
        );
        break;
    case TANH_SHAPER_DRIVE:
        open303Core.setTanhShaperDrive(
            linToLin(value, 0.0, 1.0,   25.0,     80.0)
        );
        break;

    // LFO parameters
    case LFO_WAVEFORM:
        open303Core.setLfoWaveform(static_cast<int>(value));
        break;
    case LFO_RATE:
        open303Core.setLfoRate(
            linToExp(value, 0.0, 1.0, 0.1, 20.0)
        );
        break;
    case LFO_DEPTH:
        open303Core.setLfoDepth(value);
        break;
    case LFO_DESTINATION:
        open303Core.setLfoDestination(static_cast<int>(value));
        break;
	}
}

// toogle/restore 303 original and mod modes
void JC303::setDevilMod(bool mode)
{
    if (mode == true) {
        decayMin = 30.0;
        decayMax = 3000.0;
        setParameter(NORMAL_DECAY, *normalDecay);
        setParameter(ACCENT_DECAY, *accentDecay);
        setParameter(FEEDBACK_HPF, *feedbackFilter);
        setParameter(SOFT_ATTACK, *softAttack);
        setParameter(SLIDE_TIME, *slideTime);
        setParameter(TANH_SHAPER_DRIVE, *sqrDriver);
    } else if (mode == false) {
        decayMin = 200.0;
        decayMax = 2000.0;
        open303Core.setAmpDecay(1230.0);
        open303Core.setAccentDecay(200.0);
        open303Core.setFeedbackHighpass(150.0);
        open303Core.setNormalAttack(3.0);
        open303Core.setSlideTime(60.0);
        open303Core.setTanhShaperDrive(36.9);
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

    // ── Sequencer ─────────────────────────────────────────────────────────────
    // 960 PPQN is the JUCE default and is compatible with all major DAWs.
    // The sequencer adapts all pulse maths to this resolution.
    _sequencer.prepare (sampleRate, 960.0);
    _pendingCount     = 0;
    _wasHostPlaying   = false;
    _heldNote         = -1;
    _lastStepHadSlide = false;
    _heldNote       = -1;
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
    for (auto sample = beginSample; sample < endSample; ++sample)
        monoChannel[sample] = (float) open303Core.getSample();
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
    if (_sequencer.getSyncMode() == AcidSequencer303::SyncMode::Host)
    {
        if (hostIsPlaying && ! _wasHostPlaying)
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

void JC303::processBlock (juce::AudioBuffer<float>& buffer,
                          juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const auto numSamples = buffer.getNumSamples();

    // clear buffer
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    // sequencer tick + MIDI handling + sample-accurate audio render
    renderMidi (buffer, midiMessages);

    // GuitarML overdrive
    if (*switchOverdriveState) {
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
void JC303::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    auto xml = state.createXml();

    // ── Persist sequencer state as a child element ────────────────────────────
    auto seqXml = std::make_unique<juce::XmlElement> ("AcidSeq303");
    seqXml->setAttribute ("syncMode",    static_cast<int>(_sequencer.getSyncMode()));
    seqXml->setAttribute ("startMode",   static_cast<int>(_sequencer.getStartMode()));
    seqXml->setAttribute ("bpm",         _sequencer.getTempo());
    seqXml->setAttribute ("stepLength",  _sequencer.getTrackLength());
    seqXml->setAttribute ("shift",       _sequencer.getShiftPos());
    seqXml->setAttribute ("transpose",   _sequencer.getTranspose());
    seqXml->setAttribute ("tune",        _sequencer.getTune());
    seqXml->setAttribute ("temperament", _sequencer.getTemperamentId());

    const TrackData303& td = _sequencer.getTrackData();
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
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));

            // ── Restore sequencer state ───────────────────────────────────────
            if (auto* seqXml = xmlState->getChildByName ("AcidSeq303"))
            {
                _sequencer.setSyncMode  (static_cast<AcidSequencer303::SyncMode>
                                            (seqXml->getIntAttribute ("syncMode",  0)));
                _sequencer.setStartMode (static_cast<AcidSequencer303::StartMode>
                                            (seqXml->getIntAttribute ("startMode", 0)));
                _sequencer.setTempo     (static_cast<float>
                                            (seqXml->getDoubleAttribute ("bpm", 120.0)));
                _sequencer.setTrackLength (static_cast<uint8_t>
                                            (seqXml->getIntAttribute ("stepLength", SEQ303_STEP_MAX)));
                _sequencer.setShiftPos  (static_cast<int8_t>
                                            (seqXml->getIntAttribute ("shift",     0)));
                _sequencer.setTranspose (static_cast<int8_t>
                                            (seqXml->getIntAttribute ("transpose", 0)));
                _sequencer.setTune      (static_cast<uint8_t>
                                            (seqXml->getIntAttribute ("tune",      0)));
                _sequencer.setTemperament (static_cast<uint8_t>
                                            (seqXml->getIntAttribute ("temperament", 1)));

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
