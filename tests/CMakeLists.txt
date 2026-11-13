cmake_minimum_required(VERSION 3.22)

# Standalone: cmake -S tests -B build-tests -DJUCE_DIR=/path/to/JUCE
#             cmake --build build-tests && ctest --test-dir build-tests
# Root build: cmake -B build -DJC303_BUILD_TESTS=ON
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(JC303Tests VERSION 0.1 LANGUAGES C CXX)
    enable_testing()
    if(NOT JUCE_DIR)
        set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib/JUCE")
    endif()
    add_subdirectory(${JUCE_DIR} JUCE EXCLUDE_FROM_ALL)
endif()

# Sequencer tests run against the real JUCE core modules (no stubs).
juce_add_console_app(sequencer_test PRODUCT_NAME "sequencer_test")
target_sources(sequencer_test PRIVATE sequencer_test.cpp)
target_compile_features(sequencer_test PRIVATE cxx_std_17)
target_include_directories(sequencer_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/shim
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/sequencer)
target_compile_definitions(sequencer_test PRIVATE
    JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(sequencer_test PRIVATE
    juce::juce_core juce::juce_audio_basics
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
add_test(NAME sequencer_test COMMAND sequencer_test)

add_executable(lfo_test lfo_test.cpp ../src/dsp/open303/dfl_LFO.cpp)
target_compile_features(lfo_test PRIVATE cxx_std_17)
target_include_directories(lfo_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp/open303)
add_test(NAME lfo_test COMMAND lfo_test)

# Open303 core tests need the CPM-fetched math_approx headers (root build: lib/math_approx-src).
# Standalone: cmake -S tests -B build-tests -DMATH_APPROX_INCLUDE=/path/to/math_approx-src/include
set(MATH_APPROX_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../lib/math_approx-src/include" CACHE PATH "math_approx include dir")
if(EXISTS "${MATH_APPROX_INCLUDE}/math_approx/math_approx.hpp")
    file(GLOB OPEN303_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp/open303/*.cpp)
    add_executable(open303_lfo_test open303_lfo_test.cpp ${OPEN303_SOURCES})
    target_compile_features(open303_lfo_test PRIVATE cxx_std_17)
    target_include_directories(open303_lfo_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp/open303
        ${MATH_APPROX_INCLUDE})
    add_test(NAME open303_lfo_test COMMAND open303_lfo_test)

    add_executable(open303_mod_test open303_mod_test.cpp ${OPEN303_SOURCES})
    target_compile_features(open303_mod_test PRIVATE cxx_std_17)
    target_include_directories(open303_mod_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp/open303
        ${MATH_APPROX_INCLUDE})
    add_test(NAME open303_mod_test COMMAND open303_mod_test)

    add_executable(open303_reverse_test open303_reverse_test.cpp ${OPEN303_SOURCES})
    target_compile_features(open303_reverse_test PRIVATE cxx_std_17)
    target_include_directories(open303_reverse_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp/open303
        ${MATH_APPROX_INCLUDE})
    add_test(NAME open303_reverse_test COMMAND open303_reverse_test)

    add_executable(open303_param_sweep_test open303_param_sweep_test.cpp ${OPEN303_SOURCES})
    target_compile_features(open303_param_sweep_test PRIVATE cxx_std_17)
    target_include_directories(open303_param_sweep_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp/open303
        ${MATH_APPROX_INCLUDE})
    add_test(NAME open303_param_sweep_test COMMAND open303_param_sweep_test)
endif()

# Oscillator models: header-only DSP, no JUCE.
add_executable(oscillator_test oscillator_test.cpp)
target_compile_features(oscillator_test PRIVATE cxx_std_17)
target_include_directories(oscillator_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp/open303)
add_test(NAME oscillator_test COMMAND oscillator_test)

# Factory presets against the real processor: links the plugin's shared code target (root build only).
if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    juce_add_console_app(preset_test PRODUCT_NAME "preset_test")
    target_sources(preset_test PRIVATE preset_test.cpp)
    target_compile_features(preset_test PRIVATE cxx_std_20)
    # JC303 is defined after this directory: take its generated JuceHeader and plugin defines at generate time
    target_include_directories(preset_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
        $<TARGET_PROPERTY:JC303,JUCE_GENERATED_SOURCES_DIRECTORY>
        $<TARGET_PROPERTY:JC303,INCLUDE_DIRECTORIES>)
    target_compile_definitions(preset_test PRIVATE $<TARGET_PROPERTY:JC303,COMPILE_DEFINITIONS>)
    target_link_libraries(preset_test PRIVATE JC303)
    add_test(NAME preset_test COMMAND preset_test)
endif()

# AnaMark .tun parser (pure C++, no JUCE)
add_executable(tuning_test tuning_test.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../src/dsp/tuning/TuningFileLoader.cpp)
target_compile_features(tuning_test PRIVATE cxx_std_17)
add_test(NAME tuning_test COMMAND tuning_test)
