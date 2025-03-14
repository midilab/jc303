juce_add_binary_data(BinaryData SOURCES
    resources/jc-303-gui.png
    resources/large-rotary.png
    resources/medium-rotary.png
    resources/small-rotary.png
    resources/switch.png
    resources/dark-led-step-sequencer.png
    resources/left-arrow-presets.png
    resources/right-arrow-presets.png
    resources/acid-smile.png
    resources/Inter.ttc
)

# Need to build BinaryData with -fPIC flag on Linux
set_target_properties(BinaryData PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)