juce_add_binary_data(BinaryData SOURCES
    resources/jc303-gui.png
    resources/jc303-knob.png
    resources/jc303-mod-knob.png
    resources/jc303-mod-switch.png
)

# Need to build BinaryData with -fPIC flag on Linux
set_target_properties(BinaryData PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)