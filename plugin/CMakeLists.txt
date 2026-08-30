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

project(Idm VERSION 0.1.0 LANGUAGES C CXX)

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
set(IDM_UI_HTML "${CMAKE_CURRENT_SOURCE_DIR}/../dist-single/index.html")

if(NOT EXISTS "${IDM_UI_HTML}")
  message(FATAL_ERROR
    "The UI bundle is missing:\n  ${IDM_UI_HTML}\n"
    "Build it first, from the repository root:\n  npm run build:single")
endif()

# ---------------------------------------------------------------------------
# The engine, in C++, and the gate that keeps it honest.
#
# The same fixtures the TypeScript engine emits. They are plain text, so the
# check is a comparison of values rather than of implementations.
# ---------------------------------------------------------------------------

add_library(IdmEngine STATIC
  engine/Random.cpp
  engine/TimeMap.cpp
  engine/Music.cpp
  engine/Cyclic.cpp
  engine/Transform.cpp
  engine/Planner.cpp
  engine/Events.cpp
  engine/Project.cpp)

target_include_directories(IdmEngine PUBLIC engine)

# The engine must agree with the TypeScript bit for bit, and the default here
# does not. Clang contracts `a + b * c` into a fused multiply-add, which rounds
# once where JavaScript rounds twice — worth one unit in the last place, which
# is exactly what the conformance runner caught in `interpolate`. PUBLIC,
# because the headers carry inline arithmetic that consumers compile
# themselves.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(IdmEngine PUBLIC -ffp-contract=off)
elseif(MSVC)
  target_compile_options(IdmEngine PUBLIC /fp:precise)
endif()

add_executable(IdmConformance
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
target_link_libraries(IdmConformance PRIVATE IdmEngine juce::juce_core)

target_compile_features(IdmConformance PRIVATE cxx_std_20)

target_compile_definitions(IdmConformance PRIVATE
  IDM_GOLDENS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../src/engine/__goldens__")

enable_testing()
add_test(NAME conformance COMMAND IdmConformance)

juce_add_binary_data(IdmUI
  NAMESPACE   IdmUI
  HEADER_NAME IdmUI.h
  SOURCES     "${IDM_UI_HTML}")

juce_add_plugin(IdmPlugin
  PRODUCT_NAME              "idM"
  COMPANY_NAME              "rjvaleo"
  BUNDLE_ID                 "com.rjvaleo.idm"
  PLUGIN_MANUFACTURER_CODE  Rjvl
  PLUGIN_CODE               idMa
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

juce_add_plugin(IdmMidiPlugin
  PRODUCT_NAME              "idM MIDI"
  COMPANY_NAME              "rjvaleo"
  BUNDLE_ID                 "com.rjvaleo.idmmidi"
  PLUGIN_MANUFACTURER_CODE  Rjvl
  PLUGIN_CODE               idMm
  FORMATS                   AU
  IS_SYNTH                  FALSE
  NEEDS_MIDI_INPUT          TRUE
  NEEDS_MIDI_OUTPUT         TRUE
  IS_MIDI_EFFECT            TRUE
  NEEDS_WEB_BROWSER         TRUE
  EDITOR_WANTS_KEYBOARD_FOCUS TRUE
  COPY_PLUGIN_AFTER_BUILD   TRUE)

target_sources(IdmMidiPlugin PRIVATE
  src/Diagnostics.cpp
  src/ProjectJson.cpp
  src/PopOutWindow.cpp
  src/PluginProcessor.cpp
  src/PluginEditor.cpp)

target_compile_definitions(IdmMidiPlugin PUBLIC
  JUCE_WEB_BROWSER=1
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(IdmMidiPlugin
  PRIVATE
    IdmEngine
    IdmUI
    juce::juce_audio_devices
    juce::juce_audio_utils
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

target_sources(IdmPlugin PRIVATE
  src/Diagnostics.cpp
  src/ProjectJson.cpp
  src/PopOutWindow.cpp
  src/PluginProcessor.cpp
  src/PluginEditor.cpp)

target_compile_definitions(IdmPlugin PUBLIC
  JUCE_WEB_BROWSER=1
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(IdmPlugin
  PRIVATE
    IdmEngine
    juce::juce_audio_devices
    IdmUI
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

juce_add_console_app(IdmHostTest PRODUCT_NAME "IdmHostTest")

target_sources(IdmHostTest PRIVATE tests/HostTest.cpp)

target_compile_definitions(IdmHostTest PRIVATE
  JUCE_PLUGINHOST_VST3=1
  JUCE_PLUGINHOST_AU=1
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  IDM_ARTEFACTS_DIR="${CMAKE_CURRENT_BINARY_DIR}/IdmPlugin_artefacts/Release"
  IDM_GOLDENS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../src/engine/__goldens__")

target_link_libraries(IdmHostTest PRIVATE
  juce::juce_audio_utils
  juce::juce_recommended_config_flags)

add_dependencies(IdmHostTest IdmPlugin_VST3 IdmPlugin_AU)

# ---------------------------------------------------------------------------
# Session persistence, against the processor itself.
#
# A hosted instance's setStateInformation goes through the format wrapper,
# which frames the blob its own way — so testing ours through it would test the
# wrapper's error handling instead.
# ---------------------------------------------------------------------------

juce_add_console_app(IdmStateTest PRODUCT_NAME "IdmStateTest")

target_sources(IdmStateTest PRIVATE
  tests/StateTest.cpp
  src/Diagnostics.cpp
  src/PluginProcessor.cpp
  src/ProjectJson.cpp)

target_compile_definitions(IdmStateTest PRIVATE
  IDM_NO_EDITOR=1
  JUCE_USE_CURL=0
  JUCE_WEB_BROWSER=0
  IDM_GOLDENS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../src/engine/__goldens__")

target_link_libraries(IdmStateTest PRIVATE
  IdmEngine
  juce::juce_audio_devices
  juce::juce_audio_processors
  juce::juce_recommended_config_flags)

# Lists MIDI ports, so the standalone's virtual output can be seen from outside
# the process that created it.
juce_add_console_app(IdmMidiPorts PRODUCT_NAME "IdmMidiPorts")
target_sources(IdmMidiPorts PRIVATE tests/MidiPorts.cpp)
target_compile_definitions(IdmMidiPorts PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(IdmMidiPorts PRIVATE
  juce::juce_audio_devices
  juce::juce_recommended_config_flags)

# Opens the standalone's virtual port from outside, which is the only way to
# show that notes actually leave the process.
juce_add_console_app(IdmMidiListen PRODUCT_NAME "IdmMidiListen")
target_sources(IdmMidiListen PRIVATE tests/MidiListen.cpp)
target_compile_definitions(IdmMidiListen PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(IdmMidiListen PRIVATE
  juce::juce_audio_devices
  juce::juce_recommended_config_flags)

clap_juce_extensions_plugin(TARGET IdmPlugin
  CLAP_ID "com.rjvaleo.idm"
  CLAP_FEATURES instrument "note effect" sequencer generative algorithmic)
