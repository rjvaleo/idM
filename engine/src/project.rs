//! The project state the planner reads.
//!
//! Deliberately narrower than `types.ts`: this carries what planning depends
//! on, and nothing about MIDI assignments, editing modes or window layout. The
//! document model that reads and writes the full shape arrives with `document`.

use crate::cyclic::CyclicStep;
use crate::music::Scale;
use crate::timemap::TimeMap;
use crate::transform::{NoteOrderMix, VelocityRange};

/// One step of a Pattern. Only the pitches matter to planning.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct StepEvent {
    pub pitches: Vec<i32>,
}

/// A Pattern is the raw note material — M's core unit.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct Pattern {
    pub steps: Vec<StepEvent>,
    /// M's stored Cyclic Random copy of the Original list.
    pub scrambled_steps: Vec<StepEvent>,
    /// Number of steps actually played; never more than `steps.len()`.
    pub output_length: i32,
}

/// Live state of one Voice — a path through the program.
#[derive(Clone, Debug, PartialEq)]
pub struct VoiceState {
    pub pattern_index: usize,
    pub play_enabled: bool,
    /// Semitones, or scale steps when `diatonic_transpose` is set.
    pub transposition: i32,
    pub note_order_mix: NoteOrderMix,
    /// Probability in 0..1 that a step sounds.
    pub density: f64,
    pub velocity_range: VelocityRange,
    /// Multiplier that slows the Voice.
    pub time_base_numerator: f64,
    /// Division of a whole note: 4 = quarter, 8 = eighth. Zero stops the Voice.
    pub time_base_denominator: f64,
    /// Initial delay in M ticks; 96 M ticks are one quarter note.
    pub phase: f64,
    pub time_distort: TimeMap,
    /// Per-Voice multiplier over the Cyclic Legato percentage.
    pub legato: f64,
    /// Orchestration: any combination of channels 1..16.
    pub output_channels: Vec<i32>,
    /// When set, the Voice advances only by Input Control.
    pub mouse_advance: bool,
}

/// Which of the three cyclic variables is being read.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum CyclicKind {
    Accent,
    Legato,
    Rhythm,
}

/// Five-level, sixteen-step modulation cycles, per variable and per Voice.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct CyclicVariables {
    pub accent: Vec<Vec<CyclicStep>>,
    pub legato: Vec<Vec<CyclicStep>>,
    pub rhythm: Vec<Vec<CyclicStep>>,
}

impl CyclicVariables {
    pub fn get(&self, kind: CyclicKind) -> &[Vec<CyclicStep>] {
        match kind {
            CyclicKind::Accent => &self.accent,
            CyclicKind::Legato => &self.legato,
            CyclicKind::Rhythm => &self.rhythm,
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq)]
pub struct CyclicLengths {
    pub accent: Vec<i32>,
    pub legato: Vec<i32>,
    pub rhythm: Vec<i32>,
}

impl CyclicLengths {
    pub fn get(&self, kind: CyclicKind) -> &[i32] {
        match kind {
            CyclicKind::Accent => &self.accent,
            CyclicKind::Legato => &self.legato,
            CyclicKind::Rhythm => &self.rhythm,
        }
    }
}

/// The values each of the five levels maps to. Accent has none: its level is
/// the velocity step itself.
#[derive(Clone, Debug, Default, PartialEq)]
pub struct CyclicValues {
    pub legato: Vec<f64>,
    pub rhythm: Vec<f64>,
}

#[derive(Clone, Debug, PartialEq)]
pub struct ProjectState {
    pub tempo: f64,
    pub patterns: Vec<Pattern>,
    pub voices: Vec<VoiceState>,
    /// Key root pitch class, 0..11.
    pub root: i32,
    pub scale: Scale,
    pub scale_snap: bool,
    pub seed: u32,
    /// Interpret transposition as scale steps.
    pub diatonic_transpose: bool,
    /// Stack Voice transpositions cumulatively - a harmonizer feeding a
    /// harmonizer, which builds implied chords.
    pub second_order_transpose: bool,
    /// Snap final pitches to the tonic triad.
    pub chord_tones: bool,
    pub cyclic: CyclicVariables,
    pub cyclic_lengths: CyclicLengths,
    pub cyclic_values: CyclicValues,
}

/// The Voice count is `voices.len()`, never a separate field.
pub fn voice_count(project: &ProjectState) -> usize {
    project.voices.len()
}
