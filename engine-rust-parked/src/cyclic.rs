//! Ported from `src/engine/cyclic.ts`.

use crate::num::js_round;
use crate::rng::Rng;

/// A cyclic step is either a single level or an inclusive range of them.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum CyclicStep {
    Level(f64),
    Range { min: f64, max: f64 },
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct CyclicLevelRange {
    pub min: i32,
    pub max: i32,
}

fn clamp_level(value: f64) -> i32 {
    js_round(value).clamp(0.0, 4.0) as i32
}

/// A zero-based marker column as M's inclusive 1..16 loop length.
pub fn cyclic_length_from_step_index(step_index: f64) -> i32 {
    (js_round(step_index) + 1.0).clamp(1.0, 16.0) as i32
}

/// Accept a bare level as well as a range, and canonicalise to an ordered range.
pub fn normalize_cyclic_step(step: CyclicStep) -> CyclicLevelRange {
    match step {
        CyclicStep::Level(v) => {
            let level = clamp_level(v);
            CyclicLevelRange { min: level, max: level }
        }
        CyclicStep::Range { min, max } => {
            let a = clamp_level(min);
            let b = clamp_level(max);
            CyclicLevelRange { min: a.min(b), max: a.max(b) }
        }
    }
}

/// Pick uniformly and inclusively.
///
/// A point range consumes no randomness. That is not an optimisation: it is
/// what keeps projects seeded before ranges existed sounding as they did.
pub fn pick_cyclic_level(step: CyclicStep, rng: &mut Rng) -> i32 {
    let CyclicLevelRange { min, max } = normalize_cyclic_step(step);

    if min == max {
        min
    } else {
        min + rng.int((max - min + 1) as u32) as i32
    }
}
