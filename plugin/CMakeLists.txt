cmake_minimum_required(VERSION 3.22)

# Both of these have to come before project(), and the deployment target used
# to sit after it. CMake creates these cache entries while it works out the
# compiler, and a later set(... CACHE ...) is a no-op against an entry that
# already exists — so the line read 11.0 while the binaries it produced were
# stamped with the SDK's own minimum, macOS 26. Anything moved below project()
# here silently stops taking effect.
#
# arm64;x86_64 is JUCE's documented way to build universal — "CMake API.md",
# "Building universal binaries for macOS". Both slices are wanted: Intel Macs
# run plenty of DAWs, and a host under Rosetta will only load an x86_64 plugin.
if(CMAKE_HOST_APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architectures to build")
endif()

project(MClassic VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

# CLAP, from the same sources as everything else.
#
# Worth the extra target because CLAP's note output is first-class rather than
# a legacy afterthought: VST3 carries notes reliably but Steinberg marked CC and
# pitch bend legacy, and hosts implement them inconsistently. Bitwig, Reaper and
# Studio One take CLAP natively.
add_subdirectory(clap-juce-extensions EXCLUDE_FROM_ALL)

# The UI is the browser app's own single-file build, inlined whole: one HTML
# document carrying the React bundle, the CSS and the fonts. Nothing about the
# windows is reimplemented here.
set(MCLASSIC_UI_HTML "${CMAKE_CURRENT_SOURCE_DIR}/../dist-single/index.html")

if(NOT EXISTS "${MCLASSIC_UI_HTML}")
  message(FATAL_ERROR
    "The UI bundle is missing:\n  ${MCLASSIC_UI_HTML}\n"
    "Build it first, from the repository root:\n  npm run build:single")
endif()

# ---------------------------------------------------------------------------
# The engine, in C++, and the gate that keeps it honest.
#
# The same fixtures the TypeScript engine emits. They are plain text, so the
# check is a comparison of values rather than of implementations.
# ---------------------------------------------------------------------------

add_library(MClassicEngine STATIC
  engine/Random.cpp
  engine/TimeMap.cpp
  engine/Music.cpp
  engine/Cyclic.cpp
  engine/Transform.cpp
  engine/Planner.cpp
  engine/Events.cpp
  engine/Project.cpp)

target_include_directories(MClassicEngine PUBLIC engine)

# The engine must agree with the TypeScript bit for bit, and the default here
# does not. Clang contracts `a + b * c` into a fused multiply-add, which rounds
# once where JavaScript rounds twice — worth one unit in the last place, which
# is exactly what the conformance runner caught in `interpolate`. PUBLIC,
# because the headers carry inline arithmetic that consumers compile
# themselves.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(MClassicEngine PUBLIC -ffp-contract=off)
elseif(MSVC)
  target_compile_options(MClassicEngine PUBLIC /fp:precise)
endif()

add_executable(MClassicConformance
  tests/Runner.cpp
  tests/Harness.cpp
  tests/CheckRandom.cpp
  tests/CheckTimeMap.cpp
  tests/CheckMusic.cpp
  tests/CheckCyclic.cpp
  tests/CheckTransform.cpp
  tests/CheckPlanner.cpp
  tests/CheckEvents.cpp
  tests/ProjectLoader.cpp
  src/ProjectJson.cpp)

# juce_core only, for its JSON parser. The engine itself stays free of JUCE.
target_link_libraries(MClassicConformance PRIVATE MClassicEngine juce::juce_core)

target_compile_features(MClassicConformance PRIVATE cxx_std_20)

target_compile_definitions(MClassicConformance PRIVATE
  MCLASSIC_GOLDENS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../src/engine/__goldens__")

enable_testing()
add_test(NAME conformance COMMAND MClassicConformance)

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


# ---------------------------------------------------------------------------
# The same plugin as an AU MIDI processor, for Logic.
#
# AU only, deliberately. A VST3 built with IS_MIDI_EFFECT has no audio buses,
# and Ableton rejects it outright — its log says "plugin processor successfully
# loaded" and then "Failed". The JUCE forum's guidance is the same: for MIDI out
# in Live, tick MIDI Input and MIDI Output and do NOT tick MIDI Effect.
#
# Live takes MIDI from the instrument build instead, through track routing:
# a second MIDI track with MIDI From set to the plugin's track, the plugin
# chosen in the chooser below it, and Monitor set to In. That is Ableton's own
# documented mechanism, and it needs an instrument, not a MIDI effect.
# ---------------------------------------------------------------------------

juce_add_plugin(MClassicMidiPlugin
  PRODUCT_NAME              "M Classic MIDI"
  COMPANY_NAME              "rjvaleo"
  BUNDLE_ID                 "com.rjvaleo.mclassicmidi"
  PLUGIN_MANUFACTURER_CODE  Rjvl
  PLUGIN_CODE               Mclm
  FORMATS                   AU
  IS_SYNTH                  FALSE
  NEEDS_MIDI_INPUT          TRUE
  NEEDS_MIDI_OUTPUT         TRUE
  IS_MIDI_EFFECT            TRUE
  NEEDS_WEB_BROWSER         TRUE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD   TRUE)

target_sources(MClassicMidiPlugin PRIVATE
  src/Diagnostics.cpp
  src/ProjectJson.cpp
  src/PopOutWindow.cpp
  src/PluginProcessor.cpp
  src/PluginEditor.cpp)

target_compile_definitions(MClassicMidiPlugin PUBLIC
  JUCE_WEB_BROWSER=1
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MClassicMidiPlugin
  PRIVATE
    MClassicEngine
    MClassicUI
    juce::juce_audio_devices
    juce::juce_audio_utils
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

target_sources(MClassicPlugin PRIVATE
  src/Diagnostics.cpp
  src/ProjectJson.cpp
  src/PopOutWindow.cpp
  src/PluginProcessor.cpp
  src/PluginEditor.cpp)

target_compile_definitions(MClassicPlugin PUBLIC
  JUCE_WEB_BROWSER=1
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MClassicPlugin
  PRIVATE
    MClassicEngine
    juce::juce_audio_devices
    MClassicUI
    juce::juce_audio_utils
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# A host, so "it works in a DAW" is measured rather than assumed.
#
# Loads the built bundle through JUCE's plugin format managers - the same path
# Live and Logic take - drives a transport at it, and reports the MIDI it emits.
# ---------------------------------------------------------------------------

juce_add_console_app(MClassicHostTest PRODUCT_NAME "MClassicHostTest")

target_sources(MClassicHostTest PRIVATE tests/HostTest.cpp)

target_compile_definitions(MClassicHostTest PRIVATE
  JUCE_PLUGINHOST_VST3=1
  JUCE_PLUGINHOST_AU=1
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  MCLASSIC_ARTEFACTS_DIR="${CMAKE_CURRENT_BINARY_DIR}/MClassicPlugin_artefacts/Release"
  MCLASSIC_GOLDENS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../src/engine/__goldens__")

target_link_libraries(MClassicHostTest PRIVATE
  juce::juce_audio_utils
  juce::juce_recommended_config_flags)

add_dependencies(MClassicHostTest MClassicPlugin_VST3 MClassicPlugin_AU)

# ---------------------------------------------------------------------------
# Session persistence, against the processor itself.
#
# A hosted instance's setStateInformation goes through the format wrapper,
# which frames the blob its own way — so testing ours through it would test the
# wrapper's error handling instead.
# ---------------------------------------------------------------------------

juce_add_console_app(MClassicStateTest PRODUCT_NAME "MClassicStateTest")

target_sources(MClassicStateTest PRIVATE
  tests/StateTest.cpp
  src/Diagnostics.cpp
  src/PluginProcessor.cpp
  src/ProjectJson.cpp)

target_compile_definitions(MClassicStateTest PRIVATE
  MCLASSIC_NO_EDITOR=1
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  MCLASSIC_GOLDENS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../src/engine/__goldens__")

target_link_libraries(MClassicStateTest PRIVATE
  MClassicEngine
  juce::juce_audio_devices
  juce::juce_audio_processors
  juce::juce_recommended_config_flags)

# Lists MIDI ports, so the standalone's virtual output can be seen from outside
# the process that created it.
juce_add_console_app(MClassicMidiPorts PRODUCT_NAME "MClassicMidiPorts")
target_sources(MClassicMidiPorts PRIVATE tests/MidiPorts.cpp)
target_compile_definitions(MClassicMidiPorts PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(MClassicMidiPorts PRIVATE
  juce::juce_audio_devices
  juce::juce_recommended_config_flags)

# Opens the standalone's virtual port from outside, which is the only way to
# show that notes actually leave the process.
juce_add_console_app(MClassicMidiListen PRODUCT_NAME "MClassicMidiListen")
target_sources(MClassicMidiListen PRIVATE tests/MidiListen.cpp)
target_compile_definitions(MClassicMidiListen PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(MClassicMidiListen PRIVATE
  juce::juce_audio_devices
  juce::juce_recommended_config_flags)

clap_juce_extensions_plugin(TARGET MClassicPlugin
  CLAP_ID "com.rjvaleo.mclassic"
  CLAP_FEATURES instrument "note effect" sequencer generative algorithmic)
