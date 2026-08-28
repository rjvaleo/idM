//! M Classic's generative engine.
//!
//! The port is gated on conformance, not on review: every stage must reproduce
//! the fixtures in `src/engine/__goldens__/` that the TypeScript engine emits.
//! See the tests in `tests/`.

pub mod cyclic;
pub mod music;
pub mod planner;
pub mod num;
pub mod rng;
pub mod project;
pub mod timemap;
pub mod trace;
pub mod transform;

pub use cyclic::{pick_cyclic_level, CyclicLevelRange, CyclicStep};
pub use music::{clamp_midi, diatonic_transpose, snap_to_chord, snap_to_scale, Scale};
pub use rng::{brownian_step, BrownianWalk, Rng};
pub use planner::{make_cursors, plan_window, PlannedNote, VoiceCursor, PPQN};
pub use project::{Pattern, ProjectState, StepEvent, VoiceState};
pub use timemap::{TimeMap, TimeMapPoint};
pub use trace::trace_project;
pub use transform::{
    gate, make_cyclic_order, next_mixed_step_index, next_step_index,
    note_order_mix_from_edges, normalize_velocity_range, step_duration_seconds,
    velocity_for_accent, NoteOrder, NoteOrderCursor, NoteOrderMix, NoteSource, VelocityRange,
};
