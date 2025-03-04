#include "Gui.h"

class OverdriveModelSelect : public juce::Component,
                             public juce::AudioProcessorValueTreeState::Listener
{
public:
    OverdriveModelSelect(juce::AudioProcessorValueTreeState& vts, juce::StringArray modelNamesList)
        : valueTreeState(vts), modelNameList(modelNamesList)
    {
        // Create and configure buttons and modelName
        addAndMakeVisible(prevButton);
        addAndMakeVisible(nextButton);
        addAndMakeVisible(modelName);

        prevButton.setButtonText(" < ");
        nextButton.setButtonText(" > ");

        prevButton.onClick = [this] { changeModelIndex(-1); };
        nextButton.onClick = [this] { changeModelIndex(1); };

        // Get the initial value of the parameter and set the model name
        auto* param = dynamic_cast<juce::AudioParameterInt*>(valueTreeState.getParameter("overdriveModelIndex"));
        if (param != nullptr)
        {
            int modelIndex = param->get();
            setModelName(modelNameList[modelIndex]);
        }

        // Attach the listener
        valueTreeState.addParameterListener("overdriveModelIndex", this);

        setSize(127, 50);
    }

    ~OverdriveModelSelect() override
    {
        // Remove the listener
        valueTreeState.removeParameterListener("overdriveModelIndex", this);
    }

    void changeModelIndex(int delta)
    {
        auto* param = dynamic_cast<juce::AudioParameterInt*>(valueTreeState.getParameter("overdriveModelIndex"));
        if (param != nullptr)
        {
            int currentValue = param->get();
            int newValue = juce::jlimit(param->getRange().getStart(), param->getRange().getEnd(), currentValue + delta);
            param->beginChangeGesture();
            *param = newValue;
            param->endChangeGesture();
        }
    }

    void parameterChanged(const String& parameterID, float newValue) override
    {
        if (parameterID == "overdriveModelIndex")
        {
            // Handle the parameter change
            setModelName(modelNameList[static_cast<int>(newValue)]);
        }
    }

    void setModelName(const juce::String& name)
    {
        modelName.setText(name, juce::dontSendNotification);
    }

    void resized() override
    {
        //auto area = getLocalBounds().reduced(10);
    
        // Define button sizes
        int buttonWidth = 15;
        int buttonHeight = 15;
        int labelWidth = 127;
        int labelHeight = 25;
    
        // Set the label bounds (spans the full width)
        modelName.setBounds(0, 0, labelWidth, labelHeight);
    
        // Add space between the label and buttons (10px padding)
        //area.removeFromTop(10);  // Padding between label and buttons
    
        // Set prevButton bounds (aligned to the left, 10px below the label)
        prevButton.setBounds(0, 22, buttonWidth, buttonHeight);
    
        // Set nextButton bounds (aligned to the right, 10px below the label)
        nextButton.setBounds(110, 22, buttonWidth, buttonHeight);
    }

private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::TextButton prevButton;
    juce::TextButton nextButton;
    juce::Label modelName;
    juce::StringArray modelNameList;
};