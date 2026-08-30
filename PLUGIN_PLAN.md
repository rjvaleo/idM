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
inside a plugin is a determinism and real-time risk for no benefit. Porting to
C++ instead would remove a language from the build, but would give up the
exhaustive matching that makes the event union safe to port and the `wasm32`
target that keeps the browser app on the same engine.

One engine, three consumers: `wasm32` for the browser app, and a static library
for both the plugin and the standalone.

*Amended 2026-08-30: **the engine ports to C++, not Rust.** The justification
above rests on one engine serving three consumers, the third being the browser
app via `wasm32`. The browser app does not need replacing — it works, it is
what ships today, and it keeps its TypeScript engine. Remove that consumer and
Rust buys a second language, a second toolchain, a C ABI to marshal project
state across, and Corrosion wiring Cargo into CMake on three platforms, in a
plugin that is C++ from top to bottom.*

*What survives the change is the part that mattered: the conformance fixtures
in `src/engine/__goldens__/` are plain text emitted by the TypeScript engine,
so they gate a C++ port exactly as well as they gated the Rust one. The Rust
modules are parked at `engine-rust-parked/` as a tested reference to translate
from, not deleted.*

*Recorded because a day and a half went into the Rust port before this was
reconsidered, and the next person should not have to rediscover why it stopped.*

**D2 — JUCE 9 for the plugin shell; the engine stays Rust behind a C ABI.**
Decided against the all-Rust alternative (`nih-plug` for CLAP, `clap-wrapper`
re-exporting as AUv2) for one reason: AU is the hard requirement, and in that
arrangement AU arrives through the newest and least-proven link in the chain.
JUCE's AU support is native and is what most shipping plugins use.

Two things settled it beyond that. **JUCE has first-class WebView UI** — a
C++/JavaScript bridge, parameter binding, hot reload, and an official
`WebViewPluginDemo` with a React frontend, which is precisely this project's
shape. And DAWs are tested against JUCE, so a misbehaving host is a path
thousands of plugins have already walked.

One target yields VST3, AU and Standalone, plus CLAP through
`clap-juce-extensions`.

*Amended 2026-08-28: this decision originally read JUCE 8, on the belief that
`clap-juce-extensions` was proven only against 8.x. That was wrong — its most
recent commit (2026-08-05) is a JUCE 9 fix for embedded UIs, which is precisely
our webview case.*

*The move to **9.0.1** was tested rather than assumed. The same scaffold builds
clean and reports `AU VALIDATION SUCCEEDED` under both 8.0.15 and 9.0.1 on
macOS 26 with Command Line Tools alone. JUCE 9 wins on the one axis this
project cares about: the WebBrowserComponent bindings are now a typed npm
package, `@juce-framework/webview`, where 8.x offers an untyped `index.js` at a
path 9 has already moved. Building the UI on 8 would mean writing against a
location we would have to migrate off later. The 9.0.0 breaking changes — the
SVG parser rework, `Drawable` no longer inheriting `Component`, Windows
multi-touch defaults, zlib built as C — all bite existing codebases; ours was a
hundred lines old.*

*The only argument left for 8.x is that it has a year more exposure in
shipping hosts. Real, but alone against the list above.*

**D2a — JUCE licensing is not a blocker.** JUCE is dual licensed: the JUCE
licence, or **AGPLv3** (not GPLv3 — the network-use clause differs, though for
a desktop plugin it rarely bites).

The free **Starter** tier permits **closed-source commercial distribution** up
to roughly $20,000 annual revenue. The JUCE 9 EULA carries the same
tiers and the same figures as JUCE 8; `LICENSE.md` differs only in the EULA
URL, and the framework remains AGPLv3-or-commercial. Above that, **Indie** covers up to $300,000
at $40/month or **$800 perpetual**; **Pro** is unlimited at $175/month or
$3,500 perpetual.

So selling M Classic costs nothing until it earns real money, and $800 once
thereafter — which is not a consideration against the effort of building it.
For an individual the limit aggregates *all* revenue arising from use of the
framework, including donations, sponsorship and advertising, not only sales.

This removes the argument for revisiting `nih-plug`. The remaining reasons to
prefer it would be language preference or a wish to avoid a copyleft-adjacent
dependency entirely, neither of which outweighs proven AU support.

**D2b — CMake at the top, Corrosion bridging to Cargo.** The Rust engine builds
as a static library inside the normal CMake build rather than as a separate
step. CI is a GitHub Actions matrix — macOS universal (arm64 + x86_64, signed
and notarised), Windows x86_64, Linux x86_64 — with `auval` and `pluginval`
running in CI rather than by hand.

**D3 — Webview UI, not native.** The classic window is pixel-matched to the
M 2.7 manual in React and CSS, and it is the product's identity. Rebuilding it
in a native toolkit would be months of work to arrive back where we started,
worse. The webview hosts the same React that ships in the browser build, over
JUCE's WebBrowserComponent bridge.

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
2. JUCE targets for VST3, AU and Standalone; CLAP via `clap-juce-extensions`.
   Run `auval` from the first day there is a bundle, not at the end.
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
