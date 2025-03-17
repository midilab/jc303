#pragma once

#include "neural_utils/ResampledRNNAccelerated.h"

#include "../BaseProcessor.h"
#include "../utility/DCBlocker.h"

namespace RONNTags
{
const juce::StringArray guitarMLModelResources {
    "TS9_DriveKnob_json",
    "MXR78_pedal_DistKnob_json",
    "Ibanez_Mostortion_Clone_GainKnob_json",
    "Mooer_CaliMkIV_GainKnob_json",
    "BossMT2_PedalHighGain_json",
    "Ibanez808TubeScreamer_json",
    "BigMuff_V6_T6_S9_json",
    "ProcoRatPedal_HighGain_json",
};

const juce::StringArray guitarMLModelNames {
    "Ibanez TS9", //
    "MXR 78", //
    "Ibanez Mostortion Clone",
    "Mooer CaliMkIV", //
    "Boss MT2",
    "Ibanez TS808",
    "BigMuff",
    "Pro Co RAT Distortion", //
};

const auto numBuiltInModels = (int) guitarMLModelResources.size();

const String modelTag = "model";
const String gainTag = "gain";
const String conditionTag = "condition";
const String sampleRateCorrFilterTag = "sample_rate_corr_filter";
const String customModelTag = "custom_model";
constexpr std::string_view modelNameTag = "byod_guitarml_model_name";
} // namespace RONNTags


class GuitarMLAmp : public BaseProcessor
{
public:
    explicit GuitarMLAmp (UndoManager* um = nullptr);
    ~GuitarMLAmp() override;

    ProcessorType getProcessorType() const override { return Drive; }
    static ParamLayout createParameterLayout();

    void prepare (double sampleRate, int samplesPerBlock) override;
    void processAudio (AudioBuffer<float>& buffer) override;

    /* std::unique_ptr<XmlElement> toXML() override;
    void fromXML (XmlElement* xml, const chowdsp::Version& version, bool loadPosition) override;

    bool getCustomComponents (OwnedArray<Component>& customComps, chowdsp::HostContextProvider& hcp) override;
    void addToPopupMenu (PopupMenu& menu) override; */

    void loadModel (int modelIndex, Component* parentComponent = nullptr);
    String getCurrentModelName() const;

    // added by midilab
    void setModelList(juce::Array<juce::File> modelFileList) { modelList = modelFileList; }
    int getModelListSize() { return modelList.size(); }
    int getCurrentModelIndex() { return currentModelIndex; }
    void loadUserModel(int modelIndex) {
        if (modelIndex >= modelList.size())
            modelIndex = modelList.size() - 1;
        try
        {
            const auto modelFile = modelList[modelIndex];
            const auto modelJson = chowdsp::JSONUtils::fromFile (modelFile);
            loadModelFromJson (modelJson, modelFile.getFileNameWithoutExtension());
            currentModelIndex = modelIndex;
        } catch (const std::exception& exc) {
            loadModel (0);
            //const auto errorMessage = String { "Unable to load GuitarML model from file!\n\n" } + exc.what();
        }
    }
    void setDriver (float value) {
        if (modelArch == ModelArch::LSTM40NoCond)
            getVTS().getParameter(RONNTags::gainTag)->setValue(value);
        else if (modelArch == ModelArch::LSTM40Cond)
            getVTS().getParameter(RONNTags::conditionTag)->setValue(value);
    }

private:
    void loadModelFromJson (const chowdsp::json& modelJson, const String& newModelName = {});
    using ModelChangeBroadcaster = chowdsp::Broadcaster<void()>;
    ModelChangeBroadcaster modelChangeBroadcaster;

    chowdsp::FloatParameter* gainParam = nullptr;
    chowdsp::SmoothedBufferValue<float> conditionParam;
    chowdsp::BoolParameter* sampleRateCorrectionFilterParam = nullptr;
    chowdsp::Gain<float> inGain;

    SpinLock modelChangingMutex;
    double processSampleRate = 96000.0;
    std::shared_ptr<FileChooser> customModelChooser;

    template <int numIns, int hiddenSize>
    using GuitarML_LSTM = EA::Variant<rnn_sse_arm::RNNAccelerated<numIns, hiddenSize, RecurrentLayerType::LSTMLayer, (int) RTNeural::SampleRateCorrectionMode::LinInterp>
#if JUCE_INTEL
                                      ,
                                      rnn_avx::RNNAccelerated<numIns, hiddenSize, RecurrentLayerType::LSTMLayer, (int) RTNeural::SampleRateCorrectionMode::LinInterp>
#endif
                                      >;

    using LSTM40Cond = GuitarML_LSTM<2, 40>;
    using LSTM40NoCond = GuitarML_LSTM<1, 40>;

    std::array<LSTM40Cond, 2> lstm40CondModels;
    std::array<LSTM40NoCond, 2> lstm40NoCondModels;
    chowdsp::HighShelfFilter<float> sampleRateCorrectionFilter;

    enum class ModelArch
    {
        LSTM40Cond,
        LSTM40NoCond,
    };

    ModelArch modelArch = ModelArch::LSTM40NoCond;

    chowdsp::json cachedModel {};

    DCBlocker dcBlocker;

    float normalizationGain = 1.0f;

    // added by midilab
    juce::Array<juce::File> modelList;
    int currentModelIndex = 0;
    // presets storage: user documents folder
    File userAppDataDirectory = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile(JucePlugin_Manufacturer).getChildFile(JucePlugin_Name);
    File userAppDataDirectory_tones = userAppDataDirectory.getFullPathName() + "/overdrive_models";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarMLAmp)
};
