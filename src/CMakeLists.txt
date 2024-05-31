# Add sources from the current directory to the project target.
# We pass the target that needs the sources as the first argument,
# then a visibility parameter for the sources (which should normally be PRIVATE).
# Finally, we supply a list of source files that will be built into the target.

target_sources("${PROJECT_NAME}"
    PRIVATE
        dsp/GlobalFunctions.cpp
        dsp/rosic_AcidPattern.cpp
        dsp/rosic_AcidSequencer.cpp
        dsp/rosic_AnalogEnvelope.cpp
        dsp/rosic_BiquadFilter.cpp
        dsp/rosic_BlendOscillator.cpp
        dsp/rosic_Complex.cpp
        dsp/rosic_DecayEnvelope.cpp
        dsp/rosic_EllipticQuarterBandFilter.cpp
        dsp/rosic_FourierTransformerRadix2.cpp
        dsp/rosic_FunctionTemplates.cpp
        dsp/rosic_LeakyIntegrator.cpp
        dsp/rosic_MidiNoteEvent.cpp
        dsp/rosic_MipMappedWaveTable.cpp
        dsp/rosic_NumberManipulations.cpp
        dsp/rosic_OnePoleFilter.cpp
        dsp/rosic_Open303.cpp
        dsp/rosic_RealFunctions.cpp
        dsp/rosic_TeeBeeFilter.cpp
       
	#gui/midilab/BinaryData.cpp
        #gui/midilab/Gui.cpp

	gui/amadeus/BinaryData.cpp
        gui/amadeus/Gui.cpp

        JC303.cpp)

# Add subdirectories with source files, if present
# add_subdirectory(<name>)
