# Where idM's MIDI works, and where it is untested

idM generates MIDI. Everything here is about whether that MIDI reaches
anything, which is a property of the host as much as of the plugin.

Nothing in this file is a guess. Anything unverified says so.

## The thing that makes this hard

Both plugin formats hand MIDI to the host through a gate the host controls and
**never reports**:

| Format | Gate | Source |
|---|---|---|
| VST3 | sends only when `isMidiOutputBusEnabled` — set when the host activates the event output bus | `JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp:3630` |
| AU | sends only when the host has installed a MIDI output callback | `..._AU_1.mm:2219` |

When a host declines, the notes are dropped inside the wrapper. There is no
error, no log line and nothing on screen. An engine that is running perfectly
and a host that is ignoring it look identical.

That is why the plugin publishes a **virtual MIDI port** of its own, and why the
interface carries a **notes-sent counter**. If the counter climbs and the DAW is
silent, the fault is the routing, not the music.

## Shape

Settled, and not to be relitigated without reading the sources above:

- `IS_SYNTH=1`, `ProducesMidiOutput=1`, **`IS_MIDI_EFFECT=0`**.
- A VST3 built with `IS_MIDI_EFFECT` has no audio buses, and Ableton rejects it:
  its log says *"plugin processor successfully loaded"* then *"Failed"*. The JUCE
  forum's guidance is the same — tick MIDI Input and MIDI Output, do not tick
  MIDI Effect.
- The `aumi` AU MIDI-processor build exists for Logic's MIDI FX slot only, and is
  built as **AU only** for that reason.

## Builds

macOS binaries are **universal** — `arm64` and `x86_64` in one bundle, minimum
macOS 11.0 — and the Intel slice is checked rather than assumed: the conformance
runner built universal and run under Rosetta (`arch -x86_64`) reports the same
13,225 values as the native one.

The AU plugin codes are `idMa` (instrument) and `idMm` (MIDI FX). Both pass
`auval`. Both begin with a lower-case letter, which departs from the GarageBand
10.3 convention JUCE's docs mention; if a GarageBand report ever comes back
showing the plugin missing, that is the first thing to change.

## Formats built

| Format | Built | Validated |
|---|---|---|
| AU (`aumu`, instrument) | yes | `auval -v aumu idMa Rjvl` passes |
| AU (`aumi`, MIDI FX) | yes | `auval -v aumi idMm Rjvl` passes |
| VST3 | yes | loads and emits under JUCE's VST3 host |
| CLAP | yes | builds; **not yet loaded in a CLAP host** |
| Standalone | yes | emits notes and 24-PPQN clock through its own port, heard from another process |

## Hosts

| Host | Format | Status |
|---|---|---|
| Ableton Live 12 (macOS) | AU / VST3 | **loads.** MIDI out requires routing: a second MIDI track, *MIDI From* → the idM track, the chooser below it → **idM**, Monitor **In**. That routing is what makes Live activate the bus. **Confirmed 2026-08-30** — notes reach a synth on a second track. |
| Ableton Live | VST3 MIDI-effect build | **rejected.** Do not ship one. |
| Logic Pro | AU | untested |
| Bitwig, Reaper, Studio One | CLAP / VST3 | untested |
| Cubase, FL Studio | VST3 | untested |
| Windows, any host | VST3 / CLAP | **builds and passes CI**, never run in a DAW. No virtual port there, so the host path is the only route. |
| Linux, any host | VST3 / CLAP | **builds and passes CI**, never run in a DAW. |

## Windows

Two things are known without building it:

1. **There is no virtual-port fallback.** JUCE's own header on
   `MidiOutput::createNewDevice`: *"only available on Linux, macOS and iOS"*
   (`juce_audio_devices/midi_io/juce_MidiDevices.h:345`). On Windows the host
   path is the only route, so it has to work rather than usually work.
2. **VST3 note output is the reliable part of the spec.** Notes work in Live 11+,
   Cubase 2020+, Reaper 2020+ and Studio One 6. CC and pitch bend do not,
   because Steinberg marked them legacy — which is why program changes are
   **off by default** here.

That was written before there was a CI runner. There is one now, and it builds
Windows on every push: MSVC compiles the engine, the engine reproduces all
13,225 conformance values, session state round-trips, and the host suite loads
the built VST3 through JUCE's format manager and confirms it emits across
start, stop, loop, locate, tempo change and bypass.

What remains unverified is the part that has always been the problem: whether a
real DAW on Windows activates the event output bus. No CI runner can answer
that. It needs somebody with Cubase or Reaper open.

## What was learned the expensive way

The plugin was shipped for three days as an AU instrument with no virtual port
and no on-screen status, and it looked completely dead in Ableton while passing
every test written for it. The tests were not wrong; they measured the plugin
writing notes into a buffer, which it did. Nothing measured whether anything
collected that buffer.

Test the seam the user actually stands on.
