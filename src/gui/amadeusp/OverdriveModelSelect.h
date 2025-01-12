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

        setSize(250, 50);
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
        auto area = getLocalBounds().reduced(10);
        
        // Define button sizes
        int buttonWidth = 25;
        int buttonHeight = 25;
        int labelHeight = 25;
        
        // Calculate the space for the label
        int labelWidth = area.getWidth() - (2 * buttonWidth) - 25;

        // Set button bounds
        prevButton.setBounds(area.removeFromLeft(buttonWidth));
        area.removeFromLeft(10); // Padding between button and label
        modelName.setBounds(area.removeFromLeft(labelWidth).removeFromTop(labelHeight));
        area.removeFromLeft(10); // Padding between label and button
        nextButton.setBounds(area.removeFromLeft(buttonWidth));
    }

private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::TextButton prevButton;
    juce::TextButton nextButton;
    juce::Label modelName;
    juce::StringArray modelNameList;
};