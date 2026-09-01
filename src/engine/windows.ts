export const APP_MENU_LABELS = ["File", "Edit", "Variables", "Pattern", "Windows", "Options"] as const;

export const APP_WINDOWS = [
  { id: "patterns", label: "Patterns", permanent: true },
  { id: "conducting", label: "Conducting", permanent: true },
  { id: "variables", label: "Variables", permanent: true },
  { id: "cyclic-variables", label: "Cyclic Variables", permanent: true },
  { id: "midi", label: "Midi", permanent: true },
  { id: "snapshot", label: "Snapshot", permanent: true },
  { id: "pattern-editor", label: "Pattern Editor", permanent: false },
  { id: "cyclic-editor", label: "Cyclic Editor", permanent: false },
  { id: "midi-view", label: "Midi View", permanent: false },
  { id: "midi-assignment", label: "Midi Assignment", permanent: false },
  { id: "synth", label: "Synth", permanent: false },
  { id: "density", label: "Note Density Editor", permanent: false },
  { id: "velocityRange", label: "Velocity Range Editor", permanent: false },
  { id: "noteOrderMix", label: "Note Order Editor", permanent: false },
  { id: "transposition", label: "Transposition Editor", permanent: false },
  { id: "timeDistort", label: "Time Distortion Editor", permanent: false },
  { id: "outputChannels", label: "Orchestration Editor", permanent: false },
] as const;

export type AppWindowId = (typeof APP_WINDOWS)[number]["id"];

export function openAppWindow(current: ReadonlySet<string>, id: AppWindowId): Set<string> {
  return new Set([...current, id]);
}

export function closeAppWindow(current: ReadonlySet<string>, id: AppWindowId): Set<string> {
  if (APP_WINDOWS.find((window) => window.id === id)?.permanent) return new Set(current);
  const next = new Set(current);
  next.delete(id);
  return next;
}

/*
 * Auxiliary windows are drawn on the canvas, and only there.
 *
 * They used to also open as real OS windows in the plugin, on the reasoning
 * that the panel is fixed at 1000 x 460 and the editors do not fit in it. What
 * that produced was two windows for one command - an OS window in front and the
 * in-app one behind it - and the OS window had to be closed by hand before the
 * in-app one could be used. The in-app window is the one that works, so it is
 * the only one now.
 */
export function drawnOnCanvas(id: AppWindowId, open: ReadonlySet<string>): boolean {
  return open.has(id);
}
