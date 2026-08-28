cmake_minimum_required(VERSION 3.22)

project(MClassic VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

add_subdirectory(JUCE)

# Scaffold only. This proves the toolchain builds a loadable AU/VST3/Standalone
# on this machine; the engine and the webview UI arrive in M2 and M3.
juce_add_plugin(MClassicPlugin
  PRODUCT_NAME              "M Classic"
  COMPANY_NAME              "rjvaleo"
  BUNDLE_ID                 "com.rjvaleo.mclassic"
  PLUGIN_MANUFACTURER_CODE  Rjvl
  PLUGIN_CODE               Mcls
  FORMATS                   AU VST3 Standalone
  IS_SYNTH                  TRUE
  NEEDS_MIDI_INPUT          TRUE
  NEEDS_MIDI_OUTPUT         TRUE
  IS_MIDI_EFFECT            FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD   TRUE)

target_sources(MClassicPlugin PRIVATE
  src/PluginProcessor.cpp
  src/PluginEditor.cpp)

target_compile_definitions(MClassicPlugin PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MClassicPlugin
  PRIVATE
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
