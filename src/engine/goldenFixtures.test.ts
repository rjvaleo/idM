import { describe, expect, it } from "vitest";
import { goldenFiles, rngFixture, TRACE_VOICE_COUNTS } from "./goldenFixtures";

/*
 * The committed fixtures, as text. Vite's raw glob rather than `node:fs`, for
 * the reason given in `goldenTrace.test.ts`: this tsconfig carries no Node
 * types. Eager, so a missing fixture fails at load rather than silently.
 */
const COMMITTED = {
  ...import.meta.glob("./__goldens__/*.trace", { query: "?raw", import: "default", eager: true }),
  ...import.meta.glob("./__goldens__/*.txt", { query: "?raw", import: "default", eager: true }),
} as Record<string, string>;

describe("golden fixtures", () => {
  /*
   * The point of this file. The Rust engine is checked against these exact
   * bytes, so a change to `rng.ts` that nobody meant to make must fail here —
   * on the TypeScript side, where it was introduced — rather than only in a
   * Rust build somebody may not have run.
   */
  it("match what the engine generates today, byte for byte", () => {
    for (const [name, expected] of Object.entries(goldenFiles())) {
      const committed = COMMITTED[`./__goldens__/${name}`];
      expect(committed, `no committed fixture named ${name}`).toBeDefined();
      expect(committed, `${name} has drifted - run: npx vite-node scripts/goldens.ts`)
        .toBe(expected);
    }
  });

  it("covers every voice count the traces are pinned at", () => {
    const names = Object.keys(goldenFiles());
    for (const voices of TRACE_VOICE_COUNTS) {
      expect(names).toContain(`voices-${String(voices).padStart(2, "0")}.trace`);
    }
  });

  it("records raw integers for the draws, so no float formatting is involved", () => {
    const draws = rngFixture()
      .split("\n")
      .slice(2)
      .filter((line) => line !== "" && !line.startsWith("#"))
      .slice(0, 32);

    expect(draws).toHaveLength(32);
    for (const line of draws) {
      const [, , value] = line.split(",");
      expect(value).toMatch(/^\d+$/);
      expect(Number(value)).toBeLessThan(2 ** 32);
    }
  });

  it("exercises seeds at both ends of the u32 range", () => {
    const text = rngFixture();
    expect(text).toContain("\n0,0,");
    expect(text).toContain(`\n${0xffffffff},0,`);
  });
});
