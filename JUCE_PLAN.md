# Moving idM to JUCE

The plugin is JUCE and C++. The engine moves into it. Nothing else.

Superseding note: `PLUGIN_PLAN.md` D1 said the engine ports to Rust. It does
not. The Rust work is parked at `engine-rust-parked/` and is referenced by
nothing in the build.

## Where it stands

| | |
|---|---|
| JUCE shell | **done** — AU, VST3, Standalone build; `auval` passes |
| The real UI | **done** — the browser build's own interface, served to a `WebBrowserComponent` at 1000 × 460 |
| MIDI out of `processBlock` | **done** — host playhead in, notes into the host's `MidiBuffer` at sample offsets |
| Stuck-note paths | **done** — stop, loop wrap, locate, bypass |
| `Num`, `Random`, `TimeMap` | **done** in `plugin/engine/`, verified against the TypeScript |
| The engine | **done** — `Music`, `Cyclic`, `Transform`, `Project`, `Planner`, `Events` in C++, gated on the fixtures |
| Session persistence | **done** — the document and the open windows save with the host session |
| MIDI in | **done** — reaches the interface, where the Input Control System lives |
| Standalone | **done** — its own transport, its own virtual MIDI port, MIDI Clock at 24 PPQN |
| Pop-out windows | **done** — auxiliary editors open as real OS windows |

## The seam

`processBlock` asks one question: *what plays between these two musical
positions?* A placeholder answered it with a scale while the MIDI path was being
built; M's planner answers it now, and `processBlock` did not change to accept
it. That was the point of putting the seam there.

## The order

Each step is checked against `src/engine/__goldens__/` — fixtures the
TypeScript engine emits. They are plain text, they already exist, and they make
every step pass or fail on values rather than on opinion.

### 1. `Music` — done
`src/engine/music.ts` → `plugin/engine/Music.{h,cpp}`.
Watch the negative cases: JavaScript's `%` keeps the sign of the dividend and
`Math.floor` of a quotient is a floor, not a truncation.
**Gate:** `music.txt`, 2,683 rows.

### 2. `Cyclic` — done
`src/engine/cyclic.ts` → `plugin/engine/Cyclic.{h,cpp}`.
A point range must consume **no** randomness; that is what keeps existing
projects sounding as they did.
**Gate:** `cyclic.txt`, including the mixed sequence that catches exactly that.

### 3. `Transform` — done
`src/engine/transform.ts` → `plugin/engine/Transform.{h,cpp}`.
Step duration, velocity from accent, the density gate, the three Note Order
sources, the Brownian read head.
**Gate:** `transform.txt`, 954 rows — cursor state as well as values.

### 4. `Project` — done
`src/engine/types.ts` and `project.ts` → `plugin/engine/Project.{h,cpp}`.
Voices, Patterns, cyclic tables, the global flags. Structures only.

### 5. `Planner` — done
`src/engine/planner.ts` → `plugin/engine/Planner.{h,cpp}`.
The order randomness is consumed in is part of the contract: accent, legato,
rhythm, then note order, then the density gate.
**Gate:** `voices-*.trace`, `rich-*.trace`, `guard-*.trace`, `detail-*.txt` at
1, 4, 8 and 16 Voices, byte for byte.

### 6. `Events` — done
`src/engine/events.ts` → `plugin/engine/Events.{h,cpp}`.
Generated releases, retrigger resolution, and the total order an adapter
receives. This is the layer a trace cannot see.
**Gate:** `lifecycle-*.txt`.

### 7. Replace the placeholder — done
`StepSource` is gone. `processBlock` drives the planner over the host's PPQ span,
runs the events through the lifecycle, and writes the result out. Per-Voice
channel routing through the existing `midiAssignments.outputs`.
**Gate:** the plugin plays a loaded project, and a transport soak — start,
stop, loop, locate, tempo change, bypass — leaves nothing sounding.

### 8. Project state into the plugin — done
The interface sends whole `.idm` documents across the bridge, and the
processor keeps them verbatim for `getStateInformation` — so what the host
stores is exactly what the interface produced, and a round trip cannot lose a
field this port does not read. `document.ts` itself was not ported: nothing in
the plugin needs to decode a document, only to carry one.
**Gate:** the session blob carries the document and the open windows separately,
and a blob written before window state existed still opens.

### 9. The UI drives the engine — done
The webview edits state; the processor owns it. Changes cross the JUCE
C++/JavaScript bridge into the engine, not the other way round. Closing the
window must not stop the music.

### 10. Standalone — done
Its own MIDI output port rather than the host's buffer, and MIDI clock out —
which the plugin deliberately does not send, because a plugin following host
transport should not also drive it.

### 11. Pop-out windows — done
Auxiliary windows as real OS windows, per `PLUGIN_UI.md`. Their open/closed
state saves with the session, not to `localStorage`.

## Rules for this work

- JUCE and C++. No Rust, no second toolchain, no FFI.
- Every module ported is checked against the existing fixtures before the next
  one starts.
- `-ffp-contract=off` stays on the engine target. Without it the compiler fuses
  `a + b * c` and the arithmetic drifts by one unit in the last place — which
  already happened once, in `TimeMap`.
- Nothing the user did not ask for.

## Done

Every step above is finished. What remains is not on this plan: packaging,
signing and notarisation, a CI matrix, and the host matrix from
`MIDI_PLAN.md` M4 — running it in Logic, Bitwig, Reaper, Cubase, Studio One and
FL and writing down honestly where MIDI out does not work.
