# M Classic — plugin shell

JUCE 9 host for the M Classic engine. One target yields **AU**, **VST3** and
**Standalone**; CLAP follows via `clap-juce-extensions`.

The UI is the browser app's own single-file build, served whole to a
`WebBrowserComponent`. Nothing about the windows is reimplemented here, and
nothing should be.

Nothing of the engine is here yet. The processor accepts MIDI and emits none;
the Rust engine arrives in M2.

## Layout

    plugin/
      CMakeLists.txt     target definition
      JUCE/              git submodule, pinned to 9.0.1, shallow
      src/               PluginProcessor / PluginEditor scaffold
      build/             ignored

## Building

The UI bundle must exist before CMake configures; the build fails with a
pointer to this command if it does not.

    npm run build:single
    git submodule update --init --depth 1 plugin/JUCE
    cmake -S plugin -B plugin/build -DCMAKE_BUILD_TYPE=Release
    cmake --build plugin/build -j

`COPY_PLUGIN_AFTER_BUILD` installs into `~/Library/Audio/Plug-Ins/` on macOS, so
Live and `auval` see the result without a further step.

## Requirements

- CMake 3.22+
- A C++20 toolchain
- macOS: Command Line Tools are enough — full Xcode is not needed for AU

## Known toolchain trap on macOS

If any C++ file fails with `'algorithm' file not found`, the Command Line Tools
install has a stale `/Library/Developer/CommandLineTools/usr/include/c++/v1`
shadowing the SDK's complete copy. Compare the two:

    ls /Library/Developer/CommandLineTools/usr/include/c++/v1 | wc -l
    ls "$(xcrun --show-sdk-path)/usr/include/c++/v1" | wc -l

A stale directory holds a few dozen entries against the SDK's ~190. Remove it:

    sudo rm -rf /Library/Developer/CommandLineTools/usr/include/c++

This is a machine fault, not a project one — every C++ build on the host fails
the same way. There is deliberately no workaround baked into `CMakeLists.txt`.

## Validating the AU

    rm -rf ~/Library/Audio/Plug-Ins/Components/"M Classic.component"
    cmake --build plugin/build -j
    auval -v aumu Mcls Rjvl

**Clear the installed component first.** `auval` validates whatever is in
`~/Library/Audio/Plug-Ins/Components`, not what you just built. A failed build
leaves the previous bundle in place, and the run reports
`AU VALIDATION SUCCEEDED` for code that no longer compiles.

## Proving the UI renders

`MCLASSIC_UI_PROBE` compiles in a probe that queries the live DOM once the
webview settles, and writes what it finds to the file named by
`MCLASSIC_UI_PROBE_OUT`. It is compiled out of ordinary builds.

    cmake -S plugin -B plugin/build-probe -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_FLAGS=-DMCLASSIC_UI_PROBE=1
    cmake --build plugin/build-probe --target MClassicPlugin_Standalone -j
    open --env MCLASSIC_UI_PROBE_OUT=/tmp/probe.txt \
      "plugin/build-probe/MClassicPlugin_artefacts/Release/Standalone/M Classic.app"
    cat /tmp/probe.txt

**Launch with `open`, not by running the binary.** Executing
`M Classic.app/Contents/MacOS/M Classic` straight from a shell exits after a
second without ever constructing the editor, silently and with status 0. Going
through LaunchServices gives it the GUI session it needs.

Recorded on 2026-08-28, macOS 26, JUCE 9.0.1:

    windowCount 8
    windows     Patterns a, Untitled, Snapshot, Variables,
                Cyclic Variables, Midi, Midi View, Pattern Editor
    viewport    1000x460
    scrollable  1000x460      (no overflow)
    occupied    984x423 at (4, 27)
    nodes       2439

`.ustage` reports 640x480 throughout. That is the classic Mac screen the window
coordinates are expressed in, not the panel size, and windows sit outside it by
design.

## Version note

Pinned to JUCE **9.0.1**. Both 8.0.15 and 9.0.1 were built and validated here;
9 was chosen for `@juce-framework/webview`, the typed npm package for the
WebBrowserComponent bridge that the UI depends on. See D2 in `PLUGIN_PLAN.md`.
