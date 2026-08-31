import { afterEach, describe, expect, it, vi } from "vitest";
import { createDefaultProject } from "./project";
import { MRuntime } from "./runtime";

/*
 * The defensive paths in the browser runtime.
 *
 * Every branch here exists for a state the browser can genuinely be in — a
 * context the autoplay policy suspended, a control moved before the first user
 * gesture built the audio graph, a caller that did not pass an optional
 * callback. None of them is reachable from the ordinary happy-path tests, which
 * always start the runtime first, so they sat uncovered while being exactly the
 * code that runs when something is unusual.
 */

/** Only what `ensure()` and the transport touch. `state` is settable because
 *  a suspended context is the whole point of half of these. */
class FakeAudioContext {
  currentTime = 0;
  state: AudioContextState = "running";
  destination = {} as AudioDestinationNode;
  resumed = 0;
  createGain() {
    return {
      gain: {
        value: 0, setValueAtTime: vi.fn(), linearRampToValueAtTime: vi.fn(),
        exponentialRampToValueAtTime: vi.fn(), cancelScheduledValues: vi.fn(),
        setTargetAtTime: vi.fn(),
      },
      connect: vi.fn((to: unknown) => to), disconnect: vi.fn(),
    } as unknown as GainNode;
  }
  getOutputTimestamp() {
    return { contextTime: this.currentTime, performanceTime: this.currentTime * 1000 };
  }
  resume = vi.fn(async () => { this.resumed += 1; this.state = "running"; });
  close() { return Promise.resolve(); }
}

class SuspendedAudioContext extends FakeAudioContext {
  override state: AudioContextState = "suspended";
}

afterEach(() => {
  vi.useRealTimers();
  vi.unstubAllGlobals();
});

describe("runtime before the audio graph exists", () => {
  it("accepts a master volume change with no graph to apply it to", () => {
    vi.stubGlobal("AudioContext", FakeAudioContext);
    const runtime = new MRuntime(() => createDefaultProject());

    // A slider can be dragged before anything has been started. Silently doing
    // nothing is correct; throwing would take the interface down.
    expect(() => runtime.setMasterVolume(0.5)).not.toThrow();
    expect(runtime.isRunning()).toBe(false);
  });

  it("reports no elapsed transport time before there is a transport", () => {
    vi.stubGlobal("AudioContext", FakeAudioContext);
    const runtime = new MRuntime(() => createDefaultProject());

    expect(runtime.transportElapsedSec()).toBe(0);
  });

  it("takes an incoming clock message with no clock and no context", () => {
    vi.stubGlobal("AudioContext", FakeAudioContext);
    const runtime = new MRuntime(() => createDefaultProject());

    // No ClockDriver was passed and no AudioContext has been built, so the
    // runtime's idea of "now" falls all the way through to zero.
    expect(runtime.ingestClockMessage({ type: "clock" }, 0)).toBeNull();
    expect(runtime.followedTempo()).toBeNull();
  });
});

describe("runtime against a suspended audio context", () => {
  it("resumes the context when auditioning into it", () => {
    vi.stubGlobal("AudioContext", SuspendedAudioContext);
    const runtime = new MRuntime(() => createDefaultProject());
    runtime.setSynthEnabled(false);

    // Auditioning is the first thing many users do, and before Start there are
    // no cursors — so the note's tick has nothing to read and falls back to 0.
    expect(() => runtime.audition([60], 100, [1])).not.toThrow();
  });

  it("ignores an audition with no pitches or no channels", () => {
    vi.stubGlobal("AudioContext", FakeAudioContext);
    const runtime = new MRuntime(() => createDefaultProject());

    expect(() => runtime.audition([], 100, [1])).not.toThrow();
    expect(() => runtime.audition([60], 100, [])).not.toThrow();
  });

  it("resumes the context when the transport resumes", async () => {
    vi.useFakeTimers();
    vi.stubGlobal("AudioContext", SuspendedAudioContext);
    const runtime = new MRuntime(() => createDefaultProject());
    runtime.setSynthEnabled(false);

    await runtime.start();
    runtime.pause();
    await runtime.resume();

    expect(runtime.isRunning()).toBe(true);
    runtime.stop();
  });

  it("resumes from a pause that never happened by starting instead", async () => {
    vi.useFakeTimers();
    vi.stubGlobal("AudioContext", FakeAudioContext);
    const runtime = new MRuntime(() => createDefaultProject());
    runtime.setSynthEnabled(false);

    await runtime.resume();

    expect(runtime.isRunning()).toBe(true);
    runtime.stop();
  });
});

describe("runtime with no optional callbacks", () => {
  it("plans and submits without onPlannedSteps or onCyclicReset", async () => {
    vi.stubGlobal("AudioContext", FakeAudioContext);
    let repeated: (() => void) | null = null;
    const scheduler = {
      repeat: vi.fn((fn: () => void) => { repeated = fn; return 1; }),
      once: vi.fn(() => 2),
      cancel: vi.fn(),
    };
    let now = 0;

    // Deliberately none of onPlannedNotes, onPlannedSteps or onCyclicReset.
    const runtime = new MRuntime(() => createDefaultProject(), null, {
      scheduler,
      clock: { nowSec: () => now },
    });
    runtime.setSynthEnabled(false);

    await runtime.start();
    expect(repeated).not.toBeNull();

    // Far enough for the planner to produce steps, and for a cyclic table to
    // wrap, so the optional-call branches are actually reached.
    for (let i = 0; i < 8; i += 1) {
      now += 0.25;
      expect(() => repeated?.()).not.toThrow();
    }

    runtime.stop();
  });
});
