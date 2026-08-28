//! Ported from `src/engine/music.ts`.
//!
//! Scales, snapping, and diatonic transposition. All of the arithmetic here is
//! pitch-class modular, and JavaScript's `%` keeps the sign of the dividend —
//! so every wrap uses `rem_euclid` rather than `%`, and every `Math.floor` of a
//! quotient uses `div_euclid`. A negative note is where a careless port breaks.

use crate::num::js_round;

pub const NOTE_NAMES: [&str; 12] = [
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
];

#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
pub enum Scale {
    Chromatic,
    Major,
    Minor,
    Dorian,
    Mixolydian,
    Lydian,
    Phrygian,
    HarmonicMinor,
    MajorPentatonic,
    MinorPentatonic,
    Blues,
}

impl Scale {
    /// Scale degrees as semitone offsets from the root.
    pub fn degrees(self) -> &'static [i32] {
        match self {
            Scale::Chromatic => &[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11],
            Scale::Major => &[0, 2, 4, 5, 7, 9, 11],
            Scale::Minor => &[0, 2, 3, 5, 7, 8, 10],
            Scale::Dorian => &[0, 2, 3, 5, 7, 9, 10],
            Scale::Mixolydian => &[0, 2, 4, 5, 7, 9, 10],
            Scale::Lydian => &[0, 2, 4, 6, 7, 9, 11],
            Scale::Phrygian => &[0, 1, 3, 5, 7, 8, 10],
            Scale::HarmonicMinor => &[0, 2, 3, 5, 7, 8, 11],
            Scale::MajorPentatonic => &[0, 2, 4, 7, 9],
            Scale::MinorPentatonic => &[0, 3, 5, 7, 10],
            Scale::Blues => &[0, 3, 5, 6, 7, 10],
        }
    }

    pub fn from_name(name: &str) -> Option<Self> {
        Some(match name {
            "chromatic" => Scale::Chromatic,
            "major" => Scale::Major,
            "minor" => Scale::Minor,
            "dorian" => Scale::Dorian,
            "mixolydian" => Scale::Mixolydian,
            "lydian" => Scale::Lydian,
            "phrygian" => Scale::Phrygian,
            "harmonicMinor" => Scale::HarmonicMinor,
            "majorPentatonic" => Scale::MajorPentatonic,
            "minorPentatonic" => Scale::MinorPentatonic,
            "blues" => Scale::Blues,
            _ => return None,
        })
    }
}

pub fn midi_to_name(note: i32) -> String {
    let octave = note.div_euclid(12) - 1;
    format!("{}{}", NOTE_NAMES[note.rem_euclid(12) as usize], octave)
}

/// The signed pitch-class distance from `pc` to `target_pc`, folded into
/// [-6, 5] so ties resolve downward.
fn signed_delta(target_pc: i32, pc: i32) -> i32 {
    (target_pc - pc + 6).rem_euclid(12) - 6
}

/// The degree nearest `rel`, measured around the twelve-tone circle.
fn nearest_degree(degrees: &[i32], rel: i32) -> i32 {
    let mut best = degrees[0];
    let mut best_dist = 99;

    for &d in degrees {
        let dist = (rel - d).abs().min(12 - (rel - d).abs());
        if dist < best_dist {
            best_dist = dist;
            best = d;
        }
    }

    best
}

/// Snap a MIDI note into the given key and scale. Ties resolve downward.
pub fn snap_to_scale(note: i32, root: i32, scale: Scale) -> i32 {
    let degrees = scale.degrees();
    if degrees.len() == 12 {
        return note; // chromatic: no change
    }

    let pc = note.rem_euclid(12);
    let root_pc = root.rem_euclid(12);
    let rel = (pc - root_pc).rem_euclid(12);

    let best = nearest_degree(degrees, rel);
    note + signed_delta((root_pc + best) % 12, pc)
}

/// Snap to the nearest tone of the key's tonic triad.
pub fn snap_to_chord(note: i32, root: i32, scale: Scale) -> i32 {
    let degrees = scale.degrees();
    let chord: [i32; 3] = if degrees.len() == 12 {
        [0, 4, 7]
    } else {
        [degrees[0], degrees[2], degrees[4]]
    };

    let pc = note.rem_euclid(12);
    let root_pc = root.rem_euclid(12);
    let rel = (pc - root_pc).rem_euclid(12);

    let best = nearest_degree(&chord, rel);
    note + signed_delta((root_pc + best) % 12, pc)
}

/// Move `steps` scale degrees, folding through the key. In chromatic, steps are
/// plain semitones.
pub fn diatonic_transpose(note: i32, root: i32, scale: Scale, steps: i32) -> i32 {
    let degrees = scale.degrees();
    if degrees.len() == 12 {
        return note + steps;
    }

    let root_pc = root.rem_euclid(12);
    let snapped = snap_to_scale(note, root, scale);
    let rel_pc = (snapped - root_pc).rem_euclid(12);

    // `indexOf` yields -1 when absent; mirrored rather than tidied away, since
    // the arithmetic below is defined for it and the TypeScript relies on that.
    let di = degrees.iter().position(|&d| d == rel_pc).map_or(-1, |i| i as i32);

    let len = degrees.len() as i32;
    let total = di + steps;
    let octave = total.div_euclid(len);
    let wrapped = total.rem_euclid(len);

    snapped - rel_pc + degrees[wrapped as usize] + 12 * octave
}

pub fn clamp_midi(note: f64) -> i32 {
    js_round(note).clamp(0.0, 127.0) as i32
}
