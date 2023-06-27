#pragma once

#include "JC303.h"

//==============================================================================
class JC303Editor  : public juce::AudioProcessorEditor
{
public:
    explicit JC303Editor (JC303&);
    ~JC303Editor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    JC303& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JC303Editor)
};
