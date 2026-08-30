//! Ported from `src/engine/transform.ts` — the ordered chain M applies per step.

use crate::music::clamp_midi;
use crate::num::js_round;
use crate::rng::{brownian_step, Rng};

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NoteOrder {
    Original,
    Reverse,
    Random,
    RandomWalk,
    Brownian,
}

impl NoteOrder {
    pub fn from_name(name: &str) -> Option<Self> {
        Some(match name {
            "original" => NoteOrder::Original,
            "reverse" => NoteOrder::Reverse,
            "random" => NoteOrder::Random,
            "random-walk" => NoteOrder::RandomWalk,
            "brownian" => NoteOrder::Brownian,
            _ => return None,
        })
    }
}

/// Which of the three Note Order lists a step was drawn from.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum NoteSource {
    Original,
    Cyclic,
    Utterly,
}

impl NoteSource {
    pub fn name(self) -> &'static str {
        match self {
            NoteSource::Original => "original",
            NoteSource::Cyclic => "cyclic",
            NoteSource::Utterly => "utterly",
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct NoteOrderCursor {
    /// Logical position counter.
    pub pos: i64,
    /// Last index read, for no-repeat and the walk.
    pub last: i32,
    /// 0..1 Brownian walk position.
    pub bval: f64,
}

impl Default for NoteOrderCursor {
    fn default() -> Self {
        Self { pos: 0, last: -1, bval: 0.5 }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct NoteOrderMix {
    pub original: i32,
    pub cyclic: i32,
    pub utterly: i32,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct VelocityRange {
    pub low: f64,
    pub high: f64,
}

/// Seconds per step for a Voice's time base. A whole note is `4 * (60 / tempo)`
/// seconds; a step is `numerator/denominator` of a whole note.
pub fn step_duration_seconds(tempo: f64, numerator: f64, denominator: f64) -> f64 {
    let whole_note = 4.0 * (60.0 / tempo);
    whole_note * (numerator / denominator)
}

/// Density gate: whether this step actually sounds.
pub fn gate(density: f64, rng: &mut Rng) -> bool {
    rng.chance(density)
}

/// Clamp and order the two MIDI endpoints of the Velocity Range.
pub fn normalize_velocity_range(range: VelocityRange) -> (i32, i32) {
    let a = clamp_midi(range.low);
    let b = clamp_midi(range.high);
    (a.min(b), a.max(b))
}

/// Accent 0 is a rest; levels 1-4 divide the selected range evenly.
pub fn velocity_for_accent(range: VelocityRange, raw_level: f64) -> i32 {
    let level = js_round(raw_level).clamp(0.0, 4.0) as i32;
    if level == 0 {
        return 0;
    }

    let (low, high) = normalize_velocity_range(range);
    js_round(f64::from(low) + f64::from(high - low) * f64::from(level - 1) / 3.0) as i32
}

/// The step index a Voice should read next, plus the advanced cursor.
pub fn next_step_index(
    order: NoteOrder,
    cursor: NoteOrderCursor,
    length: i32,
    rng: &mut Rng,
) -> (i32, NoteOrderCursor) {
    let mut bval = cursor.bval;

    let index = match order {
        NoteOrder::Original => (cursor.pos % i64::from(length)) as i32,
        NoteOrder::Reverse => length - 1 - (cursor.pos % i64::from(length)) as i32,
        NoteOrder::Random => rng.pick_index_avoiding(length as u32, cursor.last as u32) as i32,
        NoteOrder::Brownian => {
            // 1/f-ish smooth wander: the walk position maps onto the step range,
            // so the read head drifts rather than jumping.
            bval = brownian_step(cursor.bval, 0.18, rng);
            (length - 1).min((bval * f64::from(length)).floor() as i32)
        }
        NoteOrder::RandomWalk => {
            let base = if cursor.last < 0 { 0 } else { cursor.last };
            let dir = if rng.chance(0.5) { 1 } else { -1 };
            (base + dir).rem_euclid(length)
        }
    };

    (index, NoteOrderCursor { pos: cursor.pos + 1, last: index, bval })
}

/// The stored, repeating permutation behind Cyclic Random mode.
pub fn make_cyclic_order(length: i32, seed: u32) -> Vec<i32> {
    let mut order: Vec<i32> = (0..length).collect();
    let mut rng = Rng::new(seed);

    let mut i = order.len();
    while i > 1 {
        i -= 1;
        let j = rng.int((i + 1) as u32) as usize;
        order.swap(i, j);
    }

    order
}

/// Convert the two movable edge controls into M's three percentages.
pub fn note_order_mix_from_edges(original_value: f64, utterly_value: f64) -> NoteOrderMix {
    let original = js_round(original_value).clamp(0.0, 100.0) as i32;
    let utterly = js_round(utterly_value).clamp(0.0, f64::from(100 - original)) as i32;

    NoteOrderMix { original, cyclic: 100 - original - utterly, utterly }
}

/// Pick among the three note-order sources by percentage. Original and Cyclic
/// advance repeatably; Utterly chooses afresh on every step.
pub fn next_mixed_step_index(
    mix: NoteOrderMix,
    cursor: NoteOrderCursor,
    length: i32,
    rng: &mut Rng,
) -> (i32, NoteSource, NoteOrderCursor) {
    let roll = rng.next() * 100.0;

    let (index, source) = if roll < f64::from(mix.original) {
        ((cursor.pos % i64::from(length)) as i32, NoteSource::Original)
    } else if roll < f64::from(mix.original + mix.cyclic) {
        ((cursor.pos % i64::from(length)) as i32, NoteSource::Cyclic)
    } else {
        (
            rng.pick_index_avoiding(length as u32, cursor.last as u32) as i32,
            NoteSource::Utterly,
        )
    };

    (index, source, NoteOrderCursor { pos: cursor.pos + 1, last: index, bval: cursor.bval })
}
