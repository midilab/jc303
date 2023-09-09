#include "JC303.h"
#include "Gui.h"

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
            std::make_unique<juce::AudioParameterFloat> ("waveform", // parameterID
                                                        "Waveform",  // parameter name
                                                        0.0f,        // minimum value
                                                        1.0f,        // maximum value
                                                        //juce::NormalisableRange<float> (0.0f, 1.0f), // parameter range
                                                        1.0f),       // default value
            std::make_unique<juce::AudioParameterFloat> ("tuning",
                                                        "Tuning",
                                                        0.0f,
                                                        1.0f,
                                                        0.5f),
            std::make_unique<juce::AudioParameterFloat> ("cutoff", // parameterID
                                                        "Cutoff", // parameter name
                                                        0.0f,   // minimum value
                                                        1.0f,   // maximum value
                                                        //juce::NormalisableRange<float> (0.0f, 1.0f), // parameter range
                                                        0.0f),       // default value
            std::make_unique<juce::AudioParameterFloat> ("resonance", // parameterID
                                                        "Resonance", // parameter name
                                                        0.0f,   // minimum value
                                                        1.0f,   // maximum value
                                                        //juce::NormalisableRange<float> (0.0f, 1.0f), // parameter range
                                                        0.0f),       // default value
            std::make_unique<juce::AudioParameterFloat> ("envmod", // parameterID
                                                        "EnvMod", // parameter name
                                                        0.0f,   // minimum value
                                                        1.0f,   // maximum value
                                                        //juce::NormalisableRange<float> (0.0f, 1.0f), // parameter range
                                                        0.0f),       // default value
            std::make_unique<juce::AudioParameterFloat> ("decay", // parameterID
                                                        "Decay", // parameter name
                                                        0.0f,   // minimum value
                                                        1.0f,   // maximum value
                                                        //juce::NormalisableRange<float> (0.0f, 1.0f), // parameter range
                                                        0.85f),       // default value
            std::make_unique<juce::AudioParameterFloat> ("accent", // parameterID
                                                        "Accent", // parameter name
                                                        0.0f,   // minimum value
                                                        1.0f,   // maximum value
                                                        //juce::NormalisableRange<float> (0.0f, 1.0f), // parameter range
                                                        0.5f),       // default value
            std::make_unique<juce::AudioParameterFloat> ("volume", // parameterID
                                                        "Volume", // parameter name
                                                        0.0f,   // minimum value
                                                        1.0f,   // maximum value
                                                        //juce::NormalisableRange<float> (0.0f, 1.0f), // parameter range
                                                        0.85f)        // default value

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
}

JC303::~JC303()
{
}


void JC303::setParameter (Open303Parameters index, float value)
{
  if( index < 0 || index >= OPEN303_NUM_PARAMETERS )
    return;

	switch(index)
	{
    case WAVEFORM:
        open303Core.setWaveform( linToLin(value, 0.0, 1.0,   0.0,      1.0) );
        break;
    case TUNING:
        open303Core.setTuning(   linToLin(value, 0.0, 1.0,  400.0,    480.0) );
        break;
    case CUTOFF:
        open303Core.setCutoff(   linToExp(value, 0.0, 1.0, 314.0,    2394.0) );
        break;
    case RESONANCE:
        open303Core.setResonance(linToLin(value, 0.0, 1.0,   0.0,    100.0) );
        break;
    case ENVMOD:
        open303Core.setEnvMod(   linToLin(value, 0.0, 1.0,    0.0,   100.0)  );
        break;
    case DECAY:
        open303Core.setDecay(    linToExp(value, 0.0, 1.0,  200.0,  2000.0) );
        break;
    case ACCENT:
        open303Core.setAccent(   linToLin(value, 0.0, 1.0,   0.0,    100.0) );
        break;
    case VOLUME:
        open303Core.setVolume(   linToLin(value, 0.0, 1.0, -60.0,      0.0)  );
        break;
    //case FILTER_TYPE:
    //    open303Core.filter.setMode(  normalizedValueToIndex(value, TeeBeeFilter::NUM_MODES) );
    //    break;

    #ifdef SHOW_INTERNAL_PARAMETERS
    case AMP_SUSTAIN:
        open303Core.setAmpSustain(        linToLin(value, 0.0, 1.0, -60.0,      0.0)  );
        break;
    case TANH_SHAPER_DRIVE:
        open303Core.setTanhShaperDrive(   linToLin(value, 0.0, 1.0,   0.0,     60.0)  );
        break;
    case TANH_SHAPER_OFFSET:
        open303Core.setTanhShaperOffset(  linToLin(value, 0.0, 1.0, -10.0,     10.0)  );
        break;
    case PRE_FILTER_HPF:
        open303Core.setPreFilterHighpass( linToExp(value, 0.0, 1.0,  10.0,    500.0)  );
        break;
    case FEEDBACK_HPF:
        open303Core.setFeedbackHighpass(  linToExp(value, 0.0, 1.0,  10.0,    500.0)  );
        break;
    case POST_FILTER_HPF:
        open303Core.setPostFilterHighpass(linToExp(value, 0.0, 1.0,  10.0,    500.0)  );
        break;
    case SQUARE_PHASE_SHIFT:
        open303Core.setSquarePhaseShift(  linToLin(value, 0.0, 1.0,   0.0,    360.0)  );
        break;
    #endif

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

    /*
    // for debugging only
    open303Core.setCutoff(3.138152786059267e+002);
    open303Core.setEnvMod(0.0);
    open303Core.setEnvMod(100.0);
    open303Core.setCutoff(2.394411986817546e+003);
    open303Core.setEnvMod(0.0);
    */
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

void JC303::render(juce::AudioBuffer<float>& buffer, int beginSample, int endSample)
{
    auto* firstChannel = buffer.getWritePointer(0);
    for (auto sample = beginSample; sample < endSample; ++sample)
    {
        firstChannel[sample] += (float) open303Core.getSample();
    }

    for (int channel = 1; channel < buffer.getNumChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        std::copy(firstChannel + beginSample, 
            firstChannel + endSample, 
            channelData + beginSample);
    }
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

    // update parameters
    setParameter(WAVEFORM, *waveForm);
    setParameter(TUNING, *tuning);
    setParameter(CUTOFF, *cutoffFreq);
    setParameter(RESONANCE, *resonance);
    setParameter(ENVMOD, *envelopMod);
    setParameter(DECAY, *decay);
    setParameter(ACCENT, *accent);
    setParameter(VOLUME, *volume);

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

        render(buffer, currentSample, messagePosition);
        currentSample = messagePosition;
    }
    
    // render the audio output
    render(buffer, currentSample, buffer.getNumSamples());
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
