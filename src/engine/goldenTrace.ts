// Deterministic event traces, for checking one engine against another.
//
// M0 exists to make the Voice count a property of the project rather than a
// constant. The risk in that change is not that it fails loudly — it is that
// four Voices keep working while the *music* quietly moves, because a shared
// RNG got re-seeded differently or a cursor was built in another order.
//
// So the count becomes configurable and, in the same step, the output becomes
// pinnable. A trace is the full sequence of planned notes for a fixed project,
// seed and span, flattened to primitives and nothing else. Two engines agree
// when their traces are identical strings.
//
// That is what makes the Rust port a checkable task rather than a rewrite:
// `mulberry32` is `u32` arithmetic that reproduces exactly, so the Rust engine
// can be required to emit these same bytes — not "sound the same".

import { Rng } from "./rng";
import { NoteLifecycle, type OutputDestination } from "./events";
import { makeCursors, planWindow } from "./planner";
import { createDefaultProject } from "./project";
import type { ProjectState, CyclicStep } from "./types";

export type TraceOptions = {
  /** Seconds of music to plan. */
  spanSec?: number;
  /** How many windows to plan it in. Traces must not depend on this. */
  windows?: number;
};

/**
 * One line per note: tick, voice, channel, pitch, velocity, duration in ticks.
 *
 * Seconds are deliberately excluded. They are derived from the tempo map by
 * floating-point multiply, and two languages need not agree on the last bit of
 * a double — ticks are integers and carry the same musical fact.
 */
export function traceProject(
  project: ProjectState,
  seed: number,
  { spanSec = 8, windows = 4 }: TraceOptions = {},
): string {
  const rngs = project.voices.map((_, voice) =>
    new Rng((seed ^ Math.imul(voice + 1, 0x9e3779b1)) >>> 0));
  let cursors = makeCursors(project, 0);
  const lines: string[] = [];
  const step = spanSec / windows;

  for (let w = 0; w < windows; w++) {
    const end = step * (w + 1);
    const planned = planWindow(project, cursors, rngs, step * w, end);
    cursors = planned.cursors;
    for (const note of planned.notes) {
      lines.push([
        note.atTick ?? 0,
        note.voice,
        note.channel,
        note.note,
        note.velocity,
        note.durationTicks ?? 0,
      ].join(","));
    }
  }
  // Sorted, not emission-ordered. Planning the same span in one window or in
  // sixteen produces the same notes, but not in the same sequence: each pass
  // walks the Voices, so more windows interleave the lanes more finely. That is
  // loop structure, and a trace is supposed to be the music. Sorting by tick,
  // then Voice, then pitch makes the comparison canonical — and keeps the
  // window-independence check honest rather than passing by construction,
  // because a note that moved, vanished or appeared still changes the trace.
  lines.sort((a, b) => {
    const x = a.split(",").map(Number);
    const y = b.split(",").map(Number);
    return x[0] - y[0] || x[1] - y[1] || x[2] - y[2] || x[3] - y[3] || x[4] - y[4];
  });
  return lines.join("\n");
}

/**
 * A project built to make every Voice audible and every Voice's RNG matter.
 *
 * The default project will not do. It enables Voice 0 only and seeds Pattern 0
 * only, so a trace of it is identical at one Voice and at sixteen — which would
 * pass while proving nothing.
 *
 * So: every Voice enabled, every Voice reading the seeded pattern, each on its
 * own channel and transposed by its index so the lanes are distinguishable, and
 * density below 1 so the per-Voice RNG is actually consulted. That last one is
 * what makes the trace sensitive to seeding *order* — the failure mode where
 * four Voices still work and the music silently changes.
 */
export function traceFixture(voices: number): ProjectState {
  const project = createDefaultProject(voices);
  return {
    ...project,
    voices: project.voices.map((voice, index) => ({
      ...voice,
      patternIndex: 0,
      playEnabled: true,
      transposition: index * 2,
      density: 0.75,
    })),
  };
}

/**
 * A project that actually exercises the planner.
 *
 * `traceFixture` was built to make the Voice count matter, and it does. It does
 * almost nothing else: every cyclic step is the constant 2, so accent, legato
 * and rhythm consume no randomness and never vary; the Note Order mix is 100%
 * Original, so the Cyclic and Utterly branches never run and `scrambledSteps`
 * is never read; and every flag is off. Mutating the planner's cyclic wrap, its
 * rest handling, its scrambled-list selection or its second-order transposition
 * left those traces byte-identical.
 *
 * So this one turns the machinery on: ranges rather than points, so levels are
 * drawn; a zero level, so rests happen; cycle lengths that differ per Voice, so
 * the position wrap is observable; all three Note Order sources; two output
 * channels; staggered phases; distinct time bases; a bent Time Distortion Map;
 * and diatonic transposition stacked second-order over a snapped scale.
 */
export function traceRichFixture(voices: number): ProjectState {
  const project = createDefaultProject(voices);

  const accentAt = (voice: number, i: number): CyclicStep => {
    const k = (i + voice) % 6;
    if (k === 0) return { min: 0, max: 0 };      // a rest, so velocity 0 is reached
    if (k % 2 === 1) return { min: 1, max: 4 };  // a range, so a draw is consumed
    return { min: 3, max: 3 };                   // a point, which consumes none
  };

  const spanAt = (voice: number, i: number): CyclicStep => {
    const lo = (i + voice * 2) % 4;
    return i % 3 === 0 ? { min: lo, max: lo } : { min: lo, max: Math.min(lo + 2, 4) };
  };

  const cycle = (make: (voice: number, i: number) => CyclicStep) =>
    project.voices.map((_, voice) =>
      Array.from({ length: 16 }, (_, i) => make(voice, i)));

  // Lengths that are not all 16, so `position % length` is observable.
  const lengthsFor = () => project.voices.map((_, v) => [16, 5, 8, 3][v % 4]);

  // `createDefaultProject` seeds Pattern 0 alone; the rest hold steps with no
  // pitches, so a Voice pointed at one is silent. Four Patterns are given
  // material here — including a chord and a rest — so `patternIndex` selects
  // something, and so a step with several pitches reaches the channel loop.
  const seeded = project.patterns.map((pattern, index) => {
    if (index >= 4) return pattern;

    const shift = index * 3;
    const steps = pattern.steps.map((step, i) => {
      if (index === 0) return step;
      if (i % 5 === 4) return { ...step, pitches: [] };
      if (i % 7 === 3) return { ...step, pitches: [48 + shift + i, 55 + shift, 60 + shift] };
      return { ...step, pitches: [50 + shift + ((i * 5) % 13)] };
    });

    return {
      ...pattern,
      steps,
      // Reversed, so the Cyclic branch reading this list rather than `steps`
      // changes the music rather than happening to agree with it.
      scrambledSteps: index === 0 ? pattern.scrambledSteps : [...steps].reverse(),
      outputLength: [8, 16, 11, 6][index],
    };
  });

  return {
    ...project,
    patterns: seeded,
    root: 7,
    scale: "minorPentatonic",
    scaleSnap: true,
    diatonicTranspose: true,
    secondOrderTranspose: true,
    chordTones: false,
    cyclic: {
      accent: cycle(accentAt),
      legato: cycle(spanAt),
      rhythm: cycle((v, i) => spanAt(v, i + 1)),
    },
    cyclicLengths: {
      accent: lengthsFor(),
      legato: lengthsFor(),
      rhythm: lengthsFor(),
    },
    voices: project.voices.map((voice, index) => ({
      ...voice,
      patternIndex: index % 4,
      playEnabled: true,
      transposition: (index % 5) - 2,
      density: 0.6 + (index % 3) * 0.15,
      legato: 0.5 + (index % 4) * 0.25,
      phase: (index % 4) * 12,
      timeBaseNumerator: 1 + (index % 2),
      timeBaseDenominator: [4, 8, 16][index % 3],
      outputChannels: [(index % 16) + 1, ((index + 8) % 16) + 1],
      noteOrderMix: [
        { original: 40, cyclic: 35, utterly: 25 },
        { original: 20, cyclic: 60, utterly: 20 },
        { original: 70, cyclic: 10, utterly: 20 },
      ][index % 3],
      timeDistort:
        index % 2 === 0
          ? voice.timeDistort
          : { points: [{ x: 0.35, y: 0.6 }], length: 2, denominator: 4 },
    })),
  };
}

/**
 * The pitch guardrails on their own.
 *
 * `traceRichFixture` turns Diatonic Transpose on, and that snaps internally —
 * so the Scale Snap that follows it is a no-op and skipping it changes nothing.
 * This variant turns Diatonic off and Scale Snap and Chord Tones on, which is
 * the only arrangement where those two stages are observable.
 */
export function traceGuardFixture(voices: number): ProjectState {
  const project = traceRichFixture(voices);
  return {
    ...project,
    diatonicTranspose: false,
    scaleSnap: true,
    chordTones: true,
  };
}

export function traceGuardProject(voices: number, seed = 1, options: TraceOptions = {}): string {
  return traceProject(traceGuardFixture(voices), seed, options);
}

const detailBits = new DataView(new ArrayBuffer(8));

/** A float as its exact bit pattern. */
function f64Hex(value: number): string {
  detailBits.setFloat64(0, value);
  return detailBits.getBigUint64(0).toString(16).padStart(16, "0");
}

/**
 * Everything a trace leaves out.
 *
 * Traces carry ticks and drop seconds, because seconds come from a
 * floating-point multiply and the trace is meant to be the music. The cost is
 * that `startSec`, `durationSec` and the Rhythm multiplier are then pinned by
 * nothing: dropping the Cyclic Legato from the duration in seconds leaves every
 * trace byte-identical.
 *
 * So they are recorded here as raw bit patterns, where an exact comparison is
 * well defined, along with the Note Order source each step was drawn from.
 */
export function traceDetail(
  project: ProjectState,
  seed: number,
  { spanSec = 8, windows = 4 }: TraceOptions = {},
): string {
  const rngs = project.voices.map((_, voice) =>
    new Rng((seed ^ Math.imul(voice + 1, 0x9e3779b1)) >>> 0));
  let cursors = makeCursors(project, 0);
  const rows: string[] = [];
  const step = spanSec / windows;

  for (let w = 0; w < windows; w++) {
    const planned = planWindow(project, cursors, rngs, step * w, step * (w + 1));
    cursors = planned.cursors;
    for (const note of planned.notes) {
      rows.push([
        note.atTick ?? 0,
        note.voice,
        note.channel,
        note.note,
        note.velocity,
        note.durationTicks ?? 0,
        note.source ?? "",
        f64Hex(note.startSec),
        f64Hex(note.durationSec),
        f64Hex(note.rhythm ?? 0),
      ].join(","));
    }
  }

  // Emission order, deliberately: this file exists to pin the planner's output
  // exactly, and sorting would hide a reordering.
  return rows.join("\n");
}

export function traceDetailProject(voices: number, seed = 1, options: TraceOptions = {}): string {
  return traceDetail(traceRichFixture(voices), seed, options);
}

/**
 * The note lifecycle's output, which is what an adapter actually receives.
 *
 * Traces stop at planned notes. Everything that decides what reaches a MIDI
 * port happens after that, in `NoteLifecycle`: note-offs are generated, an
 * overlapping retrigger has its stale future off withdrawn and an early one
 * issued at the replacement's onset, and the whole batch is put in a total
 * order. None of that is observable in a trace, so it is recorded here.
 *
 * Two destinations, because the ordering rule sorts on the destination name
 * and a single destination cannot show that it does.
 */
export function traceLifecycle(
  project: ProjectState,
  seed: number,
  { spanSec = 8, windows = 4 }: TraceOptions = {},
): string {
  const rngs = project.voices.map((_, voice) =>
    new Rng((seed ^ Math.imul(voice + 1, 0x9e3779b1)) >>> 0));
  let cursors = makeCursors(project, 0);
  const lifecycle = new NoteLifecycle();
  const destinations: OutputDestination[] = ["synth", "midi"];
  const rows: string[] = [];
  const step = spanSec / windows;

  // Programs are enqueued once, before any note, the way the runtime does it.
  lifecycle.addProgramChanges(0, 0, project.voices.flatMap((voice, index) =>
    voice.outputChannels.map((channel) => ({
      voice: index, channel, program: (index * 7) % 128,
    }))));

  for (let w = 0; w < windows; w++) {
    const end = step * (w + 1);
    const planned = planWindow(project, cursors, rngs, step * w, end);
    cursors = planned.cursors;

    lifecycle.ingest(planned.notes, destinations);

    for (const event of lifecycle.drainBefore(end)) {
      rows.push([
        event.type,
        f64Hex(event.atSec),
        event.atTick,
        event.sequence,
        event.destination,
        event.voice,
        event.channel,
        event.type === "program-change" ? -1 : event.noteId,
        event.type === "program-change" ? -1 : event.note,
        event.type === "program-change" ? -1 : event.velocity,
        event.type === "program-change" ? event.program : -1,
      ].join(","));
    }
    rows.push(`# window ${w} drained, ${lifecycle.pendingCount()} pending`);
  }

  return rows.join("\n");
}

export function traceLifecycleProject(
  voices: number,
  seed = 1,
  options: TraceOptions = {},
): string {
  return traceLifecycle(traceRichFixture(voices), seed, options);
}

/** A trace of the rich fixture at a given Voice count. */
export function traceRichProject(
  voices: number,
  seed = 1,
  options: TraceOptions = {},
): string {
  return traceProject(traceRichFixture(voices), seed, options);
}

/** A trace of the fixture at a given Voice count. */
export function traceDefaultProject(
  voices: number,
  seed = 1,
  options: TraceOptions = {},
): string {
  return traceProject(traceFixture(voices), seed, options);
}
