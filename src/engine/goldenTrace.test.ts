import { describe, expect, it } from "vitest";
import { traceDefaultProject, traceFixture, traceProject } from "./goldenTrace";
import { createDefaultProject, clampVoiceCount, voiceCount,
  DEFAULT_VOICE_COUNT, MAX_VOICE_COUNT, MIN_VOICE_COUNT } from "./project";
import { decodeDocument, encodeDocument, DOCUMENT_VERSION,
  type DocumentSource } from "./document";
import { makePresetPositions } from "./variables";
import { DEFAULT_OPTIONS } from "./options";
import type { CyclicPositionBanks, CyclicPositionLengths, CyclicVariable } from "./types";

const KINDS: CyclicVariable[] = ["accent", "legato", "rhythm"];

/*
 * The committed traces, as text.
 *
 * Vite's raw glob rather than `node:fs`: this tsconfig carries no Node types,
 * and the same trick is already how `params.test.ts` reads sources. Eager, so a
 * missing golden is a load-time failure rather than a promise nobody awaited.
 */
const GOLDENS = import.meta.glob("./__goldens__/*.trace", {
  query: "?raw", import: "default", eager: true,
}) as Record<string, string>;

const golden = (n: number): string => {
  const key = `./__goldens__/voices-${String(n).padStart(2, "0")}.trace`;
  const text = GOLDENS[key];
  if (text === undefined) throw new Error(`no committed golden for ${n} voices (${key})`);
  return text.trim();
};

const COUNTS = [1, 4, 8, 16] as const;

describe("the Voice count is a property of the project", () => {
  it("builds every per-Voice array to the same length", () => {
    for (const n of COUNTS) {
      const p = createDefaultProject(n);
      expect(voiceCount(p), `${n} voices`).toBe(n);
      expect(p.cyclic.accent, `${n} accent`).toHaveLength(n);
      expect(p.cyclic.legato, `${n} legato`).toHaveLength(n);
      expect(p.cyclic.rhythm, `${n} rhythm`).toHaveLength(n);
      expect(p.cyclicLengths.accent, `${n} accent lengths`).toHaveLength(n);
      expect(p.cyclicLengths.legato, `${n} legato lengths`).toHaveLength(n);
      expect(p.cyclicLengths.rhythm, `${n} rhythm lengths`).toHaveLength(n);
    }
  });

  it("still defaults to the four M shipped", () => {
    expect(voiceCount(createDefaultProject())).toBe(DEFAULT_VOICE_COUNT);
    expect(DEFAULT_VOICE_COUNT).toBe(4);
  });

  it("clamps a count from outside the supported range", () => {
    expect(clampVoiceCount(0)).toBe(MIN_VOICE_COUNT);
    expect(clampVoiceCount(99)).toBe(MAX_VOICE_COUNT);
    expect(clampVoiceCount(Number.NaN)).toBe(DEFAULT_VOICE_COUNT);
    expect(clampVoiceCount(8.4)).toBe(8);
  });

  it("gives every Voice its own channel", () => {
    const p = createDefaultProject(16);
    expect(new Set(p.voices.map((v) => v.channel)).size).toBe(16);
  });
});

describe("golden traces", () => {
  // The point of M0. If the Rust port emits these same bytes it is correct;
  // if it does not, it is wrong — no listening required.
  it.each(COUNTS)("matches the committed trace at %i voices", (n) => {
    expect(traceDefaultProject(n)).toBe(golden(n));
  });

  it("does not depend on how the span is divided into windows", () => {
    // A scheduling boundary must not be able to change the music. This is the
    // load-bearing property: without it no other trace assertion means anything.
    for (const n of COUNTS) {
      const once = traceProject(traceFixture(n), 1, { spanSec: 8, windows: 1 });
      const many = traceProject(traceFixture(n), 1, { spanSec: 8, windows: 16 });
      expect(many, `${n} voices`).toBe(once);
      expect(once, `${n} voices vs golden`).toBe(golden(n));
    }
  });

  it("changes with the seed, so the trace is actually reading the RNG", () => {
    expect(traceDefaultProject(8, 2)).not.toBe(traceDefaultProject(8, 1));
  });

  it("differs at every Voice count, so a count change cannot pass unnoticed", () => {
    const traces = COUNTS.map((n) => traceDefaultProject(n));
    expect(new Set(traces).size).toBe(COUNTS.length);
  });

  it("scales the number of sounding lanes with the count", () => {
    for (const n of COUNTS) {
      const lanes = new Set(golden(n).split("\n").map((line: string) => line.split(",")[1]));
      expect(lanes.size, `${n} voices`).toBe(n);
    }
  });
});

describe("document v3", () => {
  /** A full, valid save source at a given Voice count. */
  const sourceFor = (voices: number): DocumentSource => {
    const project = createDefaultProject(voices);
    return {
      project,
      positions: makePresetPositions(),
      snapshots: Array(26).fill(null),
      slideshows: Array.from({ length: 9 }, () => ({ events: [], loopAtSec: null })),
      currentSnapshot: null,
      snapshotQuantize: 0,
      arrows: {},
      patternGroup: 0,
      selectedVoice: 0,
      tempoRange: { low: 80, high: 160 },
      syncRatio: 4,
      syncRatioDirection: "out",
      robotRange: { x: 0.15, y: 0.15 },
      robotTimeBase: 4,
      cyclicPositions: Object.fromEntries(KINDS.map((kind) => [
        kind, Array.from({ length: 6 }, () => project.cyclic[kind].map((v) => [...v])),
      ])) as CyclicPositionBanks,
      cyclicLengths: Object.fromEntries(KINDS.map((kind) => [
        kind, Array.from({ length: 6 }, () => Array(voices).fill(16)),
      ])) as CyclicPositionLengths,
      activeCyclicPositions: { accent: 0, legato: 0, rhythm: 0 },
      options: DEFAULT_OPTIONS,
    };
  };

  const roundTrip = (voices: number) => {
    const decoded = decodeDocument(JSON.parse(JSON.stringify(encodeDocument(sourceFor(voices)))));
    if (!decoded.ok) throw new Error(decoded.error);
    return decoded.document;
  };

  it("round-trips every supported Voice count", () => {
    for (const n of COUNTS) {
      expect(voiceCount(roundTrip(n).project), `${n} voices`).toBe(n);
    }
  });

  it("stamps version 3", () => {
    expect(roundTrip(4).version).toBe(DOCUMENT_VERSION);
    expect(DOCUMENT_VERSION).toBe(3);
  });

  it("still opens a version 2 document, which always had four", () => {
    // v2 is only distinguishable by its stamp: the payload shape is the same
    // for four Voices, which is why the bump is about refusing *newer* files
    // rather than reading older ones differently.
    const encoded = JSON.parse(JSON.stringify(roundTrip(4)));
    encoded.version = 2;
    const decoded = decodeDocument(encoded);
    expect(decoded.ok).toBe(true);
    if (decoded.ok) expect(voiceCount(decoded.document.project)).toBe(4);
  });

  it("refuses a Voice count outside the supported range", () => {
    const encoded = JSON.parse(JSON.stringify(roundTrip(4)));
    encoded.project.voices = [];
    expect(decodeDocument(encoded).ok).toBe(false);
    const tooMany = JSON.parse(JSON.stringify(roundTrip(4)));
    tooMany.project.voices = Array.from({ length: 17 }, () => tooMany.project.voices[0]);
    expect(decodeDocument(tooMany).ok).toBe(false);
  });
});
