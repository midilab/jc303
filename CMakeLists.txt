cmake_minimum_required(VERSION 3.15)

project(JC303 VERSION 0.0.1)


### Dependency versions ###

set(LIB_JUCE_TAG "7.0.5")


### IDE Generator pre-config ###

# Xcode: Disable automatic build scheme generation globally.
#        Instead, we explicitely specify which targets require schemes.
set(CMAKE_XCODE_GENERATE_SCHEME OFF)

# IDEs:  Enable grouping of source files into folders in IDEs.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# IDEs:  Create a folder in the IDE with the JUCE Module code.
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Show all module sources in IDE projects" ON)


### Dependency management ###

include(FetchContent)

# Keep dependencies outside of the "Build" directory.
# This allows to do a clean build of the project without re-downloading or
# rebuilding the dependencies.
set(FETCHCONTENT_BASE_DIR "${PROJECT_SOURCE_DIR}/lib" CACHE PATH "External dependencies path." FORCE)

FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        ${LIB_JUCE_TAG}
    GIT_SHALLOW    TRUE
    GIT_CONFIG     advice.detachedHead=false    # Disable detached HEAD warning for fetching a specific tag
    SOURCE_DIR     "${FETCHCONTENT_BASE_DIR}/JUCE"
    SUBBUILD_DIR   "${FETCHCONTENT_BASE_DIR}/JUCE-Subbuild"
    BINARY_DIR     "${FETCHCONTENT_BASE_DIR}/JUCE-Build")

FetchContent_MakeAvailable(juce)


### Plugin Project Setup ###

# If you are building a VST2 or AAX plugin, CMake needs to be told where to find these SDKs.
# This setup should be done before calling `juce_add_plugin`.

juce_set_vst2_sdk_path("${PROJECT_SOURCE_DIR}/vstsdk2.4")
# juce_set_aax_sdk_path(...)

# `juce_add_plugin` adds a static library target with the name passed as the first argument.
# This target is a normal CMake target, but has a lot of extra properties set
# up by default. As well as this shared code static library, this function adds targets for each of
# the formats specified by the FORMATS arguments. This function accepts many optional arguments.
# Check the readme at `docs/CMake API.md` in the JUCE repo for the full list.

juce_add_plugin("${PROJECT_NAME}"
	FORMATS AU VST VST3 LV2                 # The formats to build. Valid formats: Standalone Unity VST3 AU AUv3 AAX VST LV2.
                                                # AU and AUv3 plugins will only be enabled when building on macOS.
    PRODUCT_NAME "JC303"                        # The name of the final executable, which can differ from the target name.
    PLUGIN_NAME "JC303"                         # Name of the plugin that will be displayed in the DAW. Can differ from PRODUCT_NAME.

    # ICON_BIG                                  # ICON_* arguments specify a path to an image file to use as an icon for the Standalone.
    # ICON_SMALL
    COMPANY_NAME midilab                        # The name of this target's author. The value is inherited from JUCE_COMPANY_NAME.
    COMPANY_WEBSITE "https://midilab.co"        # The address of a website related to this target in some way.
                                                # The value is inherited from JUCE_COMPANY_WEBSITE.
    COMPANY_EMAIL "contact@midilab.co"          # An email address for this target's author. The value is inherited from JUCE_COMPANY_EMAIL.
    PLUGIN_MANUFACTURER_CODE Mlab               # A four-character manufacturer id with at least one upper-case character.
                                                # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case.
    PLUGIN_CODE J303                            # A unique four-character plugin id with exactly one upper-case character.
                                                # GarageBand 10.3 requires the first letter to be upper-case, and the remaining letters to be lower-case.

    IS_SYNTH TRUE                               # Is this a synth or an effect?
    NEEDS_MIDI_INPUT TRUE                       # Does the plugin need midi input?
    NEEDS_MIDI_OUTPUT FALSE                     # Does the plugin need midi output?
    IS_MIDI_EFFECT FALSE                        # Is this plugin a MIDI effect?
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE           # Does the editor need keyboard focus?

    LV2URI http://github.com/midilab/JC303/

    VST3_CATEGORIES "Generator Instrument"      # Should be one or more, separated by spaces, of the following:
                                                # Fx, Instrument, Analyzer, Delay, Distortion, Drum, Dynamics, EQ, External,
                                                # Filter, Generator, Mastering, Modulation, Mono, Network, NoOfflineProcess,
                                                # OnlyOfflineProcess, OnlyRT, Pitch Shift, Restoration, Reverb, Sampler, Spatial,
                                                # Stereo, Surround, Synth, Tools, Up-Downmix
    AU_MAIN_TYPE "kAudioUnitType_Generator kAudioUnitType_MusicDevice" # Should be one or more, separated by spaces, of the following:
                                                # kAudioUnitType_Effect, kAudioUnitType_FormatConverter, kAudioUnitType_Generator,
                                                # kAudioUnitType_MIDIProcessor, kAudioUnitType_Mixer, kAudioUnitType_MusicDevice,
                                                # kAudioUnitType_MusicEffect, kAudioUnitType_OfflineEffect, kAudioUnitType_Output,
                                                # kAudioUnitType_Panner

    COPY_PLUGIN_AFTER_BUILD FALSE)              # Should the plugin be installed to a default location after building?
                                                # NOTE: Writing to the default install locations might require administrator privileges on Windows.

# Set the C++ language standard requirenment for the "shared code" library target.
# Setting this to PUBLIC ensures that all dependent targets will inherit the specified C++ standard.
target_compile_features("${PROJECT_NAME}" PUBLIC cxx_std_17)

# Disable compiler extensions for the project targets (e.g. use -std=c++17 instead of -std=gnu++17).
get_property(project_targets DIRECTORY "${PROJECT_SOURCE_DIR}" PROPERTY BUILDSYSTEM_TARGETS)

set_target_properties(${project_targets} PROPERTIES CXX_EXTENSIONS OFF)

# Add the subdirectory with source files.
add_subdirectory(src)
include_directories(src)

# `juce_generate_juce_header` will create a JuceHeader.h for a given target, which will be generated
# into your build tree. The include path for this header will be automatically added to the target.
# NOTE: JuceHeader.h is generated when the target is built.

juce_generate_juce_header("${PROJECT_NAME}")

# `target_compile_definitions` adds some preprocessor definitions to our target.
# JUCE modules also make use of compile definitions to switch certain features on/off,
# so if there's a particular feature you need that's not on by default, check the module header
# for the correct flag to set here.
# These definitions will be visible both to your code, and also the JUCE module code.

target_compile_definitions("${PROJECT_NAME}"
    PUBLIC
        # JUCE_WEB_BROWSER and JUCE_USE_CURL would be on by default, but you might not need them.
        JUCE_WEB_BROWSER=0  # If you remove this, add `NEEDS_WEB_BROWSER TRUE` to the `juce_add_plugin` call
        JUCE_USE_CURL=0     # If you remove this, add `NEEDS_CURL TRUE` to the `juce_add_plugin` call
        JUCE_VST3_CAN_REPLACE_VST2=0)

# If your target needs extra binary assets, you can add them here.
# NOTE: Conversion to binary-data happens when the target is built.

# juce_add_binary_data(Assets       # Name of a static library target that will include all the binary resources
    # HEADER_NAME ...               # Name of the generated header. "BinaryData.h" is used by default
    # NAMESPACE ...                 # Namespace of the generated binary data class. "BinaryData" is used by default
#   SOURCES ...)                    # List of asset files of any kind that should be built into the static library

# `target_link_libraries` links libraries and JUCE modules to other libraries or executables. Here,
# we're linking our executable target to the `juce::juce_audio_utils` module. Inter-module
# dependencies are resolved automatically.

target_link_libraries("${PROJECT_NAME}"
    PRIVATE
        # Assets                # If we'd created a binary data target, we'd link to it here
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)


### IDE Generator post-config ###

# IDEs:  Place source groups for project targets into a "Targets" folder (to reduce visual clutter).
# Xcode: Generate build schemes for project targets.
set_target_properties(${project_targets} PROPERTIES
    FOLDER "Targets"
    XCODE_GENERATE_SCHEME ON)

# IDEs:  Move the "shared code" source group (main target) out of the "Targets" folder
#        to the top level.
set_target_properties("${PROJECT_NAME}" PROPERTIES FOLDER "")

# IDEs:  Organise source file grouping.
get_target_property(project_sources "${PROJECT_NAME}" SOURCES)

#   If JuceHeader.h is generated, remove it from the source file list and handle it individually.
get_target_property(juce_library_code "${PROJECT_NAME}" JUCE_GENERATED_SOURCES_DIRECTORY)
set(juce_header "${juce_library_code}/JuceHeader.h")

list(REMOVE_ITEM project_sources "${juce_header}")

#   Place JuceHeader.h and the associated CMake Rule file into a "JUCE Library Code" folder.
source_group("JUCE Library Code" FILES "${juce_header}")
source_group("JUCE Library Code/CMake Rules" FILES "${juce_header}.rule")

#   Generate source groups that follow the organisation of source file directories.
source_group(TREE "${PROJECT_SOURCE_DIR}/src" FILES ${project_sources})

#   Place Binary Data related source files into the root of their target folder.
if(TARGET Assets)
    get_target_property(project_assets Assets SOURCES)
    source_group("" FILES ${project_assets})
endif()

