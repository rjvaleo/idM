cmake_minimum_required(VERSION 3.22)

project(MClassic VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

add_subdirectory(JUCE)

# The UI is the browser app's own single-file build, inlined whole: one HTML
# document carrying the React bundle, the CSS and the fonts. Nothing about the
# windows is reimplemented here.
set(MCLASSIC_UI_HTML "${CMAKE_CURRENT_SOURCE_DIR}/../dist-single/index.html")

if(NOT EXISTS "${MCLASSIC_UI_HTML}")
  message(FATAL_ERROR
    "The UI bundle is missing:\n  ${MCLASSIC_UI_HTML}\n"
    "Build it first, from the repository root:\n  npm run build:single")
endif()

juce_add_binary_data(MClassicUI
  NAMESPACE   MClassicUI
  HEADER_NAME MClassicUI.h
  SOURCES     "${MCLASSIC_UI_HTML}")

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
  NEEDS_WEB_BROWSER         TRUE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD   TRUE)

target_sources(MClassicPlugin PRIVATE
  src/PluginProcessor.cpp
  src/PluginEditor.cpp)

target_compile_definitions(MClassicPlugin PUBLIC
  JUCE_WEB_BROWSER=1
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MClassicPlugin
  PRIVATE
    MClassicUI
    juce::juce_audio_utils
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
