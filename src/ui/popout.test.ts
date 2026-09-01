import { describe, expect, it } from "vitest";
import { APP_WINDOWS, drawnOnCanvas } from "../engine/windows";

/*
 * Auxiliary windows draw on the canvas, and nowhere else.
 *
 * They used to also open as real OS windows in the plugin. That gave two
 * windows for one command: an OS window in front, the in-app one behind it, and
 * the OS window had to be closed by hand before the in-app one could be used.
 * The in-app window is the one that works, so it is the only one.
 */
describe("an auxiliary window opens in the app, not as an OS window", () => {
  it("is drawn on the canvas when open", () => {
    expect(drawnOnCanvas("cyclic-editor", new Set(["cyclic-editor"]))).toBe(true);
  });

  it("is not drawn when it is not open", () => {
    expect(drawnOnCanvas("cyclic-editor", new Set())).toBe(false);
  });

  it("draws every window on the canvas, permanent or not", () => {
    for (const w of APP_WINDOWS) {
      expect(drawnOnCanvas(w.id, new Set([w.id])), w.id).toBe(true);
    }
  });

  it("does not depend on whether it is hosted in a plugin", () => {
    // The rule takes no host argument at all. A signature that cannot express
    // "pop this one out" cannot grow the behaviour back by accident.
    expect(drawnOnCanvas.length).toBe(2);
  });
});
