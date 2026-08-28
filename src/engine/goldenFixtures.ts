// The cross-language conformance fixtures, as data.
//
// Both the writer (`scripts/goldens.ts`) and the guard
// (`goldenFixtures.test.ts`) build the expected bytes from here, so the files
// on disk cannot drift from the engine without one of them failing.

import { Rng, BrownianWalk } from "./rng";
import { traceDefaultProject } from "./goldenTrace";
import {
  neutralTimeMap, normalizeTimeMap, isNeutralTimeMap, realToClock, clockToReal,
  timeMapSeconds, distortClockSeconds, type TimeMap,
} from "./timemap";

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

/** Voice counts the traces are pinned at: one, the classic four, and the ends. */
export const TRACE_VOICE_COUNTS = [1, 4, 8, 16] as const;

/** Every fixture file, keyed by its name inside `__goldens__`. */
export function goldenFiles(): Record<string, string> {
  const files: Record<string, string> = {
    "rng.txt": rngFixture(),
    "timemap.txt": timemapFixture(),
  };

  for (const voices of TRACE_VOICE_COUNTS) {
    // Traces are stored with a trailing newline, as text files are.
    files[`voices-${String(voices).padStart(2, "0")}.trace`] =
      traceDefaultProject(voices) + "\n";
  }

  return files;
}
