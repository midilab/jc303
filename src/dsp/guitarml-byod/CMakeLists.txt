# this guitarml port is based on Chowdhury-DSP BYOD git commit 6ef1720c830795b2fcc45b0ebbf0c217e18de5f3

# rtneural-utils needs c++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ensure target enforces C++20
target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_20)

# Ensure CMAKE_BUILD_TYPE is set to Release if not explicitly provided
# This way we can provide optmizations without change default build instructions
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type (e.g. Debug, Release, RelWithDebInfo)" FORCE)
endif()

# Provide options for CMAKE_BUILD_TYPE for clarity
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")

# set library dependency versions
set(LIB_RTNEURAL_TAG "04cb333bc4b174760958a77c7ce076eae38fe8e4") 
set(LIB_CHOWDSP_MATH_APPROX_TAG "0c68d4d17242d707ba07fa7f1901692b7ed72d58") 
set(LIB_VARIANT_TAG "3fce49cfca50ba3b05026d41ffc4911a8e653378") 

# start fetching dependencies
include(FetchContent)

# rtneural fetch
FetchContent_Declare(rtneural
    GIT_REPOSITORY https://github.com/jatinchowdhury18/RTNeural.git
    GIT_TAG        ${LIB_RTNEURAL_TAG}
    GIT_CONFIG     advice.detachedHead=false)
set(RTNEURAL_XSIMD ON CACHE BOOL "Use RTNeural with XSIMD backend" FORCE)

# chowdsp math_approx fetch
FetchContent_Declare(math_approx
    GIT_REPOSITORY https://github.com/Chowdhury-DSP/math_approx.git
    GIT_TAG        ${LIB_CHOWDSP_MATH_APPROX_TAG}
    GIT_CONFIG     advice.detachedHead=false)

# ea_variant fetch
FetchContent_Declare(ea_variant
    GIT_REPOSITORY https://github.com/eyalamirmusic/Variant.git
    GIT_TAG        ${LIB_VARIANT_TAG}
    GIT_CONFIG     advice.detachedHead=false)

FetchContent_MakeAvailable(rtneural math_approx ea_variant)

target_sources("${PROJECT_NAME}"
    PRIVATE
      processors/BaseProcessor.cpp
      processors/drive/GuitarMLAmp.cpp
)

# cmake dependencies
include(cmake/WarningFlags.cmake)
include(cmake/ConfigFlags.cmake)
include(cmake/RuntimeSIMDLib.cmake)

# AVX/SSE files for accelerated neural nets and other DSP
make_lib_simd_runtime(dsp_accelerated
    SOURCES
        processors/drive/neural_utils/RNNAccelerated.cpp
        #processors/other/poly_octave/PolyOctaveV2FilterBankImpl.cpp
)
foreach(target IN ITEMS dsp_accelerated_sse_or_arm dsp_accelerated_avx)
    target_link_libraries(${target}
        PRIVATE
            math_approx
            config_flags
            juce::juce_recommended_lto_flags
            warning_flags
    )
    target_include_directories(${target}
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}"
            ${rtneural_SOURCE_DIR}
            ${rtneural_SOURCE_DIR}/modules/xsimd/include
    )
    target_compile_definitions(${target}
        PRIVATE
            RTNEURAL_USE_XSIMD=1
            _USE_MATH_DEFINES=1
    )
    set_target_properties(${target} PROPERTIES
        POSITION_INDEPENDENT_CODE TRUE
        VISIBILITY_INLINES_HIDDEN TRUE
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
    )
endforeach()
target_compile_definitions(dsp_accelerated_sse_or_arm PRIVATE RTNEURAL_DEFAULT_ALIGNMENT=16 RTNEURAL_NAMESPACE=RTNeural_sse_arm)
target_compile_definitions(dsp_accelerated_avx PRIVATE RTNEURAL_DEFAULT_ALIGNMENT=32 RTNEURAL_NAMESPACE=RTNeural_avx)
#target_link_libraries("${PROJECT_NAME}" PRIVATE dsp_accelerated)

target_link_libraries("${PROJECT_NAME}" 
  PRIVATE
    dsp_accelerated
    ea_variant
    math_approx
    juce::juce_core
    juce::juce_dsp
    chowdsp::chowdsp_dsp_utils
    chowdsp::chowdsp_plugin_base
    chowdsp::chowdsp_json
    RTNeural
    BinaryDataGuitarMLModels
  PUBLIC
    config_flags
    juce::juce_recommended_lto_flags
    warning_flags
)

# special flags for MSVC
if (MSVC)
    target_compile_options("${PROJECT_NAME}" PRIVATE /bigobj)
endif ()

# generate binary data for guitarml models
juce_add_binary_data(BinaryDataGuitarMLModels 
  HEADER_NAME 
    BinaryDataGuitarMLModels.h 
  NAMESPACE 
    BinaryDataGuitarMLModels 
  SOURCES
    models/JC303/AguilarAgro_Bright_Bass.json
    models/JC303/AguilarAgro_Dark_Bass.json
    models/JC303/BadCat50_MedGain_PREAMP.json
    models/JC303/BigMuff_V6_T3_S5.json
    models/JC303/BlackstarHT40_AmpClean.json
    models/JC303/BlackstarHT40_GainKnob_SM57mic.json
    models/JC303/BossMT2_PedalHighGain.json
    models/JC303/DumbleKit_HighG_DirectOut.json
    models/JC303/ElCoyote_Trainwreck_Crunch.json
    models/JC303/ENGL_E430_Clean_EdoardoNapoli.json
    models/JC303/ENGL_E645_Clean_EdoardoNapoli.json
    models/JC303/EthosLeadChan_GainKnob.json
    models/JC303/Filmosound_with_cab.json
    models/JC303/Friedman_BEOD_Pedal_GainKnob.json
    models/JC303/GoatPedal_HighGain.json
    models/JC303/Ibanez808TubeScreamer.json
    models/JC303/Ibanez_Mostortion_Clone_GainKnob.json
    models/JC303/Jeckyl_and_Hyde_Distortion_DriveKnob.json
    models/JC303/LittleBigMuff_HighGainPedal.json
    models/JC303/MesaMiniRec_HighGain_DirectOut.json
    models/JC303/Mooer_CaliMkIV_GainKnob.json
    models/JC303/MXR78_pedal_DistKnob.json
    models/JC303/PorkLoinPedal_LowGain.json
    models/JC303/PrinceOfToneClone_Dist_Knob.json
    models/JC303/PrinceOfToneClone_OD_Knob.json
    models/JC303/PrincetonAmp_Clean.json
    models/JC303/ProcoRatPedal_HighGain.json
    models/JC303/ProteinBlue_pedal_DriveKnob.json
    models/JC303/RevvG3_Pedal_GainKnob.json
    models/JC303/ShiftTwin_Clean2_PREAMP.json
    models/JC303/ShiftTwin_StampedeDT_PREAMP.json
    models/JC303/Sovtek50_DodFX56B_DIRECT.json
    models/JC303/Sovtek50_MedGain_DIRECT.json
    models/JC303/Splawn_OD_FractalFM3_HighGain.json
    models/JC303/Supro_Bold_DriveKnob.json
    models/JC303/TRexMudhoney_plus_PorkLoin_HighGain.json
    models/JC303/TS9_DriveKnob.json
)

# Need to build BinaryDataGuitarMLModels with -fPIC flag on Linux
set_target_properties(BinaryDataGuitarMLModels PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# precompiled pch and set some compiler directives to force compilation to use cxx instead of c
#target_precompile_headers("${PROJECT_NAME}" PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>")
#target_include_directories("${PROJECT_NAME}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")