/*
  ==============================================================================

    Based on NeuralPi 1.3.0 PluginProcessor.cpp 

  ==============================================================================
*/

#include "NeuralPi.h"

//==============================================================================
NeuralPi::NeuralPi()
{
    setupDataDirectories();
    installTones();
    resetDirectory(userAppDataDirectory_tones);
    // Sort jsonFiles alphabetically
    std::sort(jsonFiles.begin(), jsonFiles.end());
    if (jsonFiles.size() > 0) {
        loadConfig(jsonFiles[current_model_index]);
    }

    //resetDirectoryIR(userAppDataDirectory_irs);
    // Sort irFiles alphabetically
    //std::sort(irFiles.begin(), irFiles.end());
    //if (irFiles.size() > 0) {
    //    loadIR(irFiles[current_ir_index]);
    //}

    // initialize parameters:
    //addParameter(gainParam = new AudioParameterFloat(GAIN_ID, GAIN_NAME, NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    //addParameter(masterParam = new AudioParameterFloat(MASTER_ID, MASTER_NAME, NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    //addParameter(bassParam = new AudioParameterFloat(BASS_ID, BASS_NAME, NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    //addParameter(midParam = new AudioParameterFloat(MID_ID, MID_NAME, NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    //addParameter(trebleParam = new AudioParameterFloat(TREBLE_ID, TREBLE_NAME, NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    //addParameter(presenceParam = new AudioParameterFloat(PRESENCE_ID, PRESENCE_NAME, NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f));
    //addParameter(modelParam = new AudioParameterFloat(MODEL_ID, MODEL_NAME, NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    //addParameter(irParam = new AudioParameterFloat(IR_ID, IR_NAME, NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    //addParameter(delayParam = new AudioParameterFloat(DELAY_ID, DELAY_NAME, NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    //addParameter(reverbParam = new AudioParameterFloat(REVERB_ID, REVERB_NAME, NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
}


NeuralPi::~NeuralPi()
{
}

//==============================================================================
void NeuralPi::init (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    LSTM.reset();

    // set up DC blocker
    //dcBlocker.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 35.0f);
    //dsp::ProcessSpec spec{ sampleRate, static_cast<uint32> (samplesPerBlock), 2 };
    //dcBlocker.prepare(spec);

    // Set up IR
    //cabSimIR.prepare(spec);

    // fx chain
    //fxChain.prepare(spec);    
}

void NeuralPi::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

//void NeuralPi::process (AudioBuffer<float>& buffer, MidiBuffer& midiMessages)
void NeuralPi::process (AudioBuffer<float>& buffer, int beginSample, int endSample)
{
    ScopedNoDenormals noDenormals;

    // Setup Audio Data
    const int numSamples = buffer.getNumSamples();
    //const int numInputChannels = getTotalNumInputChannels();
    //const int sampleRate = getSampleRate();

    //auto block = dsp::AudioBlock<float>(buffer).getSingleChannelBlock(0);
    //auto context = juce::dsp::ProcessContextReplacing<float>(block);

    // Amp =============================================================================
    if (amp_state == 1) {
        //auto gainParam = static_cast<float> (gainParam->get());
        //auto masterParam = static_cast<float> (masterParam->get());
        // Note: Default 0.0 -> 1.0 param range is converted to +-12.0 here
        //auto bass = (static_cast<float> (bassParam->get() - 0.5) * 24.0);
        //auto mid = (static_cast<float> (midParam->get() - 0.5) * 24.0);
        //auto treble = (static_cast<float> (trebleParam->get() - 0.5) * 24.0);
        //auto presence = (static_cast<float> (presenceParam->get() - 0.5) * 24.0);

        //auto delay = (static_cast<float> (delayParam->get()));
        //auto reverb = (static_cast<float> (reverbParam->get()));

        //auto model = static_cast<float> (modelParam->get());
        model_index = getModelIndex(modelParam);

        //auto ir = static_cast<float> (irParam->get());
        //ir_index = getIrIndex(ir);

        // Applying gainParam adjustment for snapshot models
        if (LSTM.input_size == 1) {
            buffer.applyGain(gainParam * 2.0);
        } 

        // Process EQ
        //eq4band.setParameters(bass, mid, treble, presence);// Better to move this somewhere else? Only need to set when value changes
        //eq4band.process(buffer.getReadPointer(0), buffer.getWritePointer(0), midiMessages, numSamples, numInputChannels, sampleRate);

        // Apply LSTM model
        if (model_loaded == 1 && lstm_state == true) {
            if (current_model_index != model_index) {
                loadConfig(jsonFiles[model_index]);
                current_model_index = model_index;
            }

            // Process LSTM based on input_size (snapshot model or conditioned model)
            if (LSTM.input_size == 1) {
                LSTM.process(buffer.getReadPointer(0)+beginSample, buffer.getWritePointer(0)+beginSample, endSample-beginSample);
            } else {
                LSTM.process(buffer.getReadPointer(0)+beginSample, gainParam, buffer.getWritePointer(0)+beginSample, endSample-beginSample);
            }
        }

        // Process IR
        //if (ir_state == true && num_irs > 0) {
        //    if (current_ir_index != ir_index) {
        //        loadIR(irFiles[ir_index]);
        //        current_ir_index = ir_index;
        //    }
        //    auto block = dsp::AudioBlock<float>(buffer).getSingleChannelBlock(0);
        //    auto context = juce::dsp::ProcessContextReplacing<float>(block);
        //    cabSimIR.process(context);
        //
        //    // IR generally makes output quieter, add volume here to make ir on/off volume more even
        //    buffer.applyGain(2.0);
        //}

        //    Master Volume 
        buffer.applyGain(masterParam * 2.0); // Adding volume range (2x) mainly for clean models

        // Process Delay, and Reverb
        //set_delayParams(delay);
        //set_reverbParams(reverb);
        //fxChain.process(context);
    }

    // process DC blocker
    //auto monoBlock = dsp::AudioBlock<float>(buffer).getSingleChannelBlock(0);
    //dcBlocker.process(dsp::ProcessContextReplacing<float>(monoBlock));
    
    //for (int ch = 1; ch < buffer.getNumChannels(); ++ch)
    //    buffer.copyFrom(ch, 0, buffer, 0, 0, buffer.getNumSamples());
}

int NeuralPi::getModelIndex(float model_param)
{
    int a = static_cast<int>(round(model_param * (num_models - 1.0)));
    if (a > num_models - 1) {
        a = num_models - 1;
    }
    else if (a < 0) {
        a = 0;
    }
    return a;
}

//int NeuralPi::getIrIndex(float ir_param)
//{
//    int a = static_cast<int>(round(ir_param * (num_irs - 1.0)));
//    if (a > num_irs - 1) {
//        a = num_irs - 1;
//    }
//    else if (a < 0) {
//        a = 0;
//    }
//    return a;
//}

void NeuralPi::loadConfig(File configFile)
{
    // TODO: when expose model change to the user suspend it on JC303.cpp
    //this->suspendProcessing(true);
    String path = configFile.getFullPathName();
    char_filename = path.toUTF8();

    try {
        // Check input size for conditioned models
        // read JSON file
        std::ifstream i2(char_filename);
        nlohmann::json weights_json;
        i2 >> weights_json;

        int input_size_json = weights_json["/model_data/input_size"_json_pointer];
        LSTM.input_size = input_size_json;
        if (input_size_json == 1) {
            is_conditioned = false;
            LSTM.load_json(char_filename);
        }
        else {
            is_conditioned = true;
            LSTM.load_json2(char_filename);
        }
        model_loaded = 1;
    }
    catch (const std::exception& e) {
        DBG("Unable to load json file: " + configFile.getFullPathName());
        std::cout << e.what();
    }

    // TODO: when expose model change to the user suspend it on JC303.cpp
    //this->suspendProcessing(false);
}

//void NeuralPi::loadIR(File irFile)
//{
//    this->suspendProcessing(true);
//
//    try {
//        cabSimIR.load(irFile);
//        ir_loaded = 1;
//    }
//    catch (const std::exception& e) {
//        DBG("Unable to load IR file: " + irFile.getFullPathName());
//        std::cout << e.what();
//    }
//    this->suspendProcessing(false);
//}

void NeuralPi::resetDirectory(const File& file)
{
    jsonFiles.clear();
    if (file.isDirectory())
    {
        juce::Array<juce::File> results;
        file.findChildFiles(results, juce::File::findFiles, false, "*.json");
        for (int i = results.size(); --i >= 0;)
            jsonFiles.push_back(File(results.getReference(i).getFullPathName()));
    }
}

//void NeuralPi::resetDirectoryIR(const File& file)
//{
//    irFiles.clear();
//    if (file.isDirectory())
//    {
//        juce::Array<juce::File> results;
//        file.findChildFiles(results, juce::File::findFiles, false, "*.wav");
//        for (int i = results.size(); --i >= 0;)
//            irFiles.push_back(File(results.getReference(i).getFullPathName()));
//    }
//}

void NeuralPi::addDirectory(const File& file)
{
    if (file.isDirectory())
    {
        juce::Array<juce::File> results;
        file.findChildFiles(results, juce::File::findFiles, false, "*.json");
        for (int i = results.size(); --i >= 0;)
        {
            jsonFiles.push_back(File(results.getReference(i).getFullPathName()));
            num_models = num_models + 1.0;
        }
    }
}

//void NeuralPi::addDirectoryIR(const File& file)
//{
//    if (file.isDirectory())
//    {
//        juce::Array<juce::File> results;
//        file.findChildFiles(results, juce::File::findFiles, false, "*.wav");
//        for (int i = results.size(); --i >= 0;)
//        {
//            irFiles.push_back(File(results.getReference(i).getFullPathName()));
//            num_irs = num_irs + 1.0;
//        }
//    }
//}

void NeuralPi::setupDataDirectories()
{
    // User app data directory
    File userAppDataTempFile = userAppDataDirectory.getChildFile("tmp.pdl");

    File userAppDataTempFile_tones = userAppDataDirectory_tones.getChildFile("tmp.pdl");

    File userAppDataTempFile_irs = userAppDataDirectory_irs.getChildFile("tmp.pdl");

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

    if (!userAppDataDirectory_irs.exists()) {
        userAppDataTempFile_irs.create();
    }
    if (userAppDataTempFile_irs.existsAsFile()) {
        userAppDataTempFile_irs.deleteFile();
    }


    // Add the tones directory and update tone list
    addDirectory(userAppDataDirectory_tones);
    //addDirectoryIR(userAppDataDirectory_irs);
}

void NeuralPi::installTones()
//====================================================================
// Description: Checks that the default tones
//  are installed to the NeuralPi directory, and if not, 
//  copy them from the binary data in the plugin to that directory.
//
//====================================================================
{
    // Default tones
    File ts9_tone = userAppDataDirectory_tones.getFullPathName() + "/TS9.json";
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
    }
    
}


void NeuralPi::setDriver(float paramValue)
{
    gainParam = paramValue;
}

void NeuralPi::setDryWet(float paramValue)
{
    masterParam = paramValue;
}

//void NeuralPi::set_ampEQ(float bass_slider, float mid_slider, float treble_slider, float presence_slider)
//{
//    eq4band.setParameters(bass_slider, mid_slider, treble_slider, presence_slider);
//}

//void NeuralPi::set_delayParams(float paramValue)
//{
//    auto& del = fxChain.template get<delayIndex>();
//    del.setWetLevel(paramValue);
//    // Setting delay time as larger steps to minimize clicking, and to start delay time at a reasonable value
//    if (paramValue < 0.25) {
//        del.setDelayTime(0, 0.25);
//    } else if (paramValue < 0.5) {
//        del.setDelayTime(0, 0.5);
//    } else if (paramValue < 0.75) {
//        del.setDelayTime(0, 0.75);
//    } else {
//        del.setDelayTime(0, 1.0);
//    }
//    del.setFeedback(0.8-paramValue/2);
//}


//void NeuralPi::set_reverbParams(float paramValue)
//{
//    auto& rev = fxChain.template get<reverbIndex>();
//    rev_params = rev.getParameters();
//
//    // Sets reverb params as a function of a single reverb param value ( 0.0 to 1.0)
//    rev_params.wetLevel = paramValue;
//    rev_params.damping = 0.6 - paramValue/2; // decay is inverse of damping
//    rev_params.roomSize = 0.8 - paramValue/2;
//    //rev_params.width = paramValue;
//    rev.setParameters(rev_params);
//}

float NeuralPi::convertLogScale(float in_value, float x_min, float x_max, float y_min, float y_max)
{
    float b = log(y_max / y_min) / (x_max - x_min);
    float a = y_max / exp(b * x_max);
    float converted_value = a * exp(b * in_value);
    return converted_value;
}


float NeuralPi::decibelToLinear(float dbValue)
{
    return powf(10.0, dbValue/20.0);
}