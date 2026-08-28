// The Options menu, from manual chapter 22.
//
// "The commands in the Options Menu are settings that can be turned on and off
// by choosing them. An Option is on when the menu item shows a check mark and
// off when there's no check mark."
//
// Order below is the order chapter 22 prints them.

export const OPTION_IDS = [
  "useMetronome",
  "sendClock",
  "externalClock",
  "tapAffectsVelocity",
  "dontScrambleRests",
  "slideshowRecordWait",
  "noZoomRects",
  "sustainEntersRests",
  "midiConduct",
  "secondOrderTranspose",
  "noCyclicBlinking",
  "syncRestartsSequence",
  "editorSoundWhilePlaying",
  "lockedMarkedVariables",
  "echoInBackground",
] as const;

export type OptionId = (typeof OPTION_IDS)[number];
export type Options = Record<OptionId, boolean>;

export const OPTION_LABELS: Record<OptionId, string> = {
  useMetronome: "Use Metronome",
  sendClock: "Send Clock",
  externalClock: "External Clock",
  tapAffectsVelocity: "Tap Affects Velocity",
  dontScrambleRests: "Don't Scramble Rests",
  slideshowRecordWait: "Slideshow Record Wait",
  noZoomRects: "No Zoom Rects",
  sustainEntersRests: "Sustain Enters Rests",
  midiConduct: "Midi Conduct",
  secondOrderTranspose: "Second Order Transpose",
  noCyclicBlinking: "No Cyclic Blinking",
  syncRestartsSequence: "Sync Restarts Sequence",
  editorSoundWhilePlaying: "Editor Sound While Playing",
  lockedMarkedVariables: "Locked Marked Variables",
  echoInBackground: "Echo In Background",
};

/**
 * Options with nothing to act on, and why.
 *
 * Each of these is `not-applicable` in the manual conformance audit — not
 * unimplemented, but without a target in a browser build. They stay in the menu
 * because chapter 22 prints them and this is a recreation, and they are shown
 * disabled with the reason attached, because a checkbox that toggles and
 * changes nothing is worse than one that explains itself.
 *
 * The reasons are the audit's, verbatim in substance: `option.no-zoom`,
 * `option.sync-sequence` and `option.echo-background` in
 * `src/manual/manualConformance.ts`.
 */
const UNAVAILABLE_OPTIONS: ReadonlyMap<OptionId, string> = new Map<OptionId, string>([
  ["noZoomRects",
    "Browser windows open immediately and never draw the obsolete zoom rectangles."],
  ["syncRestartsSequence",
    "Imported Sequence playback is deliberately out of scope, so Sync has no sequence to restart."],
  ["echoInBackground",
    "Whether MIDI arrives while another application is in front is decided by the browser and OS, not by M."],
]);

export function isOptionAvailable(id: OptionId): boolean {
  return !UNAVAILABLE_OPTIONS.has(id);
}

/** Why an option is disabled, or undefined when it is not. */
export function optionUnavailableReason(id: OptionId): string | undefined {
  return UNAVAILABLE_OPTIONS.get(id);
}

/**
 * "Below, all Options are shown unchecked, or Off." — with the two exceptions
 * chapter 22 calls out by name.
 */
export const DEFAULT_OPTIONS: Options = Object.freeze(
  OPTION_IDS.reduce((acc, id) => {
    // "Almost everyone will want this option checked, as is the default state."
    acc[id] = id === "slideshowRecordWait";
    return acc;
  }, {} as Options),
);

export function setOption(options: Options, id: OptionId, on: boolean): Options {
  return { ...options, [id]: on };
}

export type OptionEntry = {
  id: OptionId;
  label: string;
  checked: boolean;
  available: boolean;
  /** Present only when `available` is false. */
  unavailableReason?: string;
};

/** The menu's own view of itself, in printed order. */
export function optionEntries(options: Options): OptionEntry[] {
  return OPTION_IDS.map((id) => ({
    id,
    label: OPTION_LABELS[id],
    checked: options[id],
    unavailableReason: optionUnavailableReason(id),
    available: isOptionAvailable(id),
  }));
}
