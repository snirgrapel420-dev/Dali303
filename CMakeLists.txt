cmake_minimum_required(VERSION 3.22)

project(Dali303 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)   # universal binary
endif()

option(DALI303_DEV_MODE    "Show Developer Test Mode (engine isolation + CPU) in the GUI" OFF)
option(DALI303_BUILD_TOOLS "Build the offline DSP test tool (DaliRender)" ON)
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (empty = download JUCE 8.0.4)")

# ---------------------------------------------------------------- JUCE
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------- Plugin
juce_add_plugin(Dali303
    COMPANY_NAME                "Dali Audio"
    COMPANY_WEBSITE             "https://daliaudio.com"
    BUNDLE_ID                   com.daliaudio.dali303
    PLUGIN_MANUFACTURER_CODE    Dali
    PLUGIN_CODE                 D303
    PRODUCT_NAME                "Dali303"
    DESCRIPTION                 "A Living Acid Machine"
    FORMATS                     VST3 Standalone
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Synth
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(Dali303 PRIVATE
    Source/DSP/AcidEngine.cpp
    Source/Sequencer/Pattern.cpp
    Source/Presets/FactoryPresets.cpp
    Source/Presets/Discover.cpp
    Source/Presets/PresetManager.cpp
    Source/Plugin/Parameters.cpp
    Source/Plugin/PluginProcessor.cpp
    Source/UI/DaliLookAndFeel.cpp
    Source/UI/DaliKnob.cpp
    Source/UI/PresetBar.cpp
    Source/UI/SequencerPanel.cpp
    Source/UI/PluginEditor.cpp)

target_include_directories(Dali303 PRIVATE Source)

target_compile_definitions(Dali303 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    DALI303_DEV_MODE=$<BOOL:${DALI303_DEV_MODE}>)

target_link_libraries(Dali303
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- DSP test tool (no JUCE)
if(DALI303_BUILD_TOOLS)
    add_executable(DaliRender
        Tools/DaliRender.cpp
        Source/DSP/AcidEngine.cpp
        Source/Sequencer/Pattern.cpp
        Source/Presets/FactoryPresets.cpp
        Source/Presets/Discover.cpp)
    target_include_directories(DaliRender PRIVATE Source)
endif()
