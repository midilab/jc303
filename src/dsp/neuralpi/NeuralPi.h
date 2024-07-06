/*
  ==============================================================================

    Based on NeuralPi 1.3.0 PluginProcessor.h 

  ==============================================================================
*/

#include <JuceHeader.h>
//#include <juce_dsp/juce_dsp.h> 
#include <BinaryDataNeuralPi.h>
//#include <nlohmann/json.hpp>
#include <json/json.hpp>
#include "RTNeuralLSTM.h"
//#include "AmpOSCReceiver.h"
//#include "Eq4Band.h"
//#include "CabSim.h"
//#include "Delay.h"

#pragma once

#define GAIN_ID "gain"
#define GAIN_NAME "Gain"
#define MODEL_ID "model"
#define MODEL_NAME "Model"
#define IR_ID "ir"
#define IR_NAME "Ir"
#define MASTER_ID "master"
#define MASTER_NAME "Master"
#define BASS_ID "bass"
#define BASS_NAME "Bass"
#define MID_ID "mid"
#define MID_NAME "Mid"
#define TREBLE_ID "treble"
#define TREBLE_NAME "Treble"
#define PRESENCE_ID "presence"
#define PRESENCE_NAME "Presence"
#define DELAY_ID "delay"
#define DELAY_NAME "Delay"
#define REVERB_ID "reverb"
#define REVERB_NAME "Reverb"


class NeuralPi
{
public:
    //==============================================================================
    NeuralPi();
    ~NeuralPi();

    //==============================================================================
    void init (double sampleRate, int samplesPerBlock);
    void releaseResources();

    //void process (AudioBuffer<float>& buffer, MidiBuffer& midiMessages);
    void process (AudioBuffer<float>& buffer, int beginSample, int endSample);

    int getModelIndex(float model_param);
    //int getIrIndex(float ir_param);
    void loadConfig(File configFile);
    //void loadIR(File irFile);
    void setupDataDirectories();
    void installTones();

    void setDriver(float paramValue);
    void setDryWet(float paramValue);

    //void set_ampEQ(float bass_slider, float mid_slider, float treble_slider, float presence_slider);
    //void set_delayParams(float paramValue);
    //void set_reverbParams(float paramValue);
    
    float convertLogScale(float in_value, float x_min, float x_max, float y_min, float y_max);

    float decibelToLinear(float dbValue);

    void addDirectory(const File& file);
    //void addDirectoryIR(const File& file);
    void resetDirectory(const File& file);
    //void resetDirectoryIR(const File& file);
    std::vector<File> jsonFiles;
    //std::vector<File> irFiles;
    File currentDirectory = File::getCurrentWorkingDirectory().getFullPathName();
    File userAppDataDirectory = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile(JucePlugin_Manufacturer).getChildFile(JucePlugin_Name);
    File userAppDataDirectory_tones = userAppDataDirectory.getFullPathName() + "/tones";
    File userAppDataDirectory_irs = userAppDataDirectory.getFullPathName() + "/irs";

    // Pedal/amp states
    int amp_state = 1; // 0 = off, 1 = on
    int custom_tone = 0; // 0 = custom tone loaded, 1 = default channel tone
    File loaded_tone;
    juce::String loaded_tone_name;
    const char* char_filename = "";
    int model_loaded = 0;
    int current_model_index = 0;
    float num_models = 0.0;
    int model_index = 0; // Used in processBlock when converting slider param to model index
    bool lstm_state = true;

    juce::String loaded_ir_name;
    float num_irs = 0.0;
    int ir_loaded = 0;
    int custom_ir = 0; // 0 = custom tone loaded, 1 = default channel tone
    File loaded_ir;
    bool ir_state = true;
    int current_ir_index = 0;
    int ir_index = 0;

    bool is_conditioned = false;

    RT_LSTM LSTM;

    //juce::dsp::Reverb::Parameters rev_params;

private:
    //Eq4Band eq4band; // Amp EQ

    float gainParam = 0.0;
    float masterParam = 0.0;
    //AudioParameterFloat* bassParam;
    //AudioParameterFloat* midParam;
    //AudioParameterFloat* trebleParam;
    //AudioParameterFloat* presenceParam;
    float modelParam = 0.0;
    //AudioParameterFloat* irParam;
    //AudioParameterFloat* delayParam;
    //AudioParameterFloat* reverbParam;

    //juce::dsp::IIR::Filter<float> dcBlocker;

    // IR processing
    //CabSim cabSimIR;

    // Effects
    //enum
    //{
    //    delayIndex,
    //    reverbIndex
    //};

    //juce::dsp::ProcessorChain<Delay<float>, juce::dsp::Reverb> fxChain;

};