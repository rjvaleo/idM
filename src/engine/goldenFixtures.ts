// The cross-language conformance fixtures, as data.
//
// Both the writer (`scripts/goldens.ts`) and the guard
// (`goldenFixtures.test.ts`) build the expected bytes from here, so the files
// on disk cannot drift from the engine without one of them failing.

import { Rng, BrownianWalk } from "./rng";
import {
  traceDefaultProject, traceFixture, traceRichProject, traceRichFixture,
  traceGuardProject, traceGuardFixture, traceDetailProject,
} from "./goldenTrace";
import {
  neutralTimeMap, normalizeTimeMap, isNeutralTimeMap, realToClock, clockToReal,
  timeMapSeconds, distortClockSeconds, type TimeMap,
} from "./timemap";
import {
  SCALES, snapToScale, snapToChord, diatonicTranspose, clampMidi, midiToName,
  type ScaleName,
} from "./music";
import {
  normalizeCyclicStep, pickCyclicLevel, cyclicLengthFromStepIndex,
} from "./cyclic";
import {
  stepDurationSeconds, gate, normalizeVelocityRange, velocityForAccent,
  nextStepIndex, makeCyclicOrder, noteOrderMixFromEdges, nextMixedStepIndex,
} from "./transform";
import type { NoteOrder, NoteOrderCursor, NoteOrderMix } from "./types";

/** The seeds a trace actually uses, plus the edges of the u32 range. */
const SEEDS = [0, 1, 42, 0x9e3779b1, 0xffffffff, 0x6d2b79f5];
const DRAWS = 32;
const HELPER_DRAWS = 8;
const WALK_DRAWS = 16;

const INT_BOUNDS = [1, 2, 7, 16, 128];
const AVOID_CASES: ReadonlyArray<readonly [number, number]> = [
  [1, 0], [2, 0], [2, 1], [8, 3], [8, 7],
];

/**
 * mulberry32's raw u32 output, not the float.
 *
 * `next()` returns u32 / 2^32, which is exact in a double, so multiplying back
 * recovers the integer without loss. Comparing integers keeps the fixture free
 * of any argument about how two languages print a float.
 */
export function rngFixture(): string {
  const lines: string[] = [
    "# mulberry32 conformance fixture - regenerate with scripts/goldens.ts",
    "# raw u32 draws: seed,index,u32",
  ];

  for (const seed of SEEDS) {
    const rng = new Rng(seed);
    for (let i = 0; i < DRAWS; i++) {
      const u32 = rng.next() * 4294967296;
      if (!Number.isInteger(u32)) throw new Error(`non-integral draw at ${seed}/${i}`);
      lines.push(`${seed >>> 0},${i},${u32}`);
    }
  }

  lines.push("# Rng.int(n): seed,n,index,value");
  for (const seed of SEEDS) {
    for (const n of INT_BOUNDS) {
      const rng = new Rng(seed);
      for (let i = 0; i < HELPER_DRAWS; i++) lines.push(`${seed >>> 0},${n},${i},${rng.int(n)}`);
    }
  }

  lines.push("# Rng.pickIndexAvoiding(n, avoid): seed,n,avoid,index,value");
  for (const seed of SEEDS) {
    for (const [n, avoid] of AVOID_CASES) {
      const rng = new Rng(seed);
      for (let i = 0; i < HELPER_DRAWS; i++) {
        lines.push(`${seed >>> 0},${n},${avoid},${i},${rng.pickIndexAvoiding(n, avoid)}`);
      }
    }
  }

  // The walk is plain f64 arithmetic over the same draws. Recorded as raw bits
  // so the check is exact rather than "close enough".
  lines.push("# BrownianWalk: seed,index,f64bits(hex)");
  for (const seed of SEEDS) {
    const walk = new BrownianWalk(new Rng(seed));
    for (let i = 0; i < WALK_DRAWS; i++) {
      lines.push(`${seed >>> 0},${i},${hex(walk.next())}`);
    }
  }

  return lines.join("\n") + "\n";
}

/* ===== Time Distortion Map ===== */

const bits = new DataView(new ArrayBuffer(8));

/** A float as its exact bit pattern, so the check cannot be "close enough". */
function hex(value: number): string {
  bits.setFloat64(0, value);
  return bits.getBigUint64(0).toString(16).padStart(16, "0");
}

/**
 * Maps chosen for the edges rather than for looking plausible: unordered
 * points, points that double back, points outside the unit square, and two
 * points sharing an x so a segment has zero width.
 */
const TIME_MAPS: ReadonlyArray<readonly [string, TimeMap]> = [
  ["neutral", neutralTimeMap()],
  ["diagonal-explicit", { points: [{ x: 0.5, y: 0.5 }], length: 1, denominator: 4 }],
  ["slow-then-fast", { points: [{ x: 0.5, y: 0.25 }], length: 1, denominator: 4 }],
  ["fast-then-slow", { points: [{ x: 0.5, y: 0.75 }], length: 2, denominator: 8 }],
  ["s-curve", { points: [{ x: 0.25, y: 0.1 }, { x: 0.75, y: 0.9 }], length: 4, denominator: 4 }],
  ["unordered", { points: [{ x: 0.8, y: 0.6 }, { x: 0.2, y: 0.3 }, { x: 0.5, y: 0.5 }], length: 1, denominator: 1 }],
  ["doubling-back", { points: [{ x: 0.6, y: 0.7 }, { x: 0.4, y: 0.2 }], length: 8, denominator: 16 }],
  ["out-of-range", { points: [{ x: -0.5, y: 1.5 }, { x: 1.5, y: -0.5 }], length: 3, denominator: 2 }],
  ["zero-width-span", { points: [{ x: 0.5, y: 0.2 }, { x: 0.5, y: 0.8 }], length: 1, denominator: 4 }],
  ["step", { points: [{ x: 0.5, y: 0.0 }, { x: 0.5, y: 1.0 }], length: 16, denominator: 4 }],
  // Straddling the 1e-9 tolerance in `isNeutralTimeMap`, in both directions.
  // Without these a tighter tolerance passes every other case unchanged.
  ["barely-neutral", { points: [{ x: 0.5, y: 0.5 + 5e-10 }], length: 1, denominator: 4 }],
  ["barely-not-neutral", { points: [{ x: 0.5, y: 0.5 + 2e-9 }], length: 1, denominator: 4 }],
];

const PHASES = [0, 0.1, 0.25, 0.4, 0.5, 0.6, 0.75, 0.9, 1, 1.5, -0.25];
const TEMPOS = [120, 60, 33.333333333333336, 240, 0, -10];
const CLOCK_SECONDS = [0, 0.125, 0.5, 1, 1.75, 3.5, 10, -1];

/** The read side of the map, which is what the planner depends on. */
export function timemapFixture(): string {
  const lines: string[] = [
    "# Time Distortion Map conformance fixture - regenerate with scripts/goldens.ts",
    "# maps: id|length|denominator|x:y;x:y;...",
  ];

  for (const [id, map] of TIME_MAPS) {
    const pts = map.points.map((p) => `${p.x}:${p.y}`).join(";");
    lines.push(`${id}|${map.length}|${map.denominator}|${pts}`);
  }

  lines.push("# normalizeTimeMap: id,index,x_bits,y_bits");
  for (const [id, map] of TIME_MAPS) {
    normalizeTimeMap(map).points.forEach((p, i) => {
      lines.push(`${id},${i},${hex(p.x)},${hex(p.y)}`);
    });
  }

  lines.push("# isNeutralTimeMap: id,value");
  for (const [id, map] of TIME_MAPS) {
    lines.push(`${id},${isNeutralTimeMap(map) ? 1 : 0}`);
  }

  lines.push("# realToClock: id,phase_bits,result_bits");
  for (const [id, map] of TIME_MAPS) {
    for (const phase of PHASES) lines.push(`${id},${hex(phase)},${hex(realToClock(map, phase))}`);
  }

  lines.push("# clockToReal: id,phase_bits,result_bits");
  for (const [id, map] of TIME_MAPS) {
    for (const phase of PHASES) lines.push(`${id},${hex(phase)},${hex(clockToReal(map, phase))}`);
  }

  lines.push("# timeMapSeconds: id,tempo_bits,result_bits");
  for (const [id, map] of TIME_MAPS) {
    for (const tempo of TEMPOS) lines.push(`${id},${hex(tempo)},${hex(timeMapSeconds(map, tempo))}`);
  }

  lines.push("# distortClockSeconds: id,tempo_bits,clock_bits,result_bits");
  for (const [id, map] of TIME_MAPS) {
    for (const tempo of TEMPOS) {
      for (const clock of CLOCK_SECONDS) {
        lines.push(`${id},${hex(tempo)},${hex(clock)},${hex(distortClockSeconds(map, tempo, clock))}`);
      }
    }
  }

  return lines.join("\n") + "\n";
}

/* ===== Music: scales, snapping, diatonic transposition ===== */

const SCALE_NAMES = Object.keys(SCALES) as ScaleName[];
const ROOTS = [0, 1, 5, 7, 11];

/**
 * Notes deliberately outside 0..127 as well as inside. The pitch-class
 * arithmetic uses `%`, which in both languages keeps the sign of the dividend,
 * so a negative note is where a careless port breaks.
 */
const NOTES = [-13, -1, 0, 1, 11, 12, 47, 60, 61, 66, 71, 127, 128, 140];

export function musicFixture(): string {
  const lines: string[] = [
    "# music conformance fixture - regenerate with npm run goldens",
    "# scales: name,degree;degree;...",
  ];

  for (const name of SCALE_NAMES) lines.push(`${name},${SCALES[name].join(";")}`);

  lines.push("# snapToScale: scale,root,note,result");
  for (const scale of SCALE_NAMES) {
    for (const root of ROOTS) {
      for (const note of NOTES) lines.push(`${scale},${root},${note},${snapToScale(note, root, scale)}`);
    }
  }

  lines.push("# snapToChord: scale,root,note,result");
  for (const scale of SCALE_NAMES) {
    for (const root of ROOTS) {
      for (const note of NOTES) lines.push(`${scale},${root},${note},${snapToChord(note, root, scale)}`);
    }
  }

  lines.push("# diatonicTranspose: scale,root,note,steps,result");
  for (const scale of SCALE_NAMES) {
    for (const root of [0, 7]) {
      for (const note of [-1, 0, 60, 61, 127]) {
        for (const steps of [-14, -7, -3, -1, 0, 1, 2, 3, 7, 14]) {
          lines.push(`${scale},${root},${note},${steps},${diatonicTranspose(note, root, scale, steps)}`);
        }
      }
    }
  }

  // Halves and negatives, because JavaScript's Math.round goes half toward
  // +Infinity while Rust's f64::round goes half away from zero. A port that
  // reaches for the obvious method breaks here and nowhere else.
  lines.push("# clampMidi: note_bits,result");
  for (const note of [-1.5, -0.5, -0.4, 0, 0.5, 1.5, 2.5, 63.5, 126.5, 127.5, 200]) {
    lines.push(`${hex(note)},${clampMidi(note)}`);
  }

  lines.push("# midiToName: note,name");
  for (const note of NOTES) lines.push(`${note},${midiToName(note)}`);

  return lines.join("\n") + "\n";
}

/* ===== Cyclic variables ===== */

export function cyclicFixture(): string {
  const lines: string[] = [
    "# cyclic conformance fixture - regenerate with npm run goldens",
    "# normalizeCyclicStep(number): input_bits,min,max",
  ];

  for (const v of [-2, -0.5, 0, 0.5, 1.5, 2, 2.5, 4, 4.5, 9]) {
    const r = normalizeCyclicStep(v);
    lines.push(`${hex(v)},${r.min},${r.max}`);
  }

  lines.push("# normalizeCyclicStep(range): min_bits,max_bits,min,max");
  for (const [a, b] of [[0, 4], [4, 0], [2, 2], [-3, 9], [1.5, 2.5], [3, 1]]) {
    const r = normalizeCyclicStep({ min: a, max: b });
    lines.push(`${hex(a)},${hex(b)},${r.min},${r.max}`);
  }

  // A point range consumes no RNG - that is what preserves old seeds - so the
  // draw index must be checked, not only the value.
  lines.push("# pickCyclicLevel: seed,min,max,index,value");
  for (const seed of [1, 42, 0xffffffff]) {
    for (const [min, max] of [[0, 0], [2, 2], [0, 4], [1, 3], [3, 4]]) {
      const rng = new Rng(seed);
      for (let i = 0; i < 8; i++) {
        lines.push(`${seed >>> 0},${min},${max},${i},${pickCyclicLevel({ min, max }, rng)}`);
      }
    }
  }

  // The rule that actually matters: a point consumes no draw, so a sequence
  // mixing points and ranges on ONE generator is the only thing that can catch
  // a port which spends randomness on a point. Resetting per case cannot.
  lines.push("# pickCyclicLevel(sequence): seed,index,min,max,value");
  for (const seed of [1, 42]) {
    const rng = new Rng(seed);
    const steps: ReadonlyArray<readonly [number, number]> = [
      [2, 2], [0, 4], [1, 1], [1, 3], [3, 3], [0, 2], [4, 4], [2, 4],
      [0, 0], [1, 4], [3, 3], [0, 1],
    ];
    steps.forEach(([min, max], i) => {
      lines.push(`${seed >>> 0},${i},${min},${max},${pickCyclicLevel({ min, max }, rng)}`);
    });
  }

  lines.push("# cyclicLengthFromStepIndex: input_bits,result");
  for (const v of [-5, -0.5, 0, 0.5, 1, 7.5, 15, 16, 99]) {
    lines.push(`${hex(v)},${cyclicLengthFromStepIndex(v)}`);
  }

  return lines.join("\n") + "\n";
}

/* ===== Transform: the ordered chain applied per step ===== */

const NOTE_ORDERS: NoteOrder[] = ["original", "reverse", "random", "random-walk", "brownian"];

/**
 * Note Order mixes covering each branch of `nextMixedStepIndex` and the two
 * boundaries between them.
 */
const MIXES: ReadonlyArray<readonly [string, NoteOrderMix]> = [
  ["all-original", { original: 100, cyclic: 0, utterly: 0 }],
  ["all-cyclic", { original: 0, cyclic: 100, utterly: 0 }],
  ["all-utterly", { original: 0, cyclic: 0, utterly: 100 }],
  ["even", { original: 34, cyclic: 33, utterly: 33 }],
  ["classic", { original: 60, cyclic: 30, utterly: 10 }],
  ["none", { original: 0, cyclic: 0, utterly: 0 }],
];

export function transformFixture(): string {
  const lines: string[] = [
    "# transform conformance fixture - regenerate with npm run goldens",
    "# stepDurationSeconds: tempo_bits,numerator_bits,denominator_bits,result_bits",
  ];

  for (const tempo of [120, 60, 33.333333333333336, 240]) {
    for (const [n, d] of [[1, 4], [1, 8], [2, 4], [3, 16], [1, 1], [5, 7]]) {
      lines.push(`${hex(tempo)},${hex(n)},${hex(d)},${hex(stepDurationSeconds(tempo, n, d))}`);
    }
  }

  // Halves and out-of-range endpoints: the same Math.round trap as clampMidi,
  // and the low/high swap that normalizeVelocityRange is there to fix.
  lines.push("# normalizeVelocityRange: low_bits,high_bits,low,high");
  for (const [lo, hi] of [[0, 127], [127, 0], [-20, 200], [63.5, 64.5], [-0.5, 0.5], [10, 10]]) {
    const r = normalizeVelocityRange({ low: lo, high: hi });
    lines.push(`${hex(lo)},${hex(hi)},${r.low},${r.high}`);
  }

  lines.push("# velocityForAccent: low_bits,high_bits,level_bits,result");
  for (const [lo, hi] of [[0, 127], [40, 100], [127, 0], [64, 64]]) {
    for (const level of [-1, 0, 0.5, 1, 1.5, 2, 3, 4, 4.5, 9]) {
      lines.push(`${hex(lo)},${hex(hi)},${hex(level)},${velocityForAccent({ low: lo, high: hi }, level)}`);
    }
  }

  lines.push("# gate: seed,density_bits,index,value");
  for (const seed of [1, 42]) {
    for (const density of [0, 0.25, 0.75, 1]) {
      const rng = new Rng(seed);
      for (let i = 0; i < 8; i++) {
        lines.push(`${seed >>> 0},${hex(density)},${i},${gate(density, rng) ? 1 : 0}`);
      }
    }
  }

  // The cursor is carried, not reset, so the sequence checks that pos, last and
  // the Brownian value all advance the way the TypeScript advances them.
  lines.push("# nextStepIndex: order,seed,length,step,index,pos,last,bval_bits");
  for (const order of NOTE_ORDERS) {
    for (const seed of [1, 42]) {
      for (const length of [1, 4, 16]) {
        const rng = new Rng(seed);
        let cursor: NoteOrderCursor = { pos: 0, last: -1, bval: 0.5 };
        for (let i = 0; i < 12; i++) {
          const r = nextStepIndex(order, cursor, length, rng);
          cursor = r.cursor;
          lines.push(`${order},${seed >>> 0},${length},${i},${r.index},${cursor.pos},${cursor.last},${hex(cursor.bval)}`);
        }
      }
    }
  }

  lines.push("# makeCyclicOrder: length,seed,order");
  for (const length of [1, 2, 5, 16]) {
    for (const seed of [1, 42, 0xffffffff]) {
      lines.push(`${length},${seed >>> 0},${makeCyclicOrder(length, seed).join(";")}`);
    }
  }

  lines.push("# noteOrderMixFromEdges: original_bits,utterly_bits,original,cyclic,utterly");
  for (const [o, u] of [[0, 0], [100, 0], [0, 100], [60, 10], [60, 80], [-10, 200], [33.5, 33.5]]) {
    const m = noteOrderMixFromEdges(o, u);
    lines.push(`${hex(o)},${hex(u)},${m.original},${m.cyclic},${m.utterly}`);
  }

  lines.push("# nextMixedStepIndex: mix,seed,length,step,index,source,pos,last");
  for (const [id, mix] of MIXES) {
    for (const seed of [1, 42]) {
      for (const length of [1, 4, 16]) {
        const rng = new Rng(seed);
        let cursor: NoteOrderCursor = { pos: 0, last: -1, bval: 0.5 };
        for (let i = 0; i < 12; i++) {
          const r = nextMixedStepIndex(mix, cursor, length, rng);
          cursor = r.cursor;
          lines.push(`${id},${seed >>> 0},${length},${i},${r.index},${r.source},${cursor.pos},${cursor.last}`);
        }
      }
    }
  }

  return lines.join("\n") + "\n";
}

/** Voice counts the traces are pinned at: one, the classic four, and the ends. */
export const TRACE_VOICE_COUNTS = [1, 4, 8, 16] as const;

/** Every fixture file, keyed by its name inside `__goldens__`. */
export function goldenFiles(): Record<string, string> {
  const files: Record<string, string> = {
    "rng.txt": rngFixture(),
    "timemap.txt": timemapFixture(),
    "music.txt": musicFixture(),
    "cyclic.txt": cyclicFixture(),
    "transform.txt": transformFixture(),
  };

  for (const voices of TRACE_VOICE_COUNTS) {
    const padded = String(voices).padStart(2, "0");

    // Traces are stored with a trailing newline, as text files are.
    files[`voices-${padded}.trace`] = traceDefaultProject(voices) + "\n";

    // The exact state the trace was planned from. Handing the Rust planner the
    // project rather than asking it to rebuild one isolates the stage under
    // test: a divergence is then the planner's, not the project builder's.
    files[`project-${padded}.json`] =
      JSON.stringify(traceFixture(voices), null, 2) + "\n";

    // The rich pair, which is what actually pins the planner's behaviour.
    files[`rich-${padded}.trace`] = traceRichProject(voices) + "\n";
    files[`rich-project-${padded}.json`] =
      JSON.stringify(traceRichFixture(voices), null, 2) + "\n";

    // Scale Snap and Chord Tones, which the rich fixture hides behind Diatonic.
    files[`guard-${padded}.trace`] = traceGuardProject(voices) + "\n";
    files[`guard-project-${padded}.json`] =
      JSON.stringify(traceGuardFixture(voices), null, 2) + "\n";

    // Seconds and the Rhythm multiplier, as exact bits.
    files[`detail-${padded}.txt`] = traceDetailProject(voices) + "\n";
  }

  return files;
}
