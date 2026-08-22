juce_add_binary_data(BinaryData SOURCES
    resources/jc-303-gui.png
    resources/jc-303-gui-background.png
    resources/knob_large_silver.png
    resources/switch.png
    resources/switch_2.png
    resources/left-arrow-presets.png
    resources/right-arrow-presets.png
    resources/small-button-step-sequencer.png
    resources/medium-button-step-sequencer.png
    resources/large-button-step-sequencer.png
    #resources/ErbosDraco1StOpenNbpRegular-l5wX.ttf
    resources/PressStart2PRegular.ttf
    # TODO: remove after finish UI end version
    resources/jc303-mod-knob.png
)

# Need to build BinaryData with -fPIC flag on Linux
set_target_properties(BinaryData PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)
