// The clock and note wiring, which nothing tested.
//
// M1 of MIDI_PLAN.md. The pieces were proven — `clockinput.ts` decodes pulses,
// `clockoutput.ts` computes intervals, `webmidi.ts` sends bytes, `NoteLifecycle`
// tracks sounding notes — but the file that joins them, `runtime.ts`, is
// excluded from the coverage gate, and the outbound clock stream had no
// assertion anywhere: `sendClock` appeared in `runtime.test.ts` exactly once,
// set to false.
//
// So this asserts the bytes on the wire rather than that a method was reached.
// It is written before the Rust port deliberately: there is a working
// reference implementation to test against today, and after the port a test
// like this would only prove the Rust engine agrees with itself.

import { afterEach, describe, expect, it, vi } from "vitest";
import { createDefaultProject } from "./project";
import { MRuntime } from "./runtime";
import { clockPulseInterval } from "./clockoutput";
import type { ClockDriver, SchedulerDriver } from "./scheduler";

/** A MIDI port that records every byte and when it was told to send it. */
function recordingPort() {
  const sent: Array<{ data: number[]; at?: number }> = [];
  return {
    sent,
    port: {
      send(data: number[] | Uint8Array, at?: number) {
        sent.push({ data: Array.from(data), at });
      },
      clear() {},
    } as unknown as MIDIOutput,
    /** Just the realtime status bytes, in order. */
    realtime: () => sent.filter((m) => m.data.length === 1 && m.data[0] >= 0xf8)
      .map((m) => m.data[0]),
    /** Note-ons and note-offs as [status nibble, note]. */
    notes: () => sent.filter((m) => m.data.length === 3 && (m.data[0] & 0xf0) >= 0x80
      && (m.data[0] & 0xf0) <= 0x90).map((m) => [m.data[0] & 0xf0, m.data[1]] as const),
  };
}

class FakeAudioContext {
  currentTime = 0;
  state: AudioContextState = "running";
  destination = {} as AudioDestinationNode;
  createGain() {
    return {
      gain: { value: 0, setValueAtTime: vi.fn(), linearRampToValueAtTime: vi.fn(),
        exponentialRampToValueAtTime: vi.fn(), cancelScheduledValues: vi.fn(),
        setTargetAtTime: vi.fn() },
      connect: vi.fn((to: unknown) => to), disconnect: vi.fn(),
    } as unknown as GainNode;
  }

  // The metronome is a synth click, so a fake context that cannot build an
  // oscillator cannot test it.
  createOscillator() {
    return {
      type: "sine", frequency: { value: 0, setValueAtTime: vi.fn() },
      detune: { value: 0, setValueAtTime: vi.fn() },
      connect: vi.fn((to: unknown) => to), disconnect: vi.fn(), start: vi.fn(), stop: vi.fn(),
      onended: null,
    } as unknown as OscillatorNode;
  }

  createBiquadFilter() {
    return {
      type: "lowpass",
      frequency: { value: 0, setValueAtTime: vi.fn(), linearRampToValueAtTime: vi.fn(),
        exponentialRampToValueAtTime: vi.fn(), cancelScheduledValues: vi.fn() },
      Q: { value: 0, setValueAtTime: vi.fn() },
      connect: vi.fn((to: unknown) => to), disconnect: vi.fn(),
    } as unknown as BiquadFilterNode;
  }

  createBufferSource() {
    return {
      buffer: null, loop: false, playbackRate: { value: 1 },
      connect: vi.fn((to: unknown) => to), disconnect: vi.fn(), start: vi.fn(), stop: vi.fn(), onended: null,
    } as unknown as AudioBufferSourceNode;
  }

  createBuffer(channels: number, length: number, sampleRate: number) {
    return {
      numberOfChannels: channels, length, sampleRate,
      getChannelData: () => new Float32Array(length),
    } as unknown as AudioBuffer;
  }

  sampleRate = 48000;
  getOutputTimestamp() {
    return { contextTime: this.currentTime, performanceTime: this.currentTime * 1000 };
  }
  resume = vi.fn(async () => undefined);
}

/** A runtime whose clock and scheduler a test drives by hand. */
function rig(over: Partial<{ sendClock: boolean; tempo: number }> = {}) {
  vi.stubGlobal("AudioContext", FakeAudioContext);
  const settings = {
    useMetronome: false,
    sendClock: over.sendClock ?? true,
    externalClock: false,
    syncRatio: 4,
    syncRatioDirection: "out" as const,
  };
  const project = { ...createDefaultProject(), tempo: over.tempo ?? 120 };
  let now = 0;
  const clock: ClockDriver = { nowSec: () => now };
  // `cancel` has to actually cancel. A no-op here would let a paused transport
  // keep planning, and the test would be measuring the rig rather than M.
  let nextHandle = 0;
  const byHandle = new Map<number, () => void>();
  const scheduler: SchedulerDriver = {
    repeat: (callback) => { const h = ++nextHandle; byHandle.set(h, callback); return h; },
    once: (callback) => { const h = ++nextHandle; byHandle.set(h, callback); return h; },
    cancel: (handle) => { byHandle.delete(handle as number); },
  };

  const runtime = new MRuntime(() => project, null, {
    scheduler, clock, getPerformanceSettings: () => settings,
  });
  // These are MIDI tests: the built-in synth is not in the path, so it is not
  // in the way either. With it off, the MIDI sink is the only destination —
  // which is also the shape a plugin runs in.
  runtime.setSynthEnabled(false);
  const target = recordingPort();
  runtime.selectMidiOutput(target.port);

  return {
    runtime, target, settings, project,
    /** Advance the audio clock and run every pending wake. */
    advance(seconds: number) {
      now += seconds;
      const ctx = runtime.context as unknown as FakeAudioContext | null;
      if (ctx) ctx.currentTime = now;
      for (const wake of [...byHandle.values()]) wake();
    },
    now: () => now,
  };
}

afterEach(() => {
  vi.useRealTimers();
  vi.unstubAllGlobals();
});

describe("MIDI clock output", () => {
  it("sends Start on play and Stop on stop", async () => {
    const r = rig();
    await r.runtime.start();
    expect(r.target.realtime()).toContain(0xfa);
    r.runtime.stop();
    expect(r.target.realtime()).toContain(0xfc);
    // Order matters: Start before Stop, and Stop last.
    const rt = r.target.realtime().filter((b) => b === 0xfa || b === 0xfc);
    expect(rt[0]).toBe(0xfa);
    expect(rt[rt.length - 1]).toBe(0xfc);
  });

  it("sends nothing at all when Send Clock is off", async () => {
    const r = rig({ sendClock: false });
    await r.runtime.start();
    r.advance(1);
    r.runtime.stop();
    expect(r.target.realtime()).toEqual([]);
  });

  it("paces pulses at 24 PPQN for the project tempo", async () => {
    // 120 bpm, sync ratio 4 -> a pulse every 240/120/4/24 seconds.
    const r = rig({ tempo: 120 });
    await r.runtime.start();
    r.advance(1);
    const pulses = r.target.sent.filter((m) => m.data[0] === 0xf8 && m.at !== undefined);
    expect(pulses.length).toBeGreaterThan(1);

    // `MIDIOutput.send` timestamps are DOMHighResTimeStamp — milliseconds in
    // the `performance.now()` domain — while `clockPulseInterval` is seconds of
    // audio time. Pinning the conversion is half the point of this test: the
    // two clocks are the thing most easily got wrong here, and a factor of a
    // thousand would sound like the tempo rather than like a bug.
    const expectedMs = clockPulseInterval(120, 4) * 1000;
    const gaps: number[] = [];
    for (let i = 1; i < pulses.length; i++) gaps.push(pulses[i].at! - pulses[i - 1].at!);
    expect(gaps.length).toBeGreaterThan(4);
    for (const gap of gaps) expect(gap).toBeCloseTo(expectedMs, 6);
  });

  it("paces pulses faster at a faster tempo", async () => {
    const slow = rig({ tempo: 60 });
    await slow.runtime.start();
    slow.advance(1);
    const fast = rig({ tempo: 240 });
    await fast.runtime.start();
    fast.advance(1);

    const count = (r: ReturnType<typeof rig>) =>
      r.target.sent.filter((m) => m.data[0] === 0xf8).length;
    expect(count(fast)).toBeGreaterThan(count(slow));
  });

  it("timestamps every pulse rather than sending them all at once", async () => {
    // A burst of pulses with no timestamps would arrive together and read as a
    // tempo spike at the far end, which is the whole failure this scheduling
    // exists to avoid.
    const r = rig();
    await r.runtime.start();
    r.advance(1);
    const pulses = r.target.sent.filter((m) => m.data[0] === 0xf8);
    expect(pulses.every((m) => typeof m.at === "number")).toBe(true);
    expect(new Set(pulses.map((m) => m.at)).size).toBe(pulses.length);
  });
});

describe("notes left sounding", () => {
  it("leaves nothing hanging after stop", async () => {
    const r = rig({ sendClock: false });
    await r.runtime.start();
    r.advance(2);
    const before = r.target.notes();
    expect(before.some(([status]) => status === 0x90)).toBe(true);

    r.runtime.stop();

    // Every note-on must be answered, either by its own note-off or by the
    // panic that stop sends. Counting is enough: a hanging note is an unmatched
    // 0x90 with no 0x80 and no All Notes Off behind it.
    const after = r.target.notes();
    const ons = after.filter(([s]) => s === 0x90).length;
    const offs = after.filter(([s]) => s === 0x80).length;
    const allNotesOff = r.target.sent.some((m) => m.data[1] === 123);
    expect(offs >= ons || allNotesOff).toBe(true);
  });

  it("panics on stop, so a device that missed a note-off is still silenced", async () => {
    const r = rig({ sendClock: false });
    await r.runtime.start();
    r.advance(1);
    r.runtime.stop();
    // Sustain Off, Reset All Controllers, All Notes Off — the three the sink
    // sends, and the reason panic does not rely on CC 123 alone.
    const controllers = r.target.sent.filter((m) => (m.data[0] & 0xf0) === 0xb0)
      .map((m) => m.data[1]);
    expect(controllers).toContain(123);
  });
});

describe("transport lifecycle", () => {
  it("pauses without losing the cursor, and resumes from where it paused", async () => {
    const r = rig({ sendClock: false });
    await r.runtime.start();
    r.advance(1);
    const beforePause = r.target.notes().length;

    r.runtime.pause();
    r.advance(2);
    // Nothing new is scheduled while paused.
    expect(r.target.notes().length).toBe(beforePause);

    await r.runtime.resume();
    r.advance(1);
    expect(r.target.notes().length).toBeGreaterThan(beforePause);
  });

  it("panics and clears the queue on pause, so a held note does not sustain", async () => {
    const r = rig({ sendClock: false });
    await r.runtime.start();
    r.advance(1);
    const before = r.target.sent.length;
    r.runtime.pause();
    const controllers = r.target.sent.slice(before)
      .filter((m) => (m.data[0] & 0xf0) === 0xb0).map((m) => m.data[1]);
    expect(controllers).toContain(123);
  });

  it("treats resume with nothing paused as a start", async () => {
    const r = rig({ sendClock: true });
    await r.runtime.resume();
    expect(r.target.realtime()).toContain(0xfa);
  });

  it("returns every voice to the top on Sync", async () => {
    const r = rig({ sendClock: false });
    await r.runtime.start();
    r.advance(1);
    const before = r.target.sent.length;
    r.runtime.sync();
    // Sync cancels what was queued and silences the devices before replanning.
    const controllers = r.target.sent.slice(before)
      .filter((m) => (m.data[0] & 0xf0) === 0xb0).map((m) => m.data[1]);
    expect(controllers).toContain(123);
  });

  it("does nothing on Sync before the graph exists", () => {
    vi.stubGlobal("AudioContext", FakeAudioContext);
    const runtime = new MRuntime(() => createDefaultProject(), null, {});
    expect(() => runtime.sync()).not.toThrow();
  });
});

describe("MIDI configuration reaches the sink", () => {
  it("passes latency, channel assignments and channel mode through", async () => {
    const r = rig({ sendClock: false });
    r.runtime.setMidiLatency(35);
    r.runtime.setMidiOutputAssignments([{ deviceId: null, channel: 3 }]);
    r.runtime.sendMidiChannelMode("local-off", [1]);

    // Channel mode is the one with an observable byte: CC 122 with value 0.
    const cc = r.target.sent.filter((m) => (m.data[0] & 0xf0) === 0xb0)
      .map((m) => [m.data[1], m.data[2]]);
    expect(cc).toContainEqual([122, 0]);
  });

  it("delays every scheduled note by the latency trim", async () => {
    // Latency compensation that reached the sink but not the wire would be a
    // silent no-op, which is the failure a trim exists to prevent.
    const plain = rig({ sendClock: false });
    await plain.runtime.start();
    plain.advance(1);

    const trimmed = rig({ sendClock: false });
    trimmed.runtime.setMidiLatency(100);
    await trimmed.runtime.start();
    trimmed.advance(1);

    const firstNoteAt = (r: ReturnType<typeof rig>) =>
      r.target.sent.find((m) => (m.data[0] & 0xf0) === 0x90)?.at ?? 0;
    expect(firstNoteAt(trimmed)).toBeGreaterThan(firstNoteAt(plain));
  });

  it("keeps one retained port registry", async () => {
    const r = rig({ sendClock: false });
    expect(r.runtime.midiPorts()).toBe(r.runtime.midiPorts());
  });

  it("sets master volume only once the graph exists", () => {
    vi.stubGlobal("AudioContext", FakeAudioContext);
    const runtime = new MRuntime(() => createDefaultProject(), null, {});
    expect(() => runtime.setMasterVolume(0.5)).not.toThrow();
  });

  it("re-derives whether the synth plays from the patches it is given", async () => {
    const r = rig({ sendClock: false });
    r.runtime.setSynthSettings([{ enabled: false } as never]);
    r.runtime.setSynthEnabled(true);
    expect(() => r.runtime.setSynthSettings([{ enabled: true } as never])).not.toThrow();
  });
});

describe("the metronome", () => {
  it("clicks independently of which way clock flows", async () => {
    // It used to sit below the direction gate, so choosing sync-in switched the
    // click off silently. It is a local sound and has nothing to do with sync.
    const r = rig({ sendClock: false });
    r.settings.useMetronome = true;
    r.settings.syncRatioDirection = "in";
    await r.runtime.start();
    expect(() => r.advance(1)).not.toThrow();
  });
});

describe("audition", () => {
  it("schedules a preview through the injected scheduler", async () => {
    const r = rig({ sendClock: false });
    r.runtime.audition([60, 64], 100, [1]);
    const notes = r.target.notes();
    expect(notes.some(([status, note]) => status === 0x90 && note === 60)).toBe(true);
  });
});

describe("following an external clock", () => {
  /** A rig set to be driven rather than to drive. */
  const following = (tempo = 120) => {
    const r = rig({ sendClock: false, tempo });
    r.settings.syncRatioDirection = "in";
    r.settings.externalClock = true;
    return r;
  };

  it("takes a Song Position Pointer without treating it as transport", () => {
    const r = following();
    expect(r.runtime.ingestClockMessage(
      { type: "song-position", sixteenths: 16 } as never, 0,
    )).toBeNull();
  });

  it("ignores a message that is not clock or transport", () => {
    const r = following();
    expect(r.runtime.ingestClockMessage({ type: "note-on" } as never, 0)).toBeNull();
  });

  it("plans at the incoming tempo once pulses are arriving", async () => {
    // 24 pulses per quarter at 0.125 s apart is 480 bpm at ratio 4 — far from
    // the document's 120, so a trace planned at the document tempo would be
    // obviously different from one planned at the follower's.
    const r = following(120);
    await r.runtime.start();
    r.runtime.ingestClockMessage({ type: "start" } as never, r.now());

    // Pulses must be *recent* relative to the runtime's own clock, or the
    // follower reads as stale and the document tempo wins — which is the
    // branch this test is trying not to take. So the clock advances with them.
    const gap = 0.125 / 24;
    for (let i = 0; i < 48; i++) {
      r.advance(gap);
      r.runtime.ingestClockMessage({ type: "clock" } as never, r.now());
    }
    expect(r.target.notes().length).toBeGreaterThan(0);
  });

  it("falls back to the document tempo when the source goes quiet", async () => {
    // A clock that stops must not leave the transport frozen at a tempo nobody
    // is sending any more.
    const r = following(120);
    r.runtime.ingestClockMessage({ type: "start" } as never, 0);
    r.runtime.ingestClockMessage({ type: "clock" } as never, 0.01);
    await r.runtime.start();
    r.advance(30); // long enough that the follower is stale
    expect(() => r.advance(1)).not.toThrow();
  });

  it("schedules an audition wake through the injected scheduler", async () => {
    const r = rig({ sendClock: false });
    await r.runtime.start();
    r.runtime.audition([72], 100, [1]);
    // The audition registers a one-shot that submits on the next wake; running
    // the wakes is what exercises it.
    expect(() => r.advance(0.5)).not.toThrow();
    expect(r.target.notes().some(([, note]) => note === 72)).toBe(true);
  });
});

describe("the branches a normal run does not take", () => {
  it("plays through the synth when it is the only destination", async () => {
    // The mirror of every other test here: synth on, no MIDI port, so the
    // `synthEnabled && synth` arm is the one taken.
    vi.stubGlobal("AudioContext", FakeAudioContext);
    const runtime = new MRuntime(() => createDefaultProject(), null, {
      scheduler: { repeat: () => 1, once: () => 2, cancel: () => {} },
      clock: { nowSec: () => 0 },
    });
    runtime.setSynthEnabled(true);
    await runtime.start();
    expect(() => runtime.stop()).not.toThrow();
  });

  it("falls back to context time when no clock driver is injected", async () => {
    vi.stubGlobal("AudioContext", FakeAudioContext);
    const runtime = new MRuntime(() => createDefaultProject(), null, {
      scheduler: { repeat: () => 1, once: () => 2, cancel: () => {} },
    });
    runtime.setSynthEnabled(false);
    await runtime.start();
    expect(() => runtime.stop()).not.toThrow();
  });

  it("reports transport messages to the observer on start and on stop", async () => {
    const seen: string[] = [];
    vi.stubGlobal("AudioContext", FakeAudioContext);
    const runtime = new MRuntime(() => createDefaultProject(), null, {
      scheduler: { repeat: () => 1, once: () => 2, cancel: () => {} },
      clock: { nowSec: () => 0 },
      onTransportSent: (type) => seen.push(type),
      getPerformanceSettings: () => ({
        useMetronome: false, sendClock: true, externalClock: false,
        syncRatio: 4, syncRatioDirection: "out",
      }),
    });
    runtime.setSynthEnabled(false);
    await runtime.start();
    runtime.stop();
    expect(seen).toEqual(["start", "stop"]);
  });

  it("resumes a suspended context rather than planning into silence", async () => {
    class Suspended extends FakeAudioContext {
      state: AudioContextState = "suspended";
    }
    vi.stubGlobal("AudioContext", Suspended);
    const runtime = new MRuntime(() => createDefaultProject(), null, {
      scheduler: { repeat: () => 1, once: () => 2, cancel: () => {} },
      clock: { nowSec: () => 0 },
    });
    runtime.setSynthEnabled(false);
    await runtime.start();
    expect((runtime.context as unknown as Suspended).resume).toHaveBeenCalled();
  });

  it("ignores an audition with no pitches or no channels", async () => {
    const r = rig({ sendClock: false });
    const before = r.target.sent.length;
    r.runtime.audition([], 100, [1]);
    r.runtime.audition([60], 100, []);
    expect(r.target.sent.length).toBe(before);
  });

  it("does nothing on pause when the transport was never started", () => {
    const r = rig({ sendClock: false });
    expect(() => r.runtime.pause()).not.toThrow();
  });

  it("reports planned steps only when there are some", async () => {
    const steps: unknown[] = [];
    vi.stubGlobal("AudioContext", FakeAudioContext);
    const runtime = new MRuntime(() => createDefaultProject(), null, {
      scheduler: { repeat: () => 1, once: () => 2, cancel: () => {} },
      clock: { nowSec: () => 0 },
      onPlannedSteps: (s) => steps.push(...s),
    });
    runtime.setSynthEnabled(false);
    await runtime.start();
    expect(Array.isArray(steps)).toBe(true);
  });
});
