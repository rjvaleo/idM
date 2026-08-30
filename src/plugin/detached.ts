// A single window, on its own.
//
// In the plugin, auxiliary windows open as real OS windows rather than as
// overlays inside a fixed 1000 x 460 panel. Each one is another webview loading
// this same bundle, told by the URL fragment which window it is.
//
// Nothing is redrawn for this. The window that appears is the same component,
// the same markup and the same CSS as the one in the canvas; the rest of the
// interface is simply not shown.

/** The window this document is showing on its own, or null for the whole app. */
export function detachedWindowId(): string | null {
  if (typeof location === "undefined") return null;

  const match = /(?:^|[#&])detached=([^&]+)/.exec(location.hash);
  return match ? decodeURIComponent(match[1]) : null;
}

export function isDetached(): boolean {
  return detachedWindowId() !== null;
}
