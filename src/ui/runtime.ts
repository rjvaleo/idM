// Single shared runtime instance, bound to the live store so the engine always
// reads current project state (that's what makes live tweaking work).

import { MRuntime } from "../engine/runtime";
import { engineStatus, isPlugin, setHostedTransport } from "../plugin/bridge";
import { useM } from "../state/store";
import { decodeMidiMessage, isChannelMessage, mapAssignedInputChannel } from "../engine/midiinput";

let runtime: MRuntime | null = null;

/**
 * Inside the plugin the transport belongs to the host.
 *
 * The engine that makes the music lives in the processor and follows Ableton's
 * clock. If this one also ran, there would be two engines disagreeing about the
 * time - and this one's output has nowhere to go, since a plugin webview has no
 * Web MIDI and no audio device of its own. So in plugin mode the local runtime
 * is never started, and the interface's Start button defers to the host.
 */
export function transportIsHosted(): boolean {
  return isPlugin();
}

/**
 * Tell the engine the transport moved.
 *
 * In the plugin this goes nowhere — the host decides. In the standalone it is
 * what makes the Start button do anything at all.
 */
/**
 * How long the transport has been running, whoever owns it.
 *
 * The interface's displays scroll on elapsed time. In the plugin the local
 * runtime is deliberately never started — there is exactly one engine, and it
 * is in the processor — so asking it would return zero forever and the readouts
 * would sit still while the music played.
 */
export function transportElapsedSec(): number {
  if (isPlugin()) return engineStatus()?.elapsed ?? 0;
  return getRuntime().transportElapsedSec();
}

export function hostedTransport(running: boolean): void {
  if (isPlugin()) setHostedTransport(running);
}

export function getRuntime(): MRuntime {
  if (!runtime) {
    runtime = new MRuntime(
      () => useM.getState().project,
      (notes) => useM.getState().recordMidiNotes(notes),
      {
        onCyclicReset: (voices) => useM.getState().signalCyclicReset(voices),
        onPlannedSteps: (steps) => useM.getState().followDrumMachine(steps),
        onTransportSent: (type) => {
          useM.getState().recordMidiTransport(type, "out", performance.now() / 1000);
        },
        onMidiMessage: (event) => {
          if (!event.data) return;
          const message = decodeMidiMessage(event.data);
          if (!message) return;
          // Clock and transport carry no channel, so they are taken by the
          // runtime's follower and never reach the channel routing below.
          if (!isChannelMessage(message)) {
            const action = getRuntime()
              .ingestClockMessage(message, performance.now() / 1000);
            // The same path a Start from the Input Control System takes, so an
            // external transport and a local one cannot drift out of step.
            if (action) {
              useM.getState()
                .recordMidiTransport(action, "in", performance.now() / 1000);
            }
            if (action === "start" || action === "continue") {
              void runtime?.start().then(() => useM.getState().setPlaying(true));
            } else if (action === "stop") {
              runtime?.stop();
              useM.getState().setPlaying(false);
            }
            return;
          }
          const deviceId = (event.currentTarget as MIDIInput | null)?.id ?? null;
          const mappedChannel = mapAssignedInputChannel(
            useM.getState().project.midiAssignments.inputs, deviceId, message.channel,
          );
          if (mappedChannel === null) return;
          const mapped = { ...message, channel: mappedChannel };
          const responses = useM.getState().receiveMidi(mapped);
          for (const response of responses) {
            if (response.type === "start") {
              void runtime?.start().then(() => useM.getState().setPlaying(true));
              continue;
            }
            if (response.type === "stop") {
              runtime?.stop();
              useM.getState().setPlaying(false);
              continue;
            }
            if (response.type === "sync") {
              runtime?.sync();
              continue;
            }
            if ("voice" in response) {
              const voice = useM.getState().project.voices[response.voice];
              runtime?.audition([response.note], response.velocity,
                "channels" in response && response.channels
                  ? response.channels : voice.outputChannels, 0.25, response.voice);
            }
          }
        },
        getPerformanceSettings: () => {
          const state = useM.getState();
          return {
            useMetronome: state.options.useMetronome,
            sendClock: state.options.sendClock,
            syncRatio: state.syncRatio,
            externalClock: state.options.externalClock,
            syncRatioDirection: state.syncRatioDirection,
          };
        },
      },
    );
  }
  // Set every time, not only on creation: the guard has to hold no matter
  // which call site reached the runtime first.
  runtime.setHosted(isPlugin());
  runtime.setSynthSettings(useM.getState().synthSettings);
  return runtime;
}
