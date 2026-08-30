// The plugin bridge.
//
// In a browser this file does nothing: `window.__JUCE__` is absent and
// `installPluginBridge` returns immediately. Inside the plugin it is what makes
// the interface mean anything — the engine lives in the processor, and the
// windows are how you tell it what to play.
//
// Direction matters. The processor owns the music; the UI sends it state and
// never receives notes back. That is what keeps the plugin playing when its
// window is closed.

import { useM } from "../state/store";
import { decodeMidiMessage, isChannelMessage } from "../engine/midiinput";

/** JUCE's low-level frontend API, as it appears on the page. */
type JuceBackend = {
  emitEvent: (eventId: string, payload: unknown) => void;
  addEventListener: (eventId: string, fn: (payload: unknown) => void) => unknown;
};

type JuceGlobal = { backend?: JuceBackend };

function backend(): JuceBackend | null {
  const juce = (globalThis as { __JUCE__?: JuceGlobal }).__JUCE__;
  return juce?.backend ?? null;
}

/** True when this build is running inside the plugin rather than a browser. */
export function isPlugin(): boolean {
  return backend() !== null;
}

/**
 * Call a function the C++ side registered.
 *
 * The result id is required by the protocol and the reply is ignored: these are
 * one-way notifications, and waiting for an acknowledgement would tie the
 * interface's responsiveness to the audio thread's.
 */
let nextCallId = 0;

function callNative(name: string, ...params: unknown[]): void {
  const juce = backend();
  if (!juce) return;

  juce.emitEvent("__juce__invoke", { name, params, resultId: nextCallId++ });
}

/**
 * Send the whole document.
 *
 * The engine only needs the project, but the Variable Positions, Snapshots and
 * Slideshows have to survive a session too, and the host stores exactly one
 * blob. So the document is what crosses, the processor keeps it verbatim for
 * the host, and reaches inside it for the project.
 *
 * Whole rather than a diff, so the engine can never hold a half-applied edit.
 */
function sendDocument(): void {
  callNative("setProject", JSON.stringify(useM.getState().exportDocument()));
}

/**
 * Wire the interface to the processor.
 *
 * Every store change sends the project. Zustand notifies synchronously on each
 * `set`, so a drag would send once per frame's worth of updates; the send is
 * coalesced onto a microtask so a gesture produces one message rather than
 * hundreds.
 */
export function installPluginBridge(): void {
  const juce = backend();
  if (!juce) return;

  let queued = false;
  let last = useM.getState().project;

  const flush = () => {
    queued = false;
    sendDocument();
  };

  useM.subscribe((state) => {
    if (state.project === last) return;
    last = state.project;

    if (queued) return;
    queued = true;
    queueMicrotask(flush);
  });

  // A session the host restored reached the engine before this window existed.
  // Applying it here is what makes the windows show the project that is
  // actually playing rather than a default that is not.
  juce.addEventListener("mclassic-document", (payload) => {
    try {
      const raw = typeof payload === "string" ? JSON.parse(payload) : payload;
      const result = useM.getState().importDocument(raw);

      if (!result.ok) return;

      // Restoring is not an edit. Send it straight back so the engine and the
      // interface agree on which document is live.
      last = useM.getState().project;
    } catch {
      // A malformed blob is not worth taking the interface down for; the
      // engine keeps whatever it already had.
    }
  });

  // The host's MIDI. M's Input Control System lives here — note input, Echo
  // Map, Keyboard Transpose, Step Advance — and all of it edits the project,
  // which then reaches the engine the ordinary way. Arrives as a flat array of
  // status/data1/data2 triples, because a chord or a controller sweep crosses
  // as one burst rather than one message at a time.
  juce.addEventListener("mclassic-midi-in", (payload) => {
    if (!Array.isArray(payload)) return;

    const receive = useM.getState().receiveMidi;

    for (let i = 0; i + 2 < payload.length; i += 3) {
      const message = decodeMidiMessage(
        Uint8Array.from([Number(payload[i]), Number(payload[i + 1]), Number(payload[i + 2])]),
      );

      if (message && isChannelMessage(message)) receive(message);
    }
  });

  // The processor starts on its own default project; this replaces it with
  // whatever the interface actually has.
  sendDocument();
}
