import { describe, expect, it } from "vitest";
import { APP_WINDOWS, drawnOnCanvas, popsOutOfCanvas } from "../engine/windows";

/*
 * The plugin drew auxiliary windows twice.
 *
 * Opening the Cyclic Editor gave you a real OS window *and* an in-app window on
 * the canvas behind it. The OS one had to be closed by hand before the in-app
 * one could be used. It had been that way since pop-outs were added, because
 * "open" and "drawn here" were the same set.
 */
describe("an auxiliary window pops out instead of being drawn twice", () => {
  const inPlugin = { hosted: true, detached: false };
  const inBrowser = { hosted: false, detached: false };
  const inPopOut = { hosted: true, detached: true };
  const open = new Set<string>(["cyclic-editor"]);

  it("is not drawn on the canvas that asked for it", () => {
    expect(popsOutOfCanvas("cyclic-editor", inPlugin)).toBe(true);
    expect(drawnOnCanvas("cyclic-editor", open, inPlugin)).toBe(false);
  });

  it("is still open, so the menu and the session both know about it", () => {
    expect(open.has("cyclic-editor")).toBe(true);
  });

  it("is drawn by the detached document that renders it", () => {
    expect(popsOutOfCanvas("cyclic-editor", inPopOut)).toBe(false);
    expect(drawnOnCanvas("cyclic-editor", open, inPopOut)).toBe(true);
  });

  it("is drawn in the browser, which has a canvas that can grow", () => {
    expect(popsOutOfCanvas("cyclic-editor", inBrowser)).toBe(false);
    expect(drawnOnCanvas("cyclic-editor", open, inBrowser)).toBe(true);
  });

  it("never pops out a permanent window - those are the docked panel", () => {
    for (const w of APP_WINDOWS.filter((x) => x.permanent)) {
      expect(popsOutOfCanvas(w.id, inPlugin)).toBe(false);
      expect(drawnOnCanvas(w.id, new Set([w.id]), inPlugin)).toBe(true);
    }
  });

  it("pops out every auxiliary window, not just the ones anyone tried", () => {
    for (const w of APP_WINDOWS.filter((x) => !x.permanent)) {
      expect(popsOutOfCanvas(w.id, inPlugin), w.id).toBe(true);
      expect(drawnOnCanvas(w.id, new Set([w.id]), inPlugin), w.id).toBe(false);
    }
  });

  it("draws nothing that is not open", () => {
    expect(drawnOnCanvas("synth", new Set(), inBrowser)).toBe(false);
  });
});
