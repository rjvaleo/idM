# MIDI — what exists, what is missing, and the plan

Companion to [`PLUGIN_PLAN.md`](PLUGIN_PLAN.md). Scope: MIDI out, clock in,
clock out, and eight voices, in both the plugin and the standalone.

---

## 1. What already exists

More than the docs suggest. The logic is real and tested; what is missing is
the plugin-side adapters, the voice count, and verification against anything
physical.

| Piece | State |
|---|---|
| `outputs/webmidi.ts` | **Works.** 252 lines. Note-on `0x90`, note-off `0x80`, program `0xC0`, multi-port, timestamped. Panic sends Sustain Off, Reset All Controllers, All Notes Off. |
| Clock **out** | **Works.** `runtime.ts` emits `0xFA` start, `0xFC` stop, `0xF8` at 24 PPQN, gated on a `sendClock` option. |
| Clock **in** | **Works.** `clockinput.ts`, 138 lines: 24 PPQN, `0xFA`/`0xFB`/`0xFC`, Song Position Pointer. Wired behind an `externalClock` option. The M 2.7 manual *retired* external clock, so this is a deliberate modern addition with the MIDI spec as its authority. |
| MIDI **in** | `midiinput.ts`, 121 lines. |
| `NoteLifecycle` | **The stuck-note shadow already exists.** Tracks active notes by `destination:channel:note`; a retrigger cancels the pending note-off and emits an early one. |
| Routing | `midiAssignments.inputs[16]` / `outputs[16]`, each with device and channel. **Eight-voice routing needs no new data model.** |
| `OutputSink` | `scheduleBatch` / `cancelScheduled` / `panic`. The plugin implements this and nothing above it changes. |

### The gap that matters

```
// vitest.config.ts — coverage exclusions
"src/engine/runtime.ts",
"src/engine/outputs/webmidi.ts",
```

**The wiring is excluded from the coverage gate.** `runtime.ts` is where
`sendRealtime(0xFA)`, `sendRealtime(0xFC)` and the `0xF8` scheduling live, and
it is the one file no threshold applies to. The parts are proven; the assembly
is not. This project has been bitten by exactly that twice — a player with no
runtime processor, and a compiler silently dropping sample assignments.

And none of it has met hardware. No device testing, no hot-unplug, no
permission denial, no measured jitter or latency. The README has always said so.

---

## 2. What has to be built

### A — Voice count *(prerequisite for everything else)*

- `VOICE_COUNT` becomes a project field, 1–16, defaulting to 4
- Document schema **v3** carries `voiceCount`; v2 migrates by reading 4
- The four `[0, 1, 2, 3]` sites (`App.tsx`, `TimeDistortEditor.tsx`,
  `MidiView.tsx`, `Unified.tsx`) read the project's count
- Classic view passes 4 regardless — it is a pixel-exact recreation and stays one
- Golden traces captured at 1, 4, 8 and 16 voices

### B — MIDI out, plugin path

- An `OutputSink` writing into the host's event output, replacing `webmidi.ts`
- Sample-accurate placement inside the process block, not block-quantised
- Eight-voice channel routing through the existing `midiAssignments.outputs`
- Panic on transport stop, loop jump, and bypass

### C — Clock in

- **Host transport is the primary source**: tempo, play state, PPQ position,
  loop wrap, locate
- External MIDI clock retained for standalone; `clockinput.ts` is rewired, not
  rewritten
- **Precedence: host transport wins** when both are present. Ambiguity here is
  how a plugin ends up following two clocks and drifting between them
- Mid-flight tempo changes and host automation of tempo

### D — Clock out

- Standalone MIDI clock out — rewire what exists
- **Open decision: does the plugin emit clock at all?** Host support for a
  plugin driving MIDI clock is inconsistent, and a plugin that is *following*
  host transport emitting its own clock is arguably wrong. Default to
  standalone-only until a host matrix says otherwise.

### E — Verification, which is the actual work

- Tests for the wiring currently excluded from coverage, with a fake host
  transport — start, stop, loop, locate, tempo change
- Hardware pass: real devices, hot-unplug, replacement, permission denial
- Measured jitter and latency, against a loopback capture
- Host matrix: Logic, Live, Bitwig, Reaper, Cubase, Studio One, FL
- **Stuck-note soak**: 30 minutes of transport abuse, zero notes left sounding

---

## 3. The plan

### M0 — Voice count *(TypeScript, days)*

Do it first: it changes the engine's shape, and the Rust port should target the
final shape rather than be redone.

**Gate:** 856 tests green; the 4-voice golden trace byte-identical to today;
v2 documents open unchanged; 8 voices produce a correct trace.

### M1 — Close the wiring gap *(TypeScript, days)*

Before porting anything, test what was never tested — while there is still a
reference implementation to test.

1. A fake host transport and a fake MIDI port
2. Assert the actual byte stream: `0xFA` on start, `0xF8` at 24 PPQN against a
   tempo map, `0xFC` on stop, correct behaviour across a loop and a locate
3. Assert `NoteLifecycle` leaves nothing sounding after stop, bypass or a loop
   jump mid-note
4. Remove `runtime.ts` from the coverage exclusions, or state exactly what
   remains excluded and why

**Gate:** the clock and note paths are covered; the exclusion list shrinks.

**Why here and not later:** these tests become the specification the Rust port
is checked against. Written after the port, they only prove the port matches
itself.

### M2 — Rust engine port

Per `PLUGIN_PLAN.md` Phase 1. MIDI-specific additions: `clockinput`,
`clockoutput` and `NoteLifecycle` port with their tests, and the M1 byte-stream
assertions run against the Rust engine unchanged.

**Gate:** byte-identical event traces, and byte-identical clock streams,
against the TypeScript engine at all four voice counts.

### M3 — Host adapters *(JUCE)*

1. Host transport → clock in, with the precedence rule from C
2. `OutputSink` → host event output, sample-accurate
3. Standalone: external clock in, MIDI clock out, virtual ports
4. Panic wired to every path that can strand a note

**Gate:** `auval` and `pluginval` pass. A scripted transport soak — start,
stop, loop, locate, tempo ramp, bypass — leaves zero notes sounding.

### M4 — Certification *(the debt, finally paid)*

1. Real hardware: at least one USB interface and one virtual port per platform
2. Hot-unplug, replacement, permission denial mid-performance
3. Jitter and latency measured by loopback capture, published as numbers
4. Host matrix run and written down, including **where MIDI out does not work**

**Gate:** the matrix is published with honest per-host results. A host that
cannot route MIDI out is documented as such, not quietly hoped over.

---

## 4. Order, and why

```
M0 voice count → M1 test the wiring → M2 port → M3 adapters → M4 certify
```

M1 before M2 is the load-bearing choice. Everything else is conventional.

The reason: the untested wiring is the highest-risk code in the MIDI path, and
today there is a working reference implementation to write tests against. After
the port there is only the port, and a test written then proves the Rust engine
agrees with itself rather than that either is correct.
