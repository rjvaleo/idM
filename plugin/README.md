# M Classic — plugin shell

JUCE 9 host for the M Classic engine. One target yields **AU**, **VST3** and
**Standalone**; CLAP follows via `clap-juce-extensions`.

Nothing of the engine is here yet. What lives in `src/` is a scaffold: an
instrument that accepts MIDI, emits none, and paints an empty 1000 x 460 panel.
Its only job is to prove the toolchain produces loadable bundles before the
Rust engine (M2) and the webview UI (M3) arrive.

## Layout

    plugin/
      CMakeLists.txt     target definition
      JUCE/              git submodule, pinned to 9.0.1, shallow
      src/               PluginProcessor / PluginEditor scaffold
      build/             ignored

## Building

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

## Version note

Pinned to JUCE **9.0.1**. Both 8.0.15 and 9.0.1 were built and validated here;
9 was chosen for `@juce-framework/webview`, the typed npm package for the
WebBrowserComponent bridge that the UI depends on. See D2 in `PLUGIN_PLAN.md`.
