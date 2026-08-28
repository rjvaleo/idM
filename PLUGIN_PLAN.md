# M Classic as a plugin — build plan

**Target:** AU and VST3, macOS / Windows / Linux. MIDI clock in, MIDI out.
Eight voices in the engine. Two interfaces: the classic four-channel window
exactly as it is, and a modern take.

**Status:** plan only. Nothing below is built.

---

## 0. The risk that shapes the product

**VST3 MIDI output is unreliable, and M Classic is a MIDI generator.**

This is the first thing to settle because it determines what the product *is*.
VST3 de-emphasises MIDI by design: note data moves on event buses, and while a
plugin *can* write to an output event list, host support for routing that
anywhere useful is inconsistent. Some hosts do it well, several do not do it at
all. AU is the opposite — it has a first-class MIDI-effect category that Logic
supports properly. CLAP handles note output cleanly.

So a MIDI-only plugin would work in Logic, work in CLAP hosts, and disappoint
in a meaningful share of VST3 hosts. Three ways to respond:

1. **Ship as an instrument that also emits MIDI.** M Classic already has a
   built-in synth (`src/engine/outputs/synth.ts`). As an instrument it makes
   sound in every host on day one, and MIDI out is a bonus where the host
   supports it. This is what most generative tools do, and it is the
   recommendation.
2. **Ship the standalone with virtual ports** (IAC on macOS, a loopback driver
   on Windows). Always works, no host involved, and it is how many people will
   actually use a generative sequencer.
3. **Ship AU MIDI-FX + CLAP as the first-class MIDI targets**, with VST3
   documented as best-effort.

**Decision:** do all three. Instrument-with-MIDI-out as the plugin shape,
standalone as a peer target rather than an afterthought, and honest
host-by-host documentation. Do not promise VST3 MIDI routing until the host
matrix in Phase 4 says which hosts earn it.

---

## 1. What the code already gives us

The engine is in better shape for this than a five-year-old React app has any
right to be.

| | |
|---|---|
| `src/engine` | **5,711 lines, completely pure** — no React, no Zustand, no browser API. Only `vitest`, in tests. |
| `src/engine` tests | **6,212 lines.** More test than source. |
| `OutputSink` | `scheduleBatch(events)` / `cancelScheduled()` / `panic()`. The plugin adapter implements this and nothing else. |
| `EngineEvent` | A closed union: `NoteOn`, `NoteOff`, `ProgramChange`, each tagged `synth` or `midi`. |
| `clockinput.ts` | **MIDI clock in already exists** — 24 PPQN, `0xFA`/`0xFB`/`0xFC`, Song Position Pointer. 138 lines. |
| `clockoutput.ts`, `midiinput.ts` | Clock out and MIDI in, likewise. |
| `rng.ts` | `mulberry32` — 32-bit integer ops only. |
| `VOICE_COUNT` | **One exported constant**, 12 usages, 2 files. |

Two of those are worth dwelling on.

**`mulberry32` ports bit-for-bit.** It is `a + 0x6d2b79f5`, xorshifts and a
multiply, all in `u32`. Rust reproduces it exactly. Which means the Rust engine
can be made to emit *byte-identical* event traces to the TypeScript one — and
the 6,212 lines of existing tests become a **conformance oracle** rather than
something to rewrite. That single fact is what makes a port a defined
engineering task instead of an open-ended rewrite.

**`OutputSink` is already the plugin seam.** The browser implements it with Web
MIDI; the plugin implements it by writing into the host's event output. Nothing
above it needs to know which.

---

## 2. Architecture

```
                    ┌───────────────────────────────────────┐
                    │  m-classic-engine  (Rust, no deps)    │
                    │  planner · variables · cyclic · rng   │
                    │  clock in/out · document              │
                    └───────────────┬───────────────────────┘
                                    │  EngineEvent
              ┌─────────────────────┼─────────────────────┐
              │                     │                     │
     ┌────────▼────────┐   ┌────────▼────────┐   ┌────────▼────────┐
     │  nih-plug       │   │  standalone     │   │  wasm  (browser)│
     │  CLAP + VST3    │   │  + virtual MIDI │   │  the web app    │
     │  → clap-wrapper │   └─────────────────┘   └─────────────────┘
     │    → AU         │
     └────────┬────────┘
              │  state / parameter bridge
     ┌────────▼──────────────────────────────┐
     │  webview UI  —  the existing React app │
     │  classic (4ch, pixel-exact) · modern (8ch) │
     └────────────────────────────────────────┘
```

### Decisions of record

**D1 — Port the engine to Rust.** Not C++, not an embedded JS runtime. The
existing `rust/dsp-core` in idMLab already proves dependency-free portable Rust
works in this project and builds for both native and `wasm32`. A JS runtime
inside a plugin is a determinism and real-time risk for no benefit.

**D2 — `nih-plug` for CLAP and VST3, `clap-wrapper` for AU.** One Rust
codebase, three formats, no C++ and no JUCE licence. JUCE is the more trodden
path to AU and remains the fallback if `clap-wrapper` disappoints on
validation — that is the decision point at the end of Phase 2, not now.

**D3 — Webview UI, not native.** The classic window is pixel-matched to the
M 2.7 manual in React and CSS, and it is the product's identity. Rebuilding it
in a native toolkit would be months of work to arrive back where we started,
worse. The webview hosts the same React that ships in the browser build.

**D4 — The engine supports 1–16 voices; the classic view shows 4.** The
four-channel window stays exactly as it is, because that is the point of it.
Eight voices are a *modern view* feature. The engine is configured, not
hardcoded, and the document format carries the count.

**D5 — Trace equivalence is the porting gate.** The Rust engine is finished
when, for a given project, seed and tempo map, it emits an event trace
byte-identical to the TypeScript engine's. Not "sounds the same."

---

## 3. Phases

### Phase 0 — Make the engine voice-count agnostic *(TypeScript, this repo)*

Do this before the port, so the port targets the final shape.

1. `VOICE_COUNT` becomes a project field, defaulting to 4. 12 usages, 2 files.
2. Document schema **v3**: carries `voiceCount`; v2 migrates by reading 4.
3. The four `[0, 1, 2, 3]` sites in the UI (`App.tsx`, `TimeDistortEditor.tsx`,
   `MidiView.tsx`, `Unified.tsx`) read the project's count. The classic view
   passes 4 regardless.
4. Golden traces at 1, 4, 8 and 16 voices, captured and committed. These are
   the fixtures Phase 1 ports against.

**Gate:** 856 tests still green; the 4-voice golden trace is byte-identical to
today's output; v2 documents open unchanged.

**Rough size:** small. Days, not weeks. The constant is not the hard part — the
document migration and the trace fixtures are.

### Phase 1 — Port the engine to Rust

A new `m-classic-engine` crate: dependency-free, `no_std`-friendly, builds for
native and `wasm32-unknown-unknown`.

Order matters — port bottom-up, and check each layer against its TS twin before
building on it:

1. `rng.ts` → exact `u32` arithmetic. Verify against captured TS sequences.
2. `timemap`, `scheduler`, `clockinput`, `clockoutput` — the timing spine.
3. `transform`, `cyclic`, `variables` — the musical transforms.
4. `planner` — the generative core.
5. `document` — v3 read/write, and the v2 migration.

**Gate (D5):** a harness runs the same project + seed through both engines and
diffs the event traces. Byte-identical across all four golden voice counts, at
several tempo maps and transport shapes. Any divergence is a bug in the port,
never a tolerance to widen.

**Rough size:** this is the bulk of the work. 5,711 lines of source with 6,212
lines of test to satisfy. Call it the majority of the project's effort — but
it is *specified* effort, with an oracle, which is the best kind.

### Phase 2 — The plugin shell

1. `nih-plug` wrapper: CLAP and VST3, instrument category with MIDI output.
2. `clap-wrapper` to produce the AU. Validate with `auval` early — this is
   where the JUCE fallback decision gets made.
3. Host transport → `clockinput`: tempo, play state, song position. The plugin
   follows the host by default; external MIDI clock stays available for
   standalone.
4. `OutputSink` implementation writing into the host's event output, and into
   the built-in synth for the instrument path.
5. Standalone target with virtual MIDI ports.
6. Sample-accurate event placement inside the process block, and a
   `cancelScheduled`/`panic` path on transport stop that leaves no stuck notes.

**Gate:** `auval` passes. `pluginval` at strictness 8+ passes for VST3. A
30-minute soak under host transport start/stop/loop leaves zero stuck notes.

### Phase 3 — The interface

1. Webview host in the plugin, serving the existing React bundle.
2. State bridge: the engine is the source of truth; the UI is a projection.
   Parameter changes and project edits cross the boundary as messages, not as
   shared mutable state.
3. **Classic view** — unchanged, four channels, pixel-exact. It already exists.
4. **Modern view** — new, eight channels. This is a design job as much as an
   engineering one and wants its own scoping once the plugin runs.
5. Host automation for the parameters worth automating (tempo, density,
   transport, variable positions). Stable parameter IDs from day one; changing
   them later breaks every saved session.

**Gate:** both views run inside a host, resize sanely, and survive the host
saving and restoring plugin state.

### Phase 4 — Cross-platform and validation

1. macOS universal (arm64 + x86_64), signed and notarised.
2. Windows x86_64. Linux x86_64 for CLAP and VST3.
3. Host matrix, tested rather than assumed: Logic, Live, Bitwig, Reaper,
   Cubase, Studio One, FL. Record what MIDI out actually does in each — this is
   the evidence for the §0 documentation.
4. Installers.

**Gate:** the matrix is published with honest per-host notes, including where
MIDI routing does not work.

---

## 4. What is deliberately not in this plan

- **idMLab.** It consumes M Classic as a package and is unaffected. The Rust
  engine is additive; the TypeScript engine keeps shipping the browser app and
  keeps being the oracle.
- **The modern view's design.** Named as Phase 3 work, not designed here.
- **AAX.** Pro Tools needs Avid authorisation and its own toolchain.
- **AUv3 / iOS.** Reachable later from the same core; not a target now.

## 5. The order in one line

Voice count → Rust port with a trace-equivalence gate → CLAP/VST3/AU shell →
webview with both views → platforms and host matrix.

The port is the long pole. Everything before it is preparation and everything
after it is integration.
