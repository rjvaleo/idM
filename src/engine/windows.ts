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
 * Open, and drawn on this canvas, are not the same question.
 *
 * In the plugin an auxiliary window becomes a real OS window: the panel is
 * fixed at 1000 x 460 and the editors do not fit in it. That window is drawn by
 * the detached document, not by the canvas that asked for it.
 *
 * It is still *open* — the Windows menu has to show it as taken, and the
 * session has to restore it — so the two questions need two answers. Answering
 * both with "is it in the open set" is what put an OS window and an in-app
 * window on screen at once, one of which had to be closed by hand before the
 * other could be used.
 */
export function popsOutOfCanvas(
  id: AppWindowId,
  { hosted, detached }: { hosted: boolean; detached: boolean },
): boolean {
  if (!hosted || detached) return false;
  return APP_WINDOWS.some((window) => window.id === id && !window.permanent);
}

/** Whether this canvas should draw the window itself. */
export function drawnOnCanvas(
  id: AppWindowId,
  open: ReadonlySet<string>,
  where: { hosted: boolean; detached: boolean },
): boolean {
  return open.has(id) && !popsOutOfCanvas(id, where);
}
