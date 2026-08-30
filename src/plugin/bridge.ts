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

/** Send the whole project. Small enough to send whole, and sending whole means
 *  the engine can never hold a half-applied edit. */
function sendProject(): void {
  callNative("setProject", JSON.stringify(useM.getState().project));
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
  if (!isPlugin()) return;

  let queued = false;
  let last = useM.getState().project;

  const flush = () => {
    queued = false;
    sendProject();
  };

  useM.subscribe((state) => {
    if (state.project === last) return;
    last = state.project;

    if (queued) return;
    queued = true;
    queueMicrotask(flush);
  });

  // The processor starts on its own default project; this replaces it with
  // whatever the interface actually has, including a restored session.
  sendProject();
}
