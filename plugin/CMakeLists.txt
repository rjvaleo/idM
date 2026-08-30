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
  tests/ProjectLoader.cpp)

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

target_sources(MClassicPlugin PRIVATE
  src/PluginProcessor.cpp
  src/PluginEditor.cpp)

target_compile_definitions(MClassicPlugin PUBLIC
  JUCE_WEB_BROWSER=1
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MClassicPlugin
  PRIVATE
    MClassicEngine
    MClassicUI
    juce::juce_audio_utils
    juce::juce_gui_extra
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
