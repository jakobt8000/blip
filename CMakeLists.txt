cmake_minimum_required(VERSION 3.22)

project(Blip VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(BLIP_FORMATS VST3 AU)
else()
    set(BLIP_FORMATS VST3)
endif()

juce_add_plugin(Blip
    COMPANY_NAME "Jakob2ft"
    BUNDLE_ID com.jakob2ft.blip
    PLUGIN_MANUFACTURER_CODE Jk2f
    PLUGIN_CODE Blp1
    FORMATS ${BLIP_FORMATS}
    PRODUCT_NAME "Blip"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

juce_add_binary_data(BlipData
    SOURCES
        Resources/IBMPlexMono-Regular.ttf
        Resources/IBMPlexMono-SemiBold.ttf)

target_sources(Blip PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Blip PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Blip
    PRIVATE
        BlipData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Lokal test (bruges ikke af Mac-bygningen)
option(BLIP_TESTS "Byg test-program" OFF)
if(BLIP_TESTS)
    juce_add_console_app(BlipRenderTest PRODUCT_NAME "BlipRenderTest")
    target_sources(BlipRenderTest PRIVATE tests/RenderTest.cpp)
    target_compile_definitions(BlipRenderTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(BlipRenderTest PRIVATE Blip juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()
