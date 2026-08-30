//! Numeric helpers whose JavaScript semantics differ from Rust's defaults.

/// JavaScript's `Math.round`, which rounds a half **toward +Infinity**.
///
/// Rust's `f64::round` rounds a half *away from zero*, so the two disagree on
/// every negative half: `Math.round(-0.5)` is `-0`, `(-0.5_f64).round()` is
/// `-1.0`. The engine rounds velocities, levels, MIDI notes and tick positions,
/// and reaching for the obvious method would move notes.
pub fn js_round(v: f64) -> f64 {
    (v + 0.5).floor()
}
