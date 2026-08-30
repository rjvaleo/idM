import { beforeEach, describe, expect, it, vi } from "vitest";
import { MRuntime } from "./runtime";
import { createDefaultProject } from "./project";

/*
 * There is exactly one engine.
 *
 * In the plugin the engine lives in the processor and follows the host's clock.
 * If this one also ran, two engines would disagree about the time and this
 * one's output would have nowhere to go — a plugin webview has no Web MIDI and
 * no audio device. These assert the guard holds at the runtime rather than at
 * any particular button, because a call site can be added and a guard forgotten.
 */
/** The smallest context the runtime will accept, so a real start can be tested
 *  in Node. Only what `ensure()` touches. */
class FakeAudioContext {
  currentTime = 0;
  state: AudioContextState = "running";
  destination = {} as AudioDestinationNode;
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
  resume() { return Promise.resolve(); }
  close() { return Promise.resolve(); }
}

describe("hosted transport", () => {
  beforeEach(() => {
    vi.stubGlobal("AudioContext", FakeAudioContext);
  });

  const build = () => {
    const scheduler = {
      repeat: vi.fn(() => 1),
      once: vi.fn(() => 2),
      cancel: vi.fn(),
    };
    const runtime = new MRuntime(() => createDefaultProject(), null, {
      scheduler,
      clock: { nowSec: () => 0 },
    });
    return { runtime, scheduler };
  };

  it("does not schedule anything once the transport is hosted", async () => {
    const { runtime, scheduler } = build();
    runtime.setHosted(true);

    await runtime.start();

    expect(runtime.isRunning()).toBe(false);
    expect(scheduler.repeat).not.toHaveBeenCalled();
  });

  it("ignores resume, pause and sync while hosted", async () => {
    const { runtime, scheduler } = build();
    runtime.setHosted(true);

    await runtime.resume();
    runtime.pause();
    runtime.sync();

    expect(scheduler.repeat).not.toHaveBeenCalled();
    expect(scheduler.once).not.toHaveBeenCalled();
  });

  it("stops a running engine the moment the transport is handed over", async () => {
    const { runtime, scheduler } = build();

    await runtime.start();
    expect(runtime.isRunning()).toBe(true);

    runtime.setHosted(true);

    expect(runtime.isRunning()).toBe(false);
    expect(scheduler.cancel).toHaveBeenCalled();
  });

  it("reports whether it is hosted", () => {
    const { runtime } = build();
    expect(runtime.isHosted()).toBe(false);
    runtime.setHosted(true);
    expect(runtime.isHosted()).toBe(true);
  });
});
