// The cross-language conformance fixtures, as data.
//
// Both the writer (`scripts/goldens.ts`) and the guard
// (`goldenFixtures.test.ts`) build the expected bytes from here, so the files
// on disk cannot drift from the engine without one of them failing.

import { Rng, BrownianWalk } from "./rng";
import { traceDefaultProject } from "./goldenTrace";

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
  const bits = new DataView(new ArrayBuffer(8));
  for (const seed of SEEDS) {
    const walk = new BrownianWalk(new Rng(seed));
    for (let i = 0; i < WALK_DRAWS; i++) {
      bits.setFloat64(0, walk.next());
      lines.push(`${seed >>> 0},${i},${bits.getBigUint64(0).toString(16).padStart(16, "0")}`);
    }
  }

  return lines.join("\n") + "\n";
}

/** Voice counts the traces are pinned at: one, the classic four, and the ends. */
export const TRACE_VOICE_COUNTS = [1, 4, 8, 16] as const;

/** Every fixture file, keyed by its name inside `__goldens__`. */
export function goldenFiles(): Record<string, string> {
  const files: Record<string, string> = { "rng.txt": rngFixture() };

  for (const voices of TRACE_VOICE_COUNTS) {
    // Traces are stored with a trailing newline, as text files are.
    files[`voices-${String(voices).padStart(2, "0")}.trace`] =
      traceDefaultProject(voices) + "\n";
  }

  return files;
}
