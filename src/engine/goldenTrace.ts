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
import { makeCursors, planWindow } from "./planner";
import { createDefaultProject } from "./project";
import type { ProjectState } from "./types";

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

/** A trace of the fixture at a given Voice count. */
export function traceDefaultProject(
  voices: number,
  seed = 1,
  options: TraceOptions = {},
): string {
  return traceProject(traceFixture(voices), seed, options);
}
