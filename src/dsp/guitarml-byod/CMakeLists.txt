# this guitarml port is based on Chowdhury-DSP BYOD git commit 6ef1720c830795b2fcc45b0ebbf0c217e18de5f3

# rtneural-utils needs c++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ensure target enforces C++20
target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_20)

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
            ea_variant
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
    RTNeural
    ea_variant
    math_approx
    config_flags
    warning_flags
    juce::juce_core
    juce::juce_dsp
    juce::juce_recommended_lto_flags
    chowdsp::chowdsp_dsp_utils
    chowdsp::chowdsp_plugin_base
    chowdsp::chowdsp_json
)

# special flags for MSVC
if (MSVC)
    target_compile_options("${PROJECT_NAME}" PRIVATE /bigobj)
endif ()

# should we keep models here?
juce_add_binary_data(BinaryGuitarMLModels 
HEADER_NAME 
    BinaryGuitarMLModels.h 
NAMESPACE 
    BinaryGuitarMLModels 
SOURCES
    models/TS9_DriveKnob.json
    models/BluesJrAmp_VolKnob.json
    models/MesaRecMini_ModernChannel_GainKnob.json
)

# Need to build BinaryGuitarMLModels with -fPIC flag on Linux
#set_target_properties(BinaryGuitarMLModels PROPERTIES
#    POSITION_INDEPENDENT_CODE TRUE)

# precompiled pch and set some compiler directives to force compilation to use cxx instead of c
#target_precompile_headers("${PROJECT_NAME}" PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.h>")
#target_include_directories("${PROJECT_NAME}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")