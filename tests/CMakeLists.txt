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
