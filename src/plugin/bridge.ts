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
 * Call into the engine and wait for the answer.
 *
 * The reply arrives on `__juce__complete` keyed by the id we sent. This is the
 * only reliable direction: JUCE's push API is
 * `emitEventIfBrowserIsVisible`, which drops the event when a host does not
 * consider the plugin's webview visible, silently.
 */
function callNativeForResult(name: string, ...params: unknown[]): Promise<unknown> {
  const juce = backend();
  if (!juce) return Promise.resolve(null);

  const resultId = nextCallId++;

  return new Promise((resolve) => {
    const settle = (payload: unknown) => {
      const reply = payload as { promiseId?: number; result?: unknown } | null;
      if (!reply || reply.promiseId !== resultId) return;
      resolve(reply.result ?? null);
    };

    juce.addEventListener("__juce__complete", settle);
    juce.emitEvent("__juce__invoke", { name, params, resultId });

    // A host that never answers must not leave the poll wedged.
    setTimeout(() => resolve(null), 1000);
  });
}

/** What the engine is doing, for the interface to display. */
export type EngineStatus = {
  playing: boolean;
  tempo: number;
  notesSent: number;
  port: string;
  standalone: boolean;
};

let latestStatus: EngineStatus | null = null;

export function engineStatus(): EngineStatus | null {
  return latestStatus;
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
/**
 * Start or stop the transport.
 *
 * Ignored by the plugin build, where the host owns the transport. The
 * standalone has no host, so its Start button has to drive something — without
 * this it would render an interface that never plays.
 */
export function setHostedTransport(running: boolean): void {
  callNative("setTransport", running);
}

/**
 * Open one auxiliary window as a real OS window.
 *
 * The panel is fixed at 1000 x 460 and the auxiliary editors do not fit in it.
 * Outside a plugin they sit on a canvas that can grow; inside one there is no
 * canvas to grow, so they become windows you can move and put on a second
 * monitor.
 */
export function openPopOut(id: string, title: string): void {
  callNative("openWindow", id, title);
}

export function closePopOut(id: string): void {
  callNative("closeWindow", id);
}

/** Which auxiliary windows are open, so a session reopens with them. */
export function sendOpenWindows(ids: readonly string[]): void {
  callNative("setWindows", JSON.stringify(ids));
}

/** The windows a restored session had open. */
export function onWindowsRestored(handler: (ids: string[]) => void): void {
  const juce = backend();
  if (!juce) return;

  juce.addEventListener("mclassic-windows", (payload) => {
    try {
      const ids = typeof payload === "string" ? JSON.parse(payload) : payload;
      if (Array.isArray(ids)) handler(ids.map(String));
    } catch {
      // A malformed list costs a window position, not a session.
    }
  });
}

/** Told when a pop-out was closed by its own title bar rather than by us. */
export function onPopOutClosed(handler: (id: string) => void): void {
  const juce = backend();
  if (!juce) return;

  juce.addEventListener("mclassic-window-closed", (payload) => {
    if (typeof payload === "string") handler(payload);
  });
}

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

  // What the engine played. With the engine in the processor, this is the only
  // way the interface's Midi View can show anything: it is a monitor, and what
  // it monitors now happens on the other side of the bridge.
  juce.addEventListener("mclassic-played", (payload) => {
    if (!Array.isArray(payload)) return;

    const notes = [];

    for (let i = 0; i + 6 < payload.length; i += 7) {
      notes.push({
        voice: Number(payload[i]),
        note: Number(payload[i + 1]),
        velocity: Number(payload[i + 2]),
        channel: Number(payload[i + 3]),
        atTick: Number(payload[i + 4]),
        durationTicks: Number(payload[i + 5]),
        startSec: Number(payload[i + 6]),
        durationSec: 0,
      });
    }

    if (notes.length > 0) useM.getState().recordMidiNotes(notes);
  });

  // The host's transport, so M's own Start light follows Ableton rather than
  // sitting dark while it plays.
  juce.addEventListener("mclassic-transport", (payload) => {
    if (!Array.isArray(payload) || payload.length < 1) return;
    useM.getState().setPlaying(Boolean(payload[0]));
  });

  // Everything the engine has to say comes back through one poll. Pulled
  // rather than pushed, because JUCE's push API drops events whenever a host
  // does not consider the plugin's webview visible — which is exactly the
  // silent failure that made this look broken for days.
  const poll = async () => {
    const reply = (await callNativeForResult("poll")) as Record<string, unknown> | null;

    if (reply) {
      latestStatus = {
        playing: Boolean(reply.playing),
        tempo: Number(reply.tempo ?? 120),
        notesSent: Number(reply.notesSent ?? 0),
        port: String(reply.port ?? ""),
        standalone: Boolean(reply.standalone),
      };

      const played = reply.played;

      if (Array.isArray(played) && played.length >= 7) {
        const notes = [];

        for (let i = 0; i + 6 < played.length; i += 7) {
          notes.push({
            voice: Number(played[i]), note: Number(played[i + 1]),
            velocity: Number(played[i + 2]), channel: Number(played[i + 3]),
            atTick: Number(played[i + 4]), durationTicks: Number(played[i + 5]),
            startSec: Number(played[i + 6]), durationSec: 0,
          });
        }

        if (notes.length > 0) useM.getState().recordMidiNotes(notes);
      }

      const midiIn = reply.midiIn;

      if (Array.isArray(midiIn)) {
        const receive = useM.getState().receiveMidi;

        for (let i = 0; i + 2 < midiIn.length; i += 3) {
          const message = decodeMidiMessage(Uint8Array.from([
            Number(midiIn[i]), Number(midiIn[i + 1]), Number(midiIn[i + 2]),
          ]));

          if (message && isChannelMessage(message)) receive(message);
        }
      }

      useM.getState().setPlaying(Boolean(reply.playing));

      if (typeof reply.document === "string" && reply.document.length > 0) {
        try {
          useM.getState().importDocument(JSON.parse(reply.document));
        } catch {
          // A malformed blob costs a restore, not the session.
        }
      }
    }

    setTimeout(poll, 50);
  };

  void poll();

  // The processor starts on its own default project; this replaces it with
  // whatever the interface actually has.
  sendDocument();
}
