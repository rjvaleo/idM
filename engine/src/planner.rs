//! Ported from `src/engine/planner.ts` — the pure heart of the engine.
//!
//! Given project state, per-Voice cursors and a time window, produce the notes
//! to schedule and the advanced cursors. No audio, no timers.
//!
//! The order in which randomness is consumed is part of the contract, not an
//! implementation detail. Each step draws accent, then legato, then rhythm, and
//! only then - if the Voice is playing - the note-order index and the density
//! gate. Moving any of those reorders every draw that follows and silently
//! changes the music, which is precisely what the golden traces exist to catch.

use crate::cyclic::pick_cyclic_level;
use crate::music::{clamp_midi, diatonic_transpose, snap_to_chord, snap_to_scale};
use crate::num::js_round;
use crate::project::{CyclicKind, ProjectState};
use crate::rng::Rng;
use crate::transform::{
    gate, next_mixed_step_index, step_duration_seconds, velocity_for_accent, NoteOrderCursor,
    NoteSource,
};

/// Pulses per quarter note on the shared transport timeline.
pub const PPQN: f64 = 960.0;

/// M ticks per quarter note, the unit the Phase control is expressed in.
const M_TICKS_PER_QUARTER: f64 = 96.0;

/// The cyclic position wraps every sixteen steps.
const CYCLIC_POSITIONS: i64 = 16;

#[derive(Clone, Debug, PartialEq)]
pub struct PlannedNote {
    pub voice: usize,
    pub note: i32,
    pub velocity: i32,
    pub channel: i32,
    /// AudioContext time domain.
    pub start_sec: f64,
    pub duration_sec: f64,
    /// Absolute musical position on the shared transport timeline.
    pub at_tick: f64,
    pub duration_ticks: f64,
    /// Which of the three Note Order lists this step came from.
    pub source: NoteSource,
    /// The Rhythm variable's multiplier for this Voice on this step - its
    /// clock divider, carried out so a display can give each lane its own rate.
    pub rhythm: f64,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct PlannedStep {
    pub voice: usize,
    pub step: i32,
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct VoiceCursor {
    pub order: NoteOrderCursor,
    /// Real time of the next event - what actually gets scheduled.
    pub next_time_sec: f64,
    /// Absolute time this Voice's clock started: the origin of its time map.
    pub origin_sec: f64,
    /// Undistorted clock time elapsed since `origin_sec`.
    pub clock_sec: f64,
    pub cyclic_pos: i64,
    pub transport_tick: f64,
}

/// The level drawn from a Voice's cycle at `position`.
fn cyclic_level(
    state: &ProjectState,
    kind: CyclicKind,
    voice: usize,
    position: i64,
    rng: &mut Rng,
) -> i32 {
    let cycle = &state.cyclic.get(kind)[voice];
    let length = i64::from(state.cyclic_lengths.get(kind)[voice]);

    pick_cyclic_level(cycle[(position % length) as usize], rng)
}

/// The value that level maps to. Legato is a percentage; Rhythm is a factor.
fn cyclic_multiplier(
    state: &ProjectState,
    kind: CyclicKind,
    voice: usize,
    position: i64,
    rng: &mut Rng,
) -> f64 {
    let level = cyclic_level(state, kind, voice, position, rng);

    let value = match kind {
        CyclicKind::Legato => state.cyclic_values.legato[level as usize],
        CyclicKind::Rhythm => state.cyclic_values.rhythm[level as usize],
        CyclicKind::Accent => unreachable!("accent has no value table; its level is the velocity"),
    };

    if kind == CyclicKind::Legato {
        value / 100.0
    } else {
        value
    }
}

/// One fresh cursor per Voice, all starting at `start_sec`.
pub fn make_cursors(state: &ProjectState, start_sec: f64) -> Vec<VoiceCursor> {
    state
        .voices
        .iter()
        .map(|voice| {
            let phase = voice.phase.max(0.0);
            let phase_sec = phase * (60.0 / state.tempo) / M_TICKS_PER_QUARTER;

            VoiceCursor {
                order: NoteOrderCursor::default(),
                next_time_sec: start_sec + phase_sec,
                origin_sec: start_sec + phase_sec,
                clock_sec: 0.0,
                cyclic_pos: 0,
                transport_tick: js_round(phase * PPQN / M_TICKS_PER_QUARTER),
            }
        })
        .collect()
}

/// Plan every note beginning within `[window_start, window_end)`.
///
/// `window_start` is unused directly - each Voice carries its own
/// `next_time_sec`, which is where playback actually resumes - but it documents
/// the caller's scheduling window.
pub fn plan_window(
    state: &ProjectState,
    cursors: &[VoiceCursor],
    rngs: &mut [Rng],
    _window_start: f64,
    window_end: f64,
) -> (Vec<PlannedNote>, Vec<VoiceCursor>, Vec<PlannedStep>) {
    let mut notes = Vec::new();
    let mut steps = Vec::new();
    let mut next_cursors = Vec::with_capacity(state.voices.len());

    // Second-Order Transpose stacks the Voices cumulatively, so each adds the
    // transpositions of the Voices above it, building implied chords.
    let eff_trans: Vec<i32> = if state.second_order_transpose {
        let mut acc = 0;
        state
            .voices
            .iter()
            .map(|v| {
                acc += v.transposition;
                acc
            })
            .collect()
    } else {
        state.voices.iter().map(|v| v.transposition).collect()
    };

    for (vi, v) in state.voices.iter().enumerate() {
        let cursor = cursors[vi];

        if v.time_base_denominator <= 0.0 || v.mouse_advance {
            next_cursors.push(cursor); // advanced only by Input Control
            continue;
        }

        let rng = &mut rngs[vi];
        let pat = &state.patterns[v.pattern_index];
        let out_len = (pat.output_length as usize).min(pat.steps.len()) as i32;
        let step_dur =
            step_duration_seconds(state.tempo, v.time_base_numerator, v.time_base_denominator);

        let mut order = cursor.order;
        let mut clock_sec = cursor.clock_sec;
        let mut cyclic_pos = cursor.cyclic_pos;
        let mut transport_tick = cursor.transport_tick;

        // Steps advance clock time evenly; the Time Distortion Map decides
        // where that lands in real time. With a neutral map the two agree.
        let real_at = |clock: f64| {
            cursor.origin_sec + v.time_distort.distort_clock_seconds(state.tempo, clock)
        };

        let mut t = real_at(clock_sec);
        let base_ticks = PPQN * 4.0 * v.time_base_numerator / v.time_base_denominator;

        if out_len <= 0 {
            // Nothing to play; keep the clock from spinning forever.
            clock_sec += (window_end - t).max(0.0);
            t = real_at(clock_sec);
        } else {
            while t < window_end {
                let velocity = velocity_for_accent(
                    v.velocity_range,
                    f64::from(cyclic_level(state, CyclicKind::Accent, vi, cyclic_pos, rng)),
                );
                let legato = cyclic_multiplier(state, CyclicKind::Legato, vi, cyclic_pos, rng);
                let rhythm = cyclic_multiplier(state, CyclicKind::Rhythm, vi, cyclic_pos, rng);

                let next_clock_sec = clock_sec + step_dur * rhythm;
                let next_onset_sec = real_at(next_clock_sec);
                let onset_interval_sec = (next_onset_sec - t).max(0.0);

                if v.play_enabled {
                    let (index, source, advanced) =
                        next_mixed_step_index(v.note_order_mix, order, out_len, rng);
                    order = advanced;
                    steps.push(PlannedStep { voice: vi, step: index });

                    let list = if source == NoteSource::Cyclic {
                        &pat.scrambled_steps
                    } else {
                        &pat.steps
                    };
                    let step = &list[index as usize];

                    if velocity > 0 && !step.pitches.is_empty() && gate(v.density, rng) {
                        for &p in &step.pitches {
                            let mut n = if state.diatonic_transpose {
                                diatonic_transpose(p, state.root, state.scale, eff_trans[vi])
                            } else {
                                p + eff_trans[vi]
                            };

                            if state.scale_snap {
                                n = snap_to_scale(n, state.root, state.scale);
                            }
                            if state.chord_tones {
                                n = snap_to_chord(n, state.root, state.scale);
                            }

                            for &channel in &v.output_channels {
                                notes.push(PlannedNote {
                                    voice: vi,
                                    note: clamp_midi(f64::from(n)),
                                    velocity,
                                    channel,
                                    start_sec: t,
                                    duration_sec: onset_interval_sec * v.legato * legato,
                                    at_tick: js_round(transport_tick),
                                    duration_ticks: js_round(
                                        base_ticks * rhythm * v.legato * legato,
                                    )
                                    .max(0.0),
                                    source,
                                    rhythm,
                                });
                            }
                        }
                    }
                }

                clock_sec = next_clock_sec;
                transport_tick += js_round(base_ticks * rhythm);
                t = next_onset_sec;
                cyclic_pos = (cyclic_pos + 1) % CYCLIC_POSITIONS;
            }
        }

        next_cursors.push(VoiceCursor {
            order,
            next_time_sec: t,
            origin_sec: cursor.origin_sec,
            clock_sec,
            cyclic_pos,
            transport_tick,
        });
    }

    (notes, next_cursors, steps)
}
