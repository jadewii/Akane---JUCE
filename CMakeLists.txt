cmake_minimum_required(VERSION 3.15)

project(WiiPluckUltimate VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set macOS deployment target to 10.13 (compatible with most Macs)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS deployment version")

# Add JUCE subdirectory if it exists, otherwise find it
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
    add_subdirectory(JUCE)
else()
    find_package(JUCE CONFIG REQUIRED)
endif()

# Create the plugin
juce_add_plugin(WiiPluckUltimate
    COMPANY_NAME "Wiistruments"
    PLUGIN_MANUFACTURER_CODE Wiii
    PLUGIN_CODE Wplk
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "AKANE"
    
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD TRUE
    
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
)

# Add source files
target_sources(WiiPluckUltimate PRIVATE
    Source/PluginProcessor.cpp
    Source/UltimatePluckEngine.h
    Source/UltimatePluckProcessor.h
)

# Compiler definitions
target_compile_definitions(WiiPluckUltimate PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_MODAL_LOOPS_PERMITTED=1
)

# Link JUCE modules
target_link_libraries(WiiPluckUltimate PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# Warning flags
if(MSVC)
    target_compile_options(WiiPluckUltimate PRIVATE /W4)
else()
    target_compile_options(WiiPluckUltimate PRIVATE -Wall -Wextra -Wpedantic)
endif()

message(STATUS "========================================")
message(STATUS "WiiPluck Ultimate Configuration")
message(STATUS "========================================")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Formats: VST3, AU, Standalone")
message(STATUS "========================================")
