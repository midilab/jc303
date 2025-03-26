#pragma once

#include "neural_utils/ResampledRNNAccelerated.h"

#include "../BaseProcessor.h"
#include "../utility/DCBlocker.h"

namespace RONNTags
{

/* const juce::StringArray guitarMLModelResources {
    "TS9_DriveKnob_json",
    "Ibanez_Mostortion_Clone_GainKnob_json",
    "Mooer_CaliMkIV_GainKnob_json",
    "Ibanez808TubeScreamer_json",
};

const juce::StringArray guitarMLModelNames {
    "MXR 78",
    "Boss MT2",
    "Ibanez TS808",
    "Pro Co RAT Distortion",
}; */

const juce::StringArray guitarMLModelResources {
    "TS9_DriveKnob_json",
    "Ibanez_Mostortion_Clone_GainKnob_json",
    "Mooer_CaliMkIV_GainKnob_json",
    "BossMT2_PedalHighGain_json",
    "ProcoRatPedal_HighGain_json",
    "MXR78_pedal_DistKnob_json",
    "Ibanez808TubeScreamer_json",
    "RevvG3_Pedal_GainKnob_json",
    "Jeckyl_and_Hyde_Distortion_DriveKnob_json",
    "Friedman_BEOD_Pedal_GainKnob_json",
    "BlackstarHT40_AmpClean_json",
    "MesaMiniRec_HighGain_DirectOut_json",
    "Splawn_OD_FractalFM3_HighGain_json",
    "EthosLeadChan_GainKnob_json",
    "PrincetonAmp_Clean_json",
    "DumbleKit_HighG_DirectOut_json",
    "BlackstarHT40_GainKnob_SM57mic_json",
    "TRexMudhoney_plus_PorkLoin_HighGain_json",
    "PrinceOfToneClone_OD_Knob_json",
    "PorkLoinPedal_LowGain_json",
    "PrinceOfToneClone_Dist_Knob_json",
    "AguilarAgro_Bright_Bass_json",
    "AguilarAgro_Dark_Bass_json",
    "BadCat50_MedGain_PREAMP_json",
    "ShiftTwin_Clean2_PREAMP_json",
    "Sovtek50_MedGain_DIRECT_json",
    "ShiftTwin_StampedeDT_PREAMP_json",
    "Sovtek50_DodFX56B_DIRECT_json",
    "ENGL_E645_Clean_EdoardoNapoli_json",
    "Filmosound_with_cab_json",
    "ENGL_E430_Clean_EdoardoNapoli_json",
    "ElCoyote_Trainwreck_Crunch_json",
    "Supro_Bold_DriveKnob_json",
    "GoatPedal_HighGain_json",
    "ProteinBlue_pedal_DriveKnob_json",
    "LittleBigMuff_HighGainPedal_json",
    "BigMuff_V6_T3_S5_json"
};

const juce::StringArray guitarMLModelNames {
    "Ibanez TS9",
    "Ibanez Mostortion Clone",
    "Mooer CaliMkIV",
    "Boss MT2",
    "Pro Co RAT Distortion",
    "MXR 78",
    "Ibanez TS808",
    "RevvG3 Pedal GainKnob",
    "Jeckyl and Hyde Distortion DriveKnob",
    "Friedman BEOD Pedal GainKnob",
    "BlackstarHT40 AmpClean",
    "MesaMiniRec HighGain DirectOut",
    "Splawn OD Fractal FM3 HighGain",
    "EthosLeadChan GainKnob",
    "PrincetonAmp Clean",
    "DumbleKit HighG DirectOut",
    "BlackstarHT40 GainKnob SM57mic",
    "TRexMudhoney plus PorkLoin HighGain",
    "PrinceOfTone Clone OD Knob",
    "PorkLoinPedal LowGain",
    "PrinceOfToneClone Dist Knob",
    "AguilarAgro Bright Bass",
    "AguilarAgro Dark Bass",
    "BadCat50 MedGain PREAMP",
    "ShiftTwin Clean2 PREAMP",
    "Sovtek50 MedGain DIRECT",
    "ShiftTwin StampedeDT PREAMP",
    "Sovtek50 DodFX56B DIRECT",
    "ENGL E645 Clean EdoardoNapoli",
    "Filmosound with cab",
    "ENGL E430 Clean EdoardoNapoli",
    "ElCoyote Trainwreck Crunch",
    "Supro Bold DriveKnob",
    "GoatPedal HighGain",
    "ProteinBlue pedal DriveKnob",
    "LittleBigMuff HighGainPedal",
    "BigMuff V6 T3 S5"
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
    void setModelList(juce::Array<juce::File> modelFileList, juce::StringArray modelNameList) { 
        // modelList should contain built in models data first
        modelListNames.addArray(RONNTags::guitarMLModelNames);
        // add each element of modelNameList to the end of modelListNames
        modelListNames.addArray(modelNameList);
        // this list is only for user json files. no buil-in here
        modelList = modelFileList;
    }
    juce::StringArray getModelListNames() { return modelListNames; }
    int getModelListSize() { return RONNTags::numBuiltInModels + modelList.size(); }
    int getCurrentModelIndex() { return currentModelIndex; }
    void loadUserModel(int modelIndex) {
        if (modelIndex >= getModelListSize())
            modelIndex = getModelListSize() - 1;
        if (modelIndex < 0)
            modelIndex = 0;
        // load built-in model
        if (modelIndex < RONNTags::numBuiltInModels) {
            loadModel (modelIndex);
        // load user model
        } else {
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
    juce::StringArray modelListNames;
    int currentModelIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarMLAmp)
};
