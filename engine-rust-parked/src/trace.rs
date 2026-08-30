//! Ported from `src/engine/goldenTrace.ts`.
//!
//! A trace is the full sequence of planned notes for a fixed project, seed and
//! span, flattened to primitives. Two engines agree when their traces are
//! identical strings — which is what turns "the same music" into something a
//! build can check.

use crate::planner::{make_cursors, plan_window};
use crate::project::ProjectState;
use crate::rng::Rng;

/// Golden ratio in 32 bits, the constant `traceProject` decorrelates the
/// per-Voice seeds with.
const SEED_STRIDE: u32 = 0x9e37_79b1;

/// Format the way `Array.prototype.join` would.
///
/// Every value written here is a `Math.round` result, so it is integral, and
/// JavaScript prints an integral double without a fractional part. Rust's
/// `Display` agrees for these, but going through `i64` also normalises `-0`,
/// which JavaScript renders as `0`.
fn js_number(value: f64) -> String {
    if value.is_finite() && value.fract() == 0.0 && value.abs() < 9.0e18 {
        format!("{}", value as i64)
    } else {
        format!("{value}")
    }
}

/// The per-Voice generators, seeded exactly as `traceProject` seeds them.
pub fn trace_rngs(voices: usize, seed: u32) -> Vec<Rng> {
    (0..voices)
        .map(|voice| Rng::new(seed ^ (voice as u32 + 1).wrapping_mul(SEED_STRIDE)))
        .collect()
}

/// One line per note: tick, voice, channel, pitch, velocity, duration in ticks.
///
/// Seconds are deliberately excluded. They come from the tempo map by
/// floating-point multiply, and two languages need not agree on the last bit of
/// a double — ticks are integers and carry the same musical fact.
pub fn trace_project(project: &ProjectState, seed: u32, span_sec: f64, windows: usize) -> String {
    let mut rngs = trace_rngs(project.voices.len(), seed);
    let mut cursors = make_cursors(project, 0.0);
    let mut rows: Vec<(f64, usize, i32, i32, i32, String)> = Vec::new();

    let step = span_sec / windows as f64;

    for w in 0..windows {
        let end = step * (w + 1) as f64;
        let (notes, next, _steps) = plan_window(project, &cursors, &mut rngs, step * w as f64, end);
        cursors = next;

        for note in notes {
            rows.push((
                note.at_tick,
                note.voice,
                note.channel,
                note.note,
                note.velocity,
                format!(
                    "{},{},{},{},{},{}",
                    js_number(note.at_tick),
                    note.voice,
                    note.channel,
                    note.note,
                    note.velocity,
                    js_number(note.duration_ticks)
                ),
            ));
        }
    }

    // Sorted, not emission-ordered. Planning the same span in one window or in
    // sixteen produces the same notes in a different sequence, because each
    // pass walks the Voices and more windows interleave the lanes more finely.
    // That is loop structure; a trace is supposed to be the music. The sort is
    // stable, matching `Array.prototype.sort`, so notes agreeing on all five
    // keys keep the order they were planned in.
    rows.sort_by(|a, b| {
        a.0.partial_cmp(&b.0)
            .expect("tick must not be NaN")
            .then(a.1.cmp(&b.1))
            .then(a.2.cmp(&b.2))
            .then(a.3.cmp(&b.3))
            .then(a.4.cmp(&b.4))
    });

    rows.into_iter().map(|r| r.5).collect::<Vec<_>>().join("\n")
}

/// A float as its exact bit pattern.
fn f64_hex(value: f64) -> String {
    format!("{:016x}", value.to_bits())
}

/// Everything `trace_project` leaves out: the seconds, the Rhythm multiplier
/// and the Note Order source, in emission order.
///
/// Traces drop seconds on purpose, and the cost is that nothing pins them —
/// removing the Cyclic Legato from `duration_sec` leaves every trace identical.
/// Bit patterns make an exact comparison well defined, so they are pinned here.
pub fn trace_detail(project: &ProjectState, seed: u32, span_sec: f64, windows: usize) -> String {
    let mut rngs = trace_rngs(project.voices.len(), seed);
    let mut cursors = make_cursors(project, 0.0);
    let mut rows: Vec<String> = Vec::new();

    let step = span_sec / windows as f64;

    for w in 0..windows {
        let (notes, next, _steps) =
            plan_window(project, &cursors, &mut rngs, step * w as f64, step * (w + 1) as f64);
        cursors = next;

        for note in notes {
            rows.push(format!(
                "{},{},{},{},{},{},{},{},{},{}",
                js_number(note.at_tick),
                note.voice,
                note.channel,
                note.note,
                note.velocity,
                js_number(note.duration_ticks),
                note.source.name(),
                f64_hex(note.start_sec),
                f64_hex(note.duration_sec),
                f64_hex(note.rhythm)
            ));
        }
    }

    // Emission order, deliberately: sorting would hide a reordering.
    rows.join("\n")
}
