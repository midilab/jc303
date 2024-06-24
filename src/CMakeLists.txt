# Add sources from the current directory to the project target.
# We pass the target that needs the sources as the first argument,
# then a visibility parameter for the sources (which should normally be PRIVATE).
# Finally, we supply a list of source files that will be built into the target.

target_sources("${PROJECT_NAME}"
    PRIVATE
        dsp/open303/GlobalFunctions.cpp
        dsp/open303/rosic_AcidPattern.cpp
        dsp/open303/rosic_AcidSequencer.cpp
        dsp/open303/rosic_AnalogEnvelope.cpp
        dsp/open303/rosic_BiquadFilter.cpp
        dsp/open303/rosic_BlendOscillator.cpp
        dsp/open303/rosic_Complex.cpp
        dsp/open303/rosic_DecayEnvelope.cpp
        dsp/open303/rosic_EllipticQuarterBandFilter.cpp
        dsp/open303/rosic_FourierTransformerRadix2.cpp
        dsp/open303/rosic_FunctionTemplates.cpp
        dsp/open303/rosic_LeakyIntegrator.cpp
        dsp/open303/rosic_MidiNoteEvent.cpp
        dsp/open303/rosic_MipMappedWaveTable.cpp
        dsp/open303/rosic_NumberManipulations.cpp
        dsp/open303/rosic_OnePoleFilter.cpp
        dsp/open303/rosic_Open303.cpp
        dsp/open303/rosic_RealFunctions.cpp
        dsp/open303/rosic_TeeBeeFilter.cpp
       
	gui/${GUI_THEME}/JuceLibraryCode/BinaryData.cpp
        gui/${GUI_THEME}/Gui.cpp

        JC303.cpp)

# Add subdirectories with source files, if present
# add_subdirectory(<name>)
