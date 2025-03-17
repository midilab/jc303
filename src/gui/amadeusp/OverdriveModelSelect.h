#include "Gui.h"

class OverdriveModelSelect : public juce::Component,
                             public juce::AudioProcessorValueTreeState::Listener
{
public:
    OverdriveModelSelect(juce::AudioProcessorValueTreeState& vts, juce::StringArray modelNamesList)
        : valueTreeState(vts), modelNameList(modelNamesList)
    {
        // 
        customFont = juce::Font(juce::Typeface::createSystemTypefaceFor(BinaryData::ErbosDraco1StOpenNbpRegularl5wX_ttf, BinaryData::ErbosDraco1StOpenNbpRegularl5wX_ttfSize));
        customFont.setHeight(11.0f);

        //customFont = juce::Font(juce::Typeface::createSystemTypefaceFor(BinaryData::Inter_ttc, BinaryData::Inter_ttcSize));
        //customFont.setHeight(12.0f);
        
        // 
        imageLeftArrow = juce::ImageCache::getFromMemory(BinaryData::leftarrowpresets_png, BinaryData::leftarrowpresets_pngSize);
        imageRightArrow = juce::ImageCache::getFromMemory(BinaryData::rightarrowpresets_png, BinaryData::rightarrowpresets_pngSize);

        // Create and configure buttons and modelName
        addAndMakeVisible(prevButton);
        addAndMakeVisible(nextButton);
        addAndMakeVisible(modelName);
        
        // 
        modelName.setFont(customFont);
        modelName.setJustificationType(juce::Justification::centredLeft);

        // Set the images for the ImageButtons
        //prevButton.setImages(false, true, true, imageLeftArrow, 1.0f, juce::Colours::transparentBlack, imageLeftArrow, 1.0f, juce::Colours::transparentBlack, imageLeftArrow, 1.0f, juce::Colours::transparentBlack);
        //nextButton.setImages(false, true, true, imageRightArrow, 1.0f, juce::Colours::transparentBlack, imageRightArrow, 1.0f, juce::Colours::transparentBlack, imageRightArrow, 1.0f, juce::Colours::transparentBlack);

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

        setSize(127, 100);
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
        // Define button sizes
        int buttonWidth = 5;
        int buttonHeight = 9.1;
        int labelWidth = 127;
        int labelHeight = 25;
    
        // Set the label bounds (spans the full width)
        modelName.setBounds(0, 0, labelWidth, labelHeight);
    
        // Add space between the label and buttons (10px padding)
        //area.removeFromTop(10);  // Padding between label and buttons
    
        // Set prevButton bounds (aligned to the left, 10px below the label)
        prevButton.setBounds(4, 29, buttonWidth, buttonHeight);
    
        // Set nextButton bounds (aligned to the right, 10px below the label)
        nextButton.setBounds(118, 29, buttonWidth, buttonHeight);
    }

    // Override paint method for ImageButton to handle custom hover and normal states
    void paint(juce::Graphics& g) override
    {
        // Handle the paint for prevButton (left arrow)
        if (prevButton.isMouseOver())
        {
            paintImageButton(g, prevButton, imageLeftArrow, true);
        }
        else
        {
            paintImageButton(g, prevButton, imageLeftArrow, false);
        }

        // Handle the paint for nextButton (right arrow)
        if (nextButton.isMouseOver())
        {
            paintImageButton(g, nextButton, imageRightArrow, true);
        }
        else
        {
            paintImageButton(g, nextButton, imageRightArrow, false);
        }
    }

private:

    // Helper function to paint the image buttons with hover effects
    void paintImageButton(juce::Graphics& g, juce::ImageButton& button, const juce::Image& image, bool isHovered)
    {
        int frameHeight = image.getHeight() / 2;  // Split image vertically in two parts (hover and normal)
        int sourceY = isHovered ? frameHeight : 0; // If hovered, display second half of the image

        juce::Rectangle<int> sourceRect(0, sourceY, image.getWidth(), frameHeight);
        g.drawImage(image, button.getBounds().getX(), button.getBounds().getY(), button.getWidth(), button.getHeight(),
                    sourceRect.getX(), sourceRect.getY(), sourceRect.getWidth(), sourceRect.getHeight(),
                    false);
    }

    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::ImageButton prevButton;
    juce::ImageButton nextButton;
    juce::Label modelName;
    juce::StringArray modelNameList;

    juce::Font customFont;
    juce::Image imageLeftArrow;
    juce::Image imageRightArrow;
};