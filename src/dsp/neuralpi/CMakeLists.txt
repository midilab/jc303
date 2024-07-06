juce_add_binary_data(BinaryDataNeuralPi 
HEADER_NAME 
    BinaryDataNeuralPi.h 
NAMESPACE 
    BinaryDataNeuralPi 
SOURCES
    models/BluesJr.json
    models/TS9.json
    models/HT40_Overdrive.json
)

# Need to build BinaryDataNeuralPi with -fPIC flag on Linux
set_target_properties(BinaryDataNeuralPi PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)
