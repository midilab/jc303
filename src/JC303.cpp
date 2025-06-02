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
                                                        0.0f),
            std::make_unique<juce::AudioParameterFloat> ("envmod",
                                                        "EnvMod",
                                                        0.0f,
                                                        1.0f,
                                                        0.0f),
            std::make_unique<juce::AudioParameterFloat> ("decay",
                                                        "Decay",
                                                        0.0f,
                                                        1.0f,
                                                        0.85f),
            std::make_unique<juce::AudioParameterFloat> ("accent",
                                                        "Accent",
                                                        0.0f,
                                                        1.0f,
                                                        0.5f),
            std::make_unique<juce::AudioParameterFloat> ("volume",
                                                        "Volume",
                                                        0.0f,
                                                        1.0f,
                                                        0.85f),
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
                                                        0.1f),
            std::make_unique<juce::AudioParameterFloat> ("feedbackFilter",
                                                        "Filt. FeedBack",
                                                        0.0f,
                                                        1.0f,
                                                        0.54f),
            std::make_unique<juce::AudioParameterFloat> ("softAttack",
                                                        "Soft Attack",
                                                        0.0f,
                                                        1.0f,
                                                        0.1f),
            std::make_unique<juce::AudioParameterFloat> ("slideTime",
                                                        "Slide time",
                                                        0.0f,
                                                        1.0f,
                                                        0.55f),
            std::make_unique<juce::AudioParameterFloat> ("sqrDriver",
                                                        "Square Driver",
                                                        0.0f,
                                                        1.0f,
                                                        0.25f),
            std::make_unique<juce::AudioParameterBool> ("switchModState",
                                                        "Switch Mod",
                                                        false),
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
                                                        0.5f),
            std::make_unique<juce::AudioParameterBool> ("switchOverdriveState",
                                                        "Switch Overdrive Mod",
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
    // overdrive parameters
    overdriveModelIndex = parameters.getRawParameterValue("overdriveModelIndex");
    switchOverdriveState = parameters.getRawParameterValue("switchOverdriveState");
    overdriveLevel = parameters.getRawParameterValue("overdriveLevel");
    overdriveDryWet = parameters.getRawParameterValue("overdriveDryWet");

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
    setParameter(OVERDRIVE_LEVEL, *overdriveLevel);
    setParameter(OVERDRIVE_DRY_WET, *overdriveDryWet);
    setParameter(OVERDRIVE_MODEL_INDEX, *overdriveModelIndex);

    // presets and overdrive models
    setupDataDirectories();
    //installTones();
    //loadOverdriveTones();
    // Sort jsonFiles alphabetically
    /* std::sort(jsonFiles.begin(), jsonFiles.end());
    if (jsonFiles.size() > 0) {
        loadConfig(jsonFiles[current_model_index]);
    } */

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
    parameters.addParameterListener("overdriveLevel", this);
    parameters.addParameterListener("overdriveDryWet", this);
    parameters.addParameterListener("overdriveModelIndex", this);
    parameters.addParameterListener("switchOverdriveState", this);
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
    parameters.removeParameterListener("overdriveLevel", this);
    parameters.removeParameterListener("overdriveDryWet", this);
    parameters.removeParameterListener("overdriveModelIndex", this);
    parameters.removeParameterListener("switchOverdriveState", this);
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
    else if (parameterID == "overdriveLevel") {
        setParameter(OVERDRIVE_LEVEL, newValue);
    }
    else if (parameterID == "overdriveDryWet") {
        setParameter(OVERDRIVE_DRY_WET, newValue);
    }
    else if (parameterID == "overdriveModelIndex") {
        setParameter(OVERDRIVE_MODEL_INDEX, newValue);
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
        /*
        On non-accented notes, the TB-303’s Main Envelope Generator (MEG) had a decay time
        between 200 ms and 2 seconds – as controlled by the Decay pot. On accented notes, the
        decay time was fixed to 200 ms. In the Devil Fish, there are two new pots for MEG decay –
        Normal Decay and Accent Decay. Both have a range between 30 ms and 3 seconds.
        */
        open303Core.setAmpDecay(
            linToLin(value, 0.0, 1.0, 30.0,      3000.0)
        );
        break;
    case ACCENT_DECAY:
        // setAmpDecay 16 > 3000
        open303Core.setAccentDecay(
            linToLin(value, 0.0, 1.0, 30.0,      3000.0)
        );
        break;
    case FEEDBACK_HPF:
        // this one is expresive only on higher reesonances
        open303Core.setFeedbackHighpass(
            //linToExp(value, 0.0, 1.0,  350.0,    10.0)
            linToExp(value, 0.0, 1.0,  350.0,    100.0)
        );
        break;
    case SOFT_ATTACK:
        /*
        The Soft Attack pot varies the attack time of non-accented notes between 0.3 ms and 30 ms.
        In the TB-303 there was a (typical) 4 ms delay and then a 3 ms attack time.
        */
        open303Core.setNormalAttack(
            linToExp(value, 0.0, 1.0,  0.3,    3000.0)
        );
        break;
    case SLIDE_TIME:
        /*
        The Slide Time pot. Normally the slide time is 60 ms (milliseconds). In the Devil Fish, the
        Slide Time pot varies the time from 60 to 360 ms, when running from the internal sequencer.
        When running from an external CV, the time is between 2 and 300 ms.
        */
        open303Core.setSlideTime(
            //linToLin(value, 0.0, 1.0, 0.0, 60.0)
            linToLin(value, 0.0, 1.0, 2.0, 360.0)
        );
        break;
    case TANH_SHAPER_DRIVE:
        open303Core.setTanhShaperDrive(
            //linToLin(value, 0.0, 1.0,   0.0,     60.0)
            linToLin(value, 0.0, 1.0,   25.0,     80.0)
        );
        break;
	}
}

// toogle/restore 303 original and mod modes
void JC303::setDevilMod(bool mode)
{
    if (mode == true) {
        // fixed internal tunning, mostly based on devil fish
        // setAccentAttack(3) 3ms devil vs ?? original
        ////open303Core.setAccentAttack(3.0);
        // devilfish extended decay range
        decayMin = 30.0;
        decayMax = 3000.0;
        setParameter(NORMAL_DECAY, *normalDecay);
        setParameter(ACCENT_DECAY, *accentDecay);
        setParameter(FEEDBACK_HPF, *feedbackFilter);
        setParameter(SOFT_ATTACK, *softAttack);
        setParameter(SLIDE_TIME, *slideTime);
        setParameter(TANH_SHAPER_DRIVE, *sqrDriver);
    } else if (mode == false) {
        // restore original 303 values and block devilfish mod knobs to operate
        // original tb303 decay range
        decayMin = 200.0;
        decayMax = 2000.0;
        // NORMAL_DECAY
        open303Core.setAmpDecay(1230.0); // ampEnv.setDecay(1230.0);
        // ACCENT_DECAY
        open303Core.setAccentDecay(200.0); // 200.0
        // FEEDBACK_HPF
        open303Core.setFeedbackHighpass(150.0); // filter.setFeedbackHighpassCutoff(150.0);
        // SOFT_ATTACK
        open303Core.setNormalAttack(3.0); // 3.0;
        // SLIDE_TIME
        open303Core.setSlideTime(60.0); // 60.0;
        // TANH_SHAPER_DRIVE
        open303Core.setTanhShaperDrive(36.9); // dB2amp(36.9);
        //open303Core.setAmpSustain(-6.02); // dB2amp(newSustain) = 0.5 ~ -6.0205 or -8.68589?
        //open303Core.setAmpRelease(1.0); // 1.0
        // fixed parameters restore
        ////open303Core.setAccentAttack(3.0); // 3.0?
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
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
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
    // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
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
}

void JC303::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool JC303::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
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
        // processing open303
        monoChannel[sample] = (float) open303Core.getSample();
}

void JC303::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto currentSample = 0;

    // clear buffer
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // handle midi note messages
    for (const auto midiMetadata : midiMessages)
    {
        const auto message = midiMetadata.getMessage();
        const auto messagePosition = static_cast<int>(message.getTimeStamp());

        if (message.isNoteOn())
        {
            open303Core.noteOn(message.getNoteNumber(), message.getVelocity(), 0);
        }
        else if (message.isNoteOff())
        {
            open303Core.noteOn(message.getNoteNumber(), 0, 0);
        }
        else if (message.isAllNotesOff())
        {
            for(int i=0; i <= 127; i++) {
                open303Core.noteOn(i, 0, 0);
            }
        } else {
            continue;
        }

        // render open303
        render303(buffer, currentSample, messagePosition);
        currentSample = messagePosition;
    }

    // render open303
    render303(buffer, currentSample, buffer.getNumSamples());

    // render GuitarML overdrive
    if (*switchOverdriveState) {
        // preparing dry/wet signal
        overdriveMix.pushDrySamples(buffer);
        // processing distortion: guitarML - from BYOD
        guitarML.processAudioBlock(buffer);
        // processing dry/wet signal
        overdriveMix.mixWetSamples(buffer);
    }

    // copy mono channel to stereo
    for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom(ch, 0, buffer, 0, 0, buffer.getNumSamples());
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
        // TODO: sort by 1.file name and 2.fodler name alphabetically
        for (int i = 0; i < results.size(); i++) {
            modelFileList.add(results.getReference(i).getFullPathName());
            // Get the file name without extension
            juce::String fileName = modelFileList[i].getFileNameWithoutExtension();
            // Replace underscores with spaces
            fileName = fileName.replace("_", " ");
            // Add the modified file name to modelListNames
            modelListNames.add(fileName);
        }
        guitarML.setModelList(modelFileList, modelListNames);
        return guitarML.getModelListSize();
    }
    return 1; // to avoid -1 at overdrive indexes startup
}

void JC303::setupDataDirectories()
{
    // User app data directory
    File userAppDataTempFile = userAppDataDirectory.getChildFile("tmp.pdl");

    File userAppDataTempFile_tones = userAppDataDirectory_tones.getChildFile("tmp.pdl");

    // Create (and delete) temp file if necessary, so that user doesn't have
    // to manually create directories
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
//====================================================================
// Description: Checks that the default tones
//  are installed to the NeuralPi directory, and if not,
//  copy them from the binary data in the plugin to that directory.
//
//====================================================================
{
    // Default tones
    /* File ts9_tone = userAppDataDirectory_tones.getFullPathName() + "/TS9.json";
    File bjdirty_tone = userAppDataDirectory_tones.getFullPathName() + "/BluesJR.json";
    File ht40od_tone = userAppDataDirectory_tones.getFullPathName() + "/HT40_Overdrive.json";

    if (ts9_tone.existsAsFile() == false) {
        std::string string_command = ts9_tone.getFullPathName().toStdString();
        const char* char_ts9_tone = &string_command[0];

        std::ofstream myfile;
        myfile.open(char_ts9_tone);
        myfile << BinaryDataNeuralPi::TS9_json;

        myfile.close();
    }

    if (bjdirty_tone.existsAsFile() == false) {
        std::string string_command = bjdirty_tone.getFullPathName().toStdString();
        const char* char_bjdirty = &string_command[0];

        std::ofstream myfile;
        myfile.open(char_bjdirty);
        myfile << BinaryDataNeuralPi::BluesJr_json;

        myfile.close();
    }

    if (ht40od_tone.existsAsFile() == false) {
        std::string string_command = ht40od_tone.getFullPathName().toStdString();
        const char* char_ht40od = &string_command[0];

        std::ofstream myfile;
        myfile.open(char_ht40od);
        myfile << BinaryDataNeuralPi::HT40_Overdrive_json;

        myfile.close();
    } */

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
    // for host save functionality
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void JC303::setStateInformation (const void* data, int sizeInBytes)
{
    // for host load functionality
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xmlState));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JC303();
}
