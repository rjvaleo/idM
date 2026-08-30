# M Classic

[![Version](https://img.shields.io/badge/version-0.8.0--alpha-E65100)](CHANGELOG.md)

> **Alpha — 0.8.0.** M Classic Web is feature-complete against the M 2.7 manual
> audit apart from documented exclusions, and every gate below is green. It is
> not a 1.0: the role-specific Classic audio rack, release hardening, and
> hardware/browser MIDI certification are still open, and the Rust engine port
> ([`engine/`](engine/README.md)) and the AU/VST3 plugin
> ([`plugin/`](plugin/README.md)) are in progress. The interface and the
> `.mclone` document format may change before 1.0. See
> [`CHANGELOG.md`](CHANGELOG.md) for what this build includes, what it excludes,
> and its known limitations.

**Run it locally:** `npm run dev` for the dev server, `npm run build` for the
static site in `dist/`, or `npm run build:single` for a self-contained
`dist-single/index.html` you can open straight in a browser. There is no hosted
build of this repository, and no CI — the gates below are run locally.

**Builds:** there are no published downloads. `npm run build:single` writes a
self-contained `dist-single/index.html` (open it directly in a browser — no
server, no install); `npm run build` writes the static site to `dist/`.

### Current stack and engineering practices

![TypeScript](https://img.shields.io/badge/TypeScript-5.9.3-3178C6?logo=typescript&logoColor=white)
![React](https://img.shields.io/badge/React-18.3.1-61DAFB?logo=react&logoColor=111827)
![Vite](https://img.shields.io/badge/Vite-5.4.21-646CFF?logo=vite&logoColor=white)
![Zustand](https://img.shields.io/badge/Zustand-4.5.7-433E38)
![Vitest](https://img.shields.io/badge/Vitest-2.1.9-6E9F18?logo=vitest&logoColor=white)
![Rust engine](https://img.shields.io/badge/Rust-engine_port_in_progress-000000?logo=rust&logoColor=white)
![JUCE](https://img.shields.io/badge/JUCE-9.0.1_AU_%7C_VST3_%7C_Standalone-8DC63F)
![Node](https://img.shields.io/badge/local_verification-Node_25.4.0-339933?logo=node.js&logoColor=white)
![npm](https://img.shields.io/badge/npm-package_tooling-CB3837?logo=npm&logoColor=white)
![Web Audio API](https://img.shields.io/badge/Web_Audio_API-timestamped_synthesis-8A2BE2)
![Web MIDI API](https://img.shields.io/badge/Web_MIDI_API-live_input_%2B_timestamped_output-0A7EA4)
![HTML5](https://img.shields.io/badge/HTML5-semantic_UI-E34F26?logo=html5&logoColor=white)
![CSS3](https://img.shields.io/badge/CSS3-responsive_vector_UI-1572B6?logo=css&logoColor=white)

![TDD](https://img.shields.io/badge/practice-TDD-2E7D32)
![Engine coverage](https://img.shields.io/badge/engine%2Fstate_coverage-100%25_lines_%7C_functions_%7C_statements%2C_99%25_branches-brightgreen)
![Tests](https://img.shields.io/badge/tests-909_passing-brightgreen)
![Typecheck](https://img.shields.io/badge/TypeScript_typecheck-passing-brightgreen)
![Production build](https://img.shields.io/badge/production_build-passing-brightgreen)
![Architecture](https://img.shields.io/badge/architecture-pure_engine_%2B_platform_adapters-44546A)
![Timing](https://img.shields.io/badge/timing-960_PPQN_%2B_adaptive_lookahead-6A1B9A)
![Deterministic](https://img.shields.io/badge/generation-seeded_%26_deterministic-795548)
![Clean room](https://img.shields.io/badge/reimplementation-clean_room-37474F)
![Document format](https://img.shields.io/badge/document_format-.mclone_v3-263238?logo=json&logoColor=white)

### Roadmap candidates and targets

![Tauri candidate](https://img.shields.io/badge/Tauri-native_shell_candidate-24C8DB?logo=tauri&logoColor=white)
![CLAP candidate](https://img.shields.io/badge/CLAP-via_clap--juce--extensions-5C2D91)
![Platforms planned](https://img.shields.io/badge/targets-macOS_%7C_Windows_%7C_Linux-607D8B)
![Editions](https://img.shields.io/badge/editions-Classic_%7C_Studio-7B1FA2)

These badges describe product targets and implementation candidates, not the
current shipped stack. The plugin targets, the host matrix, and the decisions
behind them are in [`PLUGIN_PLAN.md`](PLUGIN_PLAN.md) and
[`MIDI_PLAN.md`](MIDI_PLAN.md).

Behavioural specifications inherited from the M 2.7 manual — pattern commands,
phrasing and legato, movie capture and SMF export, conducting, and the timing
and MIDI invariants — are in Appendix A of `IDMLAB_MASTER_PLAN.md`, which went
to the `idmlab` repository in the split. The executable form of the same
inventory stayed here: `src/manual/manualConformance.ts`, checked by
`npm run test:manual`.

Current verified checkpoint (**0.8.0-alpha**): **909 passing tests across 66
files**, **100%** statement, line and function coverage and **99%** branch
coverage for the included engine and state modules, clean TypeScript checking,
and successful normal and single-file production builds. The retained M 2.7
manual gap queues are closed. Live MIDI input, recording, routing, Input
Control, Mouse/Step Advance, metronome, clock, and the sixteen-channel
assignment matrix are implemented. Sound Choice and MIDI import/imported
Sequence playback remain intentional exclusions.

The next frontier is the plugin: a JUCE shell in [`plugin/`](plugin/README.md)
and a Rust port of the engine in [`engine/`](engine/README.md), against
[`PLUGIN_PLAN.md`](PLUGIN_PLAN.md). The role-specific Classic audio rack is
behind it. Hardware/browser MIDI certification proceeds in parallel when
representative devices are available.

Projects save as `.mclone` files. Their payload is readable, versioned JSON,
but it represents the complete musical project—not merely a graph. Legacy
`.mclone.json` and `.json` project files remain importable.

## Documentation

**[`PLUGIN_PLAN.md`](PLUGIN_PLAN.md) is the plan for what M Classic becomes
next.** `IDMLAB_MASTER_PLAN.md`, which used to be the only plan for the combined
project, went to the `idmlab` repository in the split. The many separate
roadmap, status, spec and next-steps documents that used to live here were
folded into it on 2026-08-05 and removed; they remain in git history.

- [`CHANGELOG.md`](CHANGELOG.md) — released versions and what each one included.
- [`PLUGIN_PLAN.md`](PLUGIN_PLAN.md) — the AU/VST3/CLAP plan and the host matrix.
- [`MIDI_PLAN.md`](MIDI_PLAN.md) — MIDI out, clock in and out, and eight voices.
- [`PLUGIN_UI.md`](PLUGIN_UI.md) — how the browser interface becomes the plugin window.
- [`plugin/README.md`](plugin/README.md) — the JUCE shell and how to build it.
- [`engine/README.md`](engine/README.md) — the Rust port and the golden traces it must reproduce.
- [`fonts/CATALOG.md`](fonts/CATALOG.md) — per-file font licence findings.
- `reference/README.md` (moved to the `audio-research` repository) — provenance of the manual,
  screenshots and mockups.
- `reference/panels/CATALOG.md` (moved to the `audio-research` repository) — the hardware
  panel catalogue and its layout grammar.
- `rust/README.md` (moved to the `idmlab` repository) — the DSP crate's real-time
  non-negotiables.

## Status

**Unified classic interface — it plays.** A pure, fully-tested generative engine
drives both a built-in WebAudio synth and Web MIDI output, behind a movable
recreation of M's main windows plus dedicated Pattern, Cyclic, and Midi View tools.

Two ports are underway and neither runs anything yet. `engine/` reproduces the
TypeScript engine's traces in Rust as far as the planner; `plugin/` is a JUCE
shell that serves the browser build's own UI and carries no engine.

![M-Clone at 150% in the Red channel palette, light theme: Patterns, transport,
Snapshot, Pattern Editor, Cyclic Variables, Variables, the Cyclic Editor in
Classic view, the Midi output strip and Midi View, with Transposition,
Orchestration, Velocity Range, Note Density, Note Order, and Time Distortion
open down the right side.](docs/screenshots/m-clone-red-theme.png)

*The full window suite at 150%, Red channel palette, light theme.*

![The same interface at 100% in the Classic four-color palette, showing the
project "Suffix Check.mclone" with the built-in Synth module open alongside
Midi View, the Pattern Editor, the Cyclic Editor, Note Order, Velocity Range,
Note Density, and Transposition.](docs/screenshots/m-clone-classic-theme.png)

*The Classic four-color palette at 100%, with the built-in Synth open. Each
Voice keeps its color across every module.*

Working today:

- **Voices** and **Patterns**, a per-step **piano-roll editor**. A project opens
  with four Voices and may carry between one and sixteen; the `voices` array is
  the count, and every window draws a lane per Voice. It holds 24 Patterns —
  six Pattern Groups of four, one Pattern per Voice at a time.
- **Variable Positions (a–f):** six snapshot-able positions for Note Order,
  Transposition, Note Density, Velocity Range, Time Distortion, and
  Orchestration — click a cell to activate it, double-click a cell to open that
  variable's per-voice editor. Orchestration's cells sit in the Midi window
  with the rest of the output routing. Pattern Group also has six
  active/conductable positions.
- **M-style Note Order mixing:** each Voice blends Original Order, a
  stored/repeating Cyclic Random scramble, and continually changing Utterly
  Random playback. Two handles sit directly on the segmented bar: the first
  sets the Original/Cyclic boundary and the second sets the Cyclic/Utterly
  boundary. The solid, gray, and polka-dot regions—and their percentages—update
  continuously while either handle is dragged. The handles stay on fixed sides
  of their boundaries, remain inside the bar at 0% and 100%, and update at most
  once per animation frame for responsive movement. Every a–f Variable Position
  stores an independent mix for every Voice.
- **Pattern-owned Cyclic Random lists:** ReScramble, Original → Scrambled, and
  Swap Scrambled and Original work on a whole Pattern or selected Region.
  Ordinary Pattern edits automatically maintain the stored repeating copy.
- **Pattern Editor (M-style window):** vertical piano keyboard, dotted step
  grid, region/eraser/plunger/scissors tools, View 1–4 + Size panel, octave
  shift, and a bottom-right size box. **Drag to paint notes on/off**; the grid
  grows by adding **fixed-size** cells (never stretched) both horizontally and
  vertically — the area past the pattern's length is a greyed but fully paintable
  grid, and painting into it **auto-extends the length** to the furthest note
  (grow-only; erasing never shrinks it).
- **Movable window canvas:** every window drags by its title bar to anywhere and
  stays exactly where it is dropped; permanent modules remember where they were
  left, while auxiliary windows open in a fresh slot each time. Hairline
  borders, opaque backgrounds, and the last-clicked window comes to the front.
  Modules keep a fixed size.
- **Window manager:** right-click blank canvas to open any available window.
  The six reference main windows stay open; auxiliary editors and Midi View can
  close and reopen, and all editor windows can coexist without modal overlays.
  Right-clicking occupied module space retains that module's commands and adds
  the same available-window launcher beneath them, so free canvas space is not
  required to open another module.
  New/reopened auxiliaries occupy the leftmost free column beyond the permanent
  modules and stack with 4px padding. A window stays exactly where it is
  dropped; the edge snapping and overlap resolution apply only when a window
  opens.
- **Unified window navigation:** every main and auxiliary window uses the same
  compact reference title bar. Patterns, Conducting, Variables, Midi, the Pattern
  Editor and the Cyclic Editor add window-specific commands on right-click;
  Snapshot, Midi View, Midi Assignment, Synth and the six Variable editors have
  no context menu yet.
- **Classic global menu:** File, Edit, Variables, Pattern, Windows, and Options
  are restored at the top; the footer is removed so the canvas uses the full
  remaining viewport.
- **Uniform rendered typography:** all panel titles render at 10px in 16px
  chrome, primary controls at 8px, and dense readouts at the 7px compact tier.
- **640 × 480 floor, viewport-sized desktop:** the reference composition still
  lands at its original coordinates, but the desktop grows to fill the window
  rather than stopping at 640 × 480, so modules drag anywhere the viewport
  reaches. The whole module suite scales from 50–200% in 10% increments, with a
  1:1 control and scale-aware dragging, menus, and persistence.
- **Complete channel theming:** four-voice artwork in Patterns, Variables,
  Cyclic, Midi, Pattern Editor, and auxiliary editors follows the global preset
  or custom palette rather than falling back to black and white.
- **Snapshots and Slideshows:** 26 partial A–Z locations with Hold/Do,
  Edit/copy, Blink Everything, recall/erase/restore, plus nine timed
  record/play/pause/loop/stop Slideshows, Record Wait, Snapshot-quantized
  playback, and version-3 persistence that still opens v1 and v2 documents.
- **Manual-faithful Velocity Range:** per-Voice shaded low/high range bar drawn
  directly on the axis line — **click-drag on the line to draw the range** —
  plus editable endpoint numericals. Accent 0 is silent; levels 1–4 span the
  selected low→high range.
- **Conducting Window:** manual-faithful Start / Stop / Pause / Sync, Tempo,
  six-by-six Baton grid, conducted Tempo range, Sync Ratio, bounded Robot
  Conductor, reference-colored controls, clipped-safe lower numericals, and
  pull-out per-Voice Continuous Conducting for Velocity Range and Legato.
  Right-click adds Key, Scale and Scale Snap.
- **Midi performance strip:** each of six a–f Orchestration Positions stores a
  per-Voice × 16-channel routing matrix; a Voice can layer across multiple MIDI
  channels or be silent.
- **Separate Midi Assignment window:** sixteen input/output device mappings,
  latency, conducting controllers, and channel messages stay in their File-menu
  setup window instead of expanding the quick strip. The program-number base
  selector is stored and saved but nothing reads it yet.
- **Two outputs:** Web MIDI to any device or DAW, and a built-in monitor synth
  that is **off by default** — open Windows ▸ Synth and switch on a stream's
  Power to hear it without a MIDI destination.
- **Live MIDI input:** sixteen assignable device/channel rows feed per-Voice
  Source and Use modes, Echo-Thru/Echo Map, Keyboard Transpose, Pattern
  recording, `sa` Step Advance, Mouse Advance, and the Appendix B Input Control
  System. Controller X/Y assignments drive the Baton.
- **Four independent monitor patches:** each stream has its own color-coded,
  click-safe subtractive Synth patch with dual oscillators, sub/noise mixer,
  routable LFO, resonant multimode filter, dual ADSR envelopes, glide, velocity,
  and master controls.
- **MIDI reliability foundation:** shared 960-PPQN positions, per-Voice RNG,
  explicit ordered Note On/Off/Program Change events, lifecycle-owned releases,
  equal-timestamp batch submission, and clear-before-panic transport transitions.
- **Light / Dark themes:** the header toggle re-skins the whole interface between
  coherent light and dark palettes, including Patterns, Conducting, and Midi
  View. Individual module title bars use flat surfaces rather than stripes.

The five-level Accent, Legato, and Rhythm Cyclic Variables drive playback.
Legato is M's Phrasing system: its 6/25/50/75/100% levels scale the actual time
to the next onset and may exceed 100% for overlapping articulation. All three
Cyclic Variables have six conductable, Snapshot/Slideshow-aware Positions.
Their dedicated editor has a reference-derived Classic view and a right-click
Modern view that exposes all three variables and all twelve Voice grids at once.
Time Distortion, Grid conducting, Robot Conducting, and assigned external MIDI
Conducting are active performance paths.

## Run it

One-command control script (starts the dev server and opens the app in your
browser):

```bash
./mclone.sh start     # start + open the app (installs deps on first run)
./mclone.sh stop      # stop the server
./mclone.sh restart   # stop then start
./mclone.sh status    # is it running?
```

Override the port with `MCLONE_PORT=5200 ./mclone.sh start`. The same variable
is needed on `stop` and `status`. Or run it manually:

```bash
npm install
npm run dev
```

The dev server opens the app itself — `vite.config.ts` sets `server.open` — at
http://localhost:5174/. Press **Start**, edit the grid, activate positions,
and ride the sliders. For MIDI, choose **File ▸ Midi Assignment**, click
**Enable / Refresh MIDI**, and map input/output ports (Chromium browsers). To
build a single self-contained HTML preview: `npm run build:single`
then open `dist-single/index.html`.

## Deploy

M Classic ships three ways today, all from this repository:

```bash
npm run build        # typechecked production build in dist/
npm run build:single # one self-contained HTML file in dist-single/
npm run build:lib    # library build in lib/, what the package exports
```

The single-file build is the interesting one: the whole instrument inlined into
one document that runs by opening it, with no server and no install. A browser
needs an HTTPS secure context for Web MIDI, so a hosted copy still wants TLS;
device access then depends on browser support and user permission.

A fourth is under way: `plugin/` builds an AU, a VST3 and a Standalone around
the single-file UI. See [`plugin/README.md`](plugin/README.md).

There is no Pages workflow here, and no CI of any kind — `.github/` is empty.
The pre-split repository deployed to
[rjvaleo.github.io/M-Clone](https://rjvaleo.github.io/M-Clone/) and that site is
still live, but it serves the combined application as it stood before M Classic
and idMLab became separate repositories — it does not track this one.

## Releases

No release workflow is wired up yet. The inherited one built the combined
bundle and was removed in the split; what replaces it depends on what M Classic
is released *as*. The plugin is past the planning stage — `plugin/` already
builds AU, VST3 and Standalone targets around the single-file UI
([`plugin/README.md`](plugin/README.md)) — but nothing is packaged, signed or
distributed yet.

## Develop

```bash
npm test          # unit tests (Vitest)
npm run coverage  # tests + coverage (engine/state held at 100%)
npm run test:watch # interactive test watch mode
npm run test:manual # M 2.7 manual conformance audit
npm run typecheck # tsc --noEmit
npm run build     # typecheck + production build
npm run build:single # self-contained dist-single/index.html
npm run build:lib # library build into lib/ (also runs on npm install)
npm run preview   # preview the normal production build
```

The cross-language gate for the Rust port, three commands that have to agree:

```bash
npm run goldens       # rewrite the conformance fixtures from the TS engine
npm run goldens:check # fail if they would change
npm run engine:test   # check the Rust engine against them
```

The plugin is built separately, and needs `npm run build:single` to have run
first — its UI is that file. The sequence, the JUCE submodule, the macOS
toolchain trap and the `auval` procedure are in
[`plugin/README.md`](plugin/README.md). The Rust engine port and the golden
fixtures that gate it are in [`engine/README.md`](engine/README.md).

## Layout

```
src/
  engine/          framework-agnostic musical core (developed test-first)
    music.ts       scales / key-snapping
    rng.ts         seeded + Brownian randomness
    transform.ts   per-step transform primitives
    planner.ts     the pure scheduler heart
    transport.ts   timing-change continuity segments
    scheduler.ts   adaptive lookahead policy + diagnostics
    events.ts      explicit event protocol + note lifecycle
    eventbatch.ts  versioned adapter-neutral MIDI event batches
    midiinput.ts   normalized live MIDI input and routing helpers
    inputcontrol.ts Appendix B Input Control command mapping
    clockoutput.ts MIDI Clock scheduling
    movie.ts       performance capture + deterministic SMF encoding
    variables.ts   Variable Positions (a–f) model
    snapshot.ts    partial Snapshot capture/apply + quantization
    slideshow.ts   deterministic recording/playback state machine
    document.ts    defensive JSON v3 codec + v1/v2 migration
    project.ts     defaults
    runtime.ts     Web Audio lookahead scheduler (browser-only wiring)
    goldenTrace.ts deterministic traces, the cross-language contract
    __goldens__/   the fixtures the Rust engine must reproduce
    outputs/       synth + Web MIDI sinks
  state/store.ts   zustand store (live project document)
  ui/              React — the Unified movable-window canvas, light + dark themes
    Unified.tsx    the canvas and movable application windows
    PatternEditor.tsx  M-style editor (paint, resize, auto-length)
    CyclicEditor.tsx   Classic/Modern five-level cyclic editor
    MidiView.tsx       per-Voice generated-output tracker
    ConductorWindow.tsx transport, Baton, tempo, Robot, and Movie controls
    SynthWindow.tsx    four-patch subtractive monitor control surface
    windowlauncher.tsx canvas-wide auxiliary-window launcher
    useDraggable.ts    draggable/persisted window positions + z-order
  manual/          executable manual capability inventory
scripts/           golden fixture regeneration
engine/            Rust port of the engine (rlib + staticlib), golden-gated
plugin/            JUCE 9 shell — AU / VST3 / Standalone around the single-file UI
lib/               library build output (npm main/types/exports)
docs/              screenshots
```

Included engine and state logic are held at **100% statement/line/function
coverage and 99% branch coverage** by Vitest's V8 provider. Browser runtime and
Web MIDI behavior also have fake-clock/fake-scheduler/fake-port tests, while
their browser-only adapter code is excluded from the Node coverage threshold.
The browser scheduler wakes on the main thread with a bounded adaptive horizon;
the exact guarantees and verification steps live with the code, in
`src/engine/scheduler.ts` and `src/engine/clockwiring.test.ts`.
