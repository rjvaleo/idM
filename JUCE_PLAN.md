# Moving M Classic to JUCE

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
| **The notes are a placeholder** | eighth notes over a pentatonic. **Not M.** This is what the rest of the plan removes. |

## The seam

`processBlock` asks one question: *what plays between these two musical
positions?* Today `StepSource` answers it with a scale. When M's planner answers
it instead, `processBlock` does not change.

That is the whole shape of the remaining work: fill in the engine behind that
question, then delete the placeholder.

## The order

Each step is checked against `src/engine/__goldens__/` — fixtures the
TypeScript engine emits. They are plain text, they already exist, and they make
every step pass or fail on values rather than on opinion.

### 1. `Music` — scales, snapping, transposition
`src/engine/music.ts` → `plugin/engine/Music.{h,cpp}`.
Watch the negative cases: JavaScript's `%` keeps the sign of the dividend and
`Math.floor` of a quotient is a floor, not a truncation.
**Gate:** `music.txt`, 2,683 rows.

### 2. `Cyclic` — the five-level variables
`src/engine/cyclic.ts` → `plugin/engine/Cyclic.{h,cpp}`.
A point range must consume **no** randomness; that is what keeps existing
projects sounding as they did.
**Gate:** `cyclic.txt`, including the mixed sequence that catches exactly that.

### 3. `Transform` — the per-step chain
`src/engine/transform.ts` → `plugin/engine/Transform.{h,cpp}`.
Step duration, velocity from accent, the density gate, the three Note Order
sources, the Brownian read head.
**Gate:** `transform.txt`, 954 rows — cursor state as well as values.

### 4. `Project` — the state the planner reads
`src/engine/types.ts` and `project.ts` → `plugin/engine/Project.{h,cpp}`.
Voices, Patterns, cyclic tables, the global flags. Structures only.

### 5. `Planner` — the heart
`src/engine/planner.ts` → `plugin/engine/Planner.{h,cpp}`.
The order randomness is consumed in is part of the contract: accent, legato,
rhythm, then note order, then the density gate.
**Gate:** `voices-*.trace`, `rich-*.trace`, `guard-*.trace`, `detail-*.txt` at
1, 4, 8 and 16 Voices, byte for byte.

### 6. `Events` — the note lifecycle
`src/engine/events.ts` → `plugin/engine/Events.{h,cpp}`.
Generated releases, retrigger resolution, and the total order an adapter
receives. This is the layer a trace cannot see.
**Gate:** `lifecycle-*.txt`.

### 7. Replace the placeholder
`StepSource` goes. `processBlock` drives the planner over the host's PPQ span,
runs the events through the lifecycle, and writes the result out. Per-Voice
channel routing through the existing `midiAssignments.outputs`.
**Gate:** the plugin plays a loaded project, and a transport soak — start,
stop, loop, locate, tempo change, bypass — leaves nothing sounding.

### 8. Project state into the plugin
`src/engine/document.ts` → `plugin/engine/Document.{h,cpp}`, so the plugin can
open a `.mclone`, and `getStateInformation`/`setStateInformation` save with the
host session.
**Gate:** v1, v2 and v3 documents open unchanged.

### 9. The UI drives the engine
The webview edits state; the processor owns it. Changes cross the JUCE
C++/JavaScript bridge into the engine, not the other way round. Closing the
window must not stop the music.

### 10. Standalone
Its own MIDI output port rather than the host's buffer, and MIDI clock out —
which the plugin deliberately does not send, because a plugin following host
transport should not also drive it.

### 11. Pop-out windows
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
