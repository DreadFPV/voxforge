cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary" FORCE)
endif()

project(VoxForge VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: use a local checkout (-DVOXFORGE_JUCE_PATH=...) or fetch it
set(VOXFORGE_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout")
if(VOXFORGE_JUCE_PATH)
    add_subdirectory(${VOXFORGE_JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(VOXFORGE_FORMATS VST3 AU)
if(NOT APPLE)
    set(VOXFORGE_FORMATS VST3)
endif()

juce_add_plugin(VoxForge
    COMPANY_NAME "JustCreateit"
    COMPANY_WEBSITE "https://justcreateit.ch"
    BUNDLE_ID "ch.justcreateit.voxforge"
    PLUGIN_MANUFACTURER_CODE Jcit
    PLUGIN_CODE Vxfg
    PRODUCT_NAME "VoxForge"
    FORMATS ${VOXFORGE_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Pitch\ Shift Dynamics
    AU_MAIN_TYPE kAudioUnitType_MusicEffect)

target_sources(VoxForge PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/Presets.cpp)

target_include_directories(VoxForge PRIVATE Source)

target_compile_definitions(VoxForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VoxForge
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline DSP tests (no JUCE)
option(VOXFORGE_BUILD_TESTS "Build offline DSP tests" ON)
if(VOXFORGE_BUILD_TESTS)
    add_executable(VoxForgeTests tests/OfflineTest.cpp)
    target_include_directories(VoxForgeTests PRIVATE Source)
endif()
