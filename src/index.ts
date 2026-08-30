/**
 * The public surface of idM.
 *
 * idM is two layers, and this file is the seam between them. `src/engine`
 * is the generative core — 5,700 lines with no React, no Zustand and no browser
 * API in it, which is what makes a VST3/AU port possible at all. `src/ui` is one
 * presentation of that core, for the browser.
 *
 * idMLab consumes this package to run idM as a module rather than
 * reimplementing it, so what is exported here is a contract: anything idMLab
 * reaches for must appear below, and removing something from this list is a
 * breaking change even when the file it came from is untouched.
 */

// The whole browser application, as one component. idMLab's ClassicView mounts
// this — the same code that passes this package's own test suite.
export { App } from "./ui/App";

// Note Order geometry. idMLab draws its own Note Order control and needs the
// same handle maths, so the two cannot drift apart.
export { noteOrderHandleLayout, setNoteOrderBoundary } from "./engine/transform";

// The generative core, unwrapped. This is what the native plugin will bind to.
export * from "./engine/types";
