# The plugin window

How M Classic's interface becomes a plugin. Branch: `plugin-host`.

## The rule

**This is a port, not a redesign.** The windows we have, exactly as they are —
same components, same pixel-exact layouts, same CSS. Nothing is redrawn,
nothing is reinvented. What changes is the container they run in.

## The docked panel

Measured from the running app at default positions, with the default window set
open:

| | x | y | w | h |
|---|---:|---:|---:|---:|
| menu bar | 0 | 0 | — | 22 |
| Patterns | 4 | 27 | 228 | 120 |
| Transport | 232 | 27 | 229 | 109 |
| Snapshot | 468 | 27 | 62 | 315 |
| Midi View | 534 | 27 | 454 | 251 |
| Variables | 4 | 143 | 220 | 156 |
| Cyclic Variables | 232 | 143 | 229 | 156 |
| Pattern Editor | 534 | 282 | 336 | 167 |
| Midi | 4 | 303 | 454 | 45 |

Total **988 × 449**. The panel is **1000 × 460**.

The important part: **they already dock.** Those x values — 4, 232, 468, 534 —
and y values — 27, 143, 282, 303 — tile into a grid with 4 px gaps and no
overlap. Default placement is already the docked layout. Docking is not an
arrangement job; it is switching dragging off.

The menu bar is already part of the app: File, Edit, Variables, Pattern,
Windows, Options, View, rendered in HTML at 22 px. A plugin gets no OS menu
bar, and it does not need one — this bar comes along with the port.

## Pop-out windows

Ten windows are not in the default set and have no room in a full grid:

Cyclic Editor · Midi Assignment · Synth · Note Density · Velocity Range ·
Note Order · Transposition · Time Distortion · Orchestration

**These open as real OS windows.** Not overlays inside the panel, not a growing
panel. Windows → Cyclic Editor produces a window you can move, resize and place
on a second monitor.

### What that costs, and has to be handled

A plugin runs in the host's process and can create native windows — JUCE does
this with `DocumentWindow`, and plenty of plugins ship browsers and editors that
way. Four things must be built rather than assumed:

1. **Keyboard focus.** Ableton claims keys for its own shortcuts. A detached
   window that wants typing — the Pattern Editor especially — has to take focus
   deliberately and give it back.
2. **Lifetime.** When the host closes the plugin editor, every pop-out must
   close with it. A leaked window outlives its engine and is unreachable.
3. **One webview each.** With a webview UI, every pop-out is another webview
   instance and another bridge to keep in sync. Ten open at once is a real cost;
   they should be created on open and destroyed on close, not pooled.
4. **AUv3 is out.** Its sandbox does not permit this. Not a current target —
   AU, VST3, CLAP and standalone are — but it forecloses iOS later.

### What the React app needs

Almost nothing. Each pop-out renders one existing component into its own
webview root. The components are already separate and already standalone; what
is added is a way to mount one on its own instead of inside the canvas, and the
state bridge already required for the docked panel.

## Decided

**The panel is fixed at 1000 × 460.** Not resizable. This matches the original,
which had a fixed layout, and it removes reflow from the port entirely — there
is one size, and the windows are already laid out for it.

Two consequences.

**Zoom needs a decision it did not need in a browser.** The View menu offers
Zoom Out / Zoom In / Actual Size against a workspace scale. In a browser the
page grows and the viewport scrolls; in a fixed 1000 × 460 panel, zooming in
overflows with nowhere to go. Either zoom is removed from the plugin's View
menu, or the panel scrolls, or zoom scales the whole grid down only. Removing it
is the most honest for a fixed panel and the least code.

**Pop-out state saves with the host session.** Which windows are open, and where
each one sits, restore when the session reopens.

That means moving where it lives. Today `useDraggable` persists positions to
`localStorage`, keyed per window. In a plugin that is wrong twice over: it is
per-webview-origin rather than per-session, so two instances of the plugin in
one set would fight over the same keys, and it does not travel with the project.
Pop-out state belongs in the plugin's own state blob — the one the host asks for
and hands back — alongside the project.

Worth keeping separate inside that blob, though: the project is the musical
document and already has its own `.mclone` format, while open-windows-and-where
is interface state. Restoring a session should not be able to corrupt a project
because a window moved.
