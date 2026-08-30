//! Ported from `src/engine/events.ts`.
//!
//! Everything between a planned note and a MIDI port happens here: note-offs
//! are generated, an overlapping retrigger has its stale future off withdrawn
//! and an early one issued at the replacement's onset, and the batch is put in
//! a total order.
//!
//! This is the part a golden trace cannot see. A trace stops at planned notes;
//! `lifecycle-NN.txt` records what an adapter actually receives.

use std::collections::HashMap;

use crate::planner::PlannedNote;

#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub enum OutputDestination {
    /// Ordered before `Synth`, matching JavaScript's `localeCompare` on the
    /// strings "midi" and "synth" — the tie-break is part of the contract, not
    /// an accident of how the enum happens to be written.
    Midi,
    Synth,
}

impl OutputDestination {
    pub fn name(self) -> &'static str {
        match self {
            OutputDestination::Midi => "midi",
            OutputDestination::Synth => "synth",
        }
    }

    pub fn from_name(name: &str) -> Option<Self> {
        match name {
            "midi" => Some(OutputDestination::Midi),
            "synth" => Some(OutputDestination::Synth),
            _ => None,
        }
    }
}

/// Event kinds, declared in their tie-break order: at one instant a program
/// change precedes a release, and a release precedes an attack.
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum EventKind {
    ProgramChange,
    NoteOff,
    NoteOn,
}

impl EventKind {
    pub fn name(self) -> &'static str {
        match self {
            EventKind::ProgramChange => "program-change",
            EventKind::NoteOff => "note-off",
            EventKind::NoteOn => "note-on",
        }
    }
}

/// One event bound for an output adapter.
///
/// Flat rather than a tagged union, because this crosses a C ABI. `note_id`,
/// `note` and `velocity` are meaningless on a program change, and `program` is
/// meaningless on the others; none of them participate in ordering.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct EngineEvent {
    pub kind: EventKind,
    pub at_sec: f64,
    pub at_tick: f64,
    pub sequence: u64,
    pub destination: OutputDestination,
    pub voice: usize,
    pub channel: i32,
    pub note_id: u64,
    pub note: i32,
    pub velocity: i32,
    pub program: i32,
}

/// The total order an adapter receives events in.
///
/// Time, then kind, then destination, then channel, then the sequence number —
/// which is monotonic, so the comparison never ends in a tie and the sort is
/// deterministic regardless of the algorithm underneath it.
pub fn compare_engine_events(a: &EngineEvent, b: &EngineEvent) -> std::cmp::Ordering {
    a.at_sec
        .partial_cmp(&b.at_sec)
        .expect("event times must not be NaN")
        .then(a.kind.cmp(&b.kind))
        .then(a.destination.cmp(&b.destination))
        .then(a.channel.cmp(&b.channel))
        .then(a.sequence.cmp(&b.sequence))
}

#[derive(Clone, Copy, Debug)]
struct ActiveNote {
    note_id: u64,
    off_sequence: u64,
}

/// A sounding note, identified the way the TypeScript keys its map.
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash)]
struct ActiveKey {
    destination: OutputDestination,
    channel: i32,
    note: i32,
}

/// One program change to enqueue.
#[derive(Clone, Copy, Debug)]
pub struct ProgramChange {
    pub voice: usize,
    pub channel: i32,
    pub program: i32,
}

/// Owns future note-offs and resolves overlapping notes before they reach an
/// output adapter.
#[derive(Debug, Default)]
pub struct NoteLifecycle {
    pending: Vec<EngineEvent>,
    active: HashMap<ActiveKey, ActiveNote>,
    sequence: u64,
    note_id: u64,
}

impl NoteLifecycle {
    pub fn new() -> Self {
        Self::default()
    }

    /// Take planned notes for every destination they are bound for.
    ///
    /// The notes are put in onset order first, and by Voice within an instant,
    /// because the retrigger rule depends on the order they arrive in: which
    /// note is "previous" is decided here.
    pub fn ingest(&mut self, notes: &[PlannedNote], destinations: &[OutputDestination]) {
        let mut sorted: Vec<&PlannedNote> = notes.iter().collect();

        // Stable, like `Array.prototype.sort`: notes agreeing on both keys keep
        // the order the planner emitted them in.
        sorted.sort_by(|a, b| {
            a.start_sec
                .partial_cmp(&b.start_sec)
                .expect("note onsets must not be NaN")
                .then(a.voice.cmp(&b.voice))
        });

        for note in sorted {
            for &destination in destinations {
                self.add_note(note, destination);
            }
        }
    }

    fn add_note(&mut self, note: &PlannedNote, destination: OutputDestination) {
        let key = ActiveKey { destination, channel: note.channel, note: note.note };

        // Already sounding: withdraw the note-off that has not happened yet and
        // release the old note at the replacement's onset instead.
        if let Some(previous) = self.active.get(&key).copied() {
            self.pending.retain(|event| event.sequence != previous.off_sequence);

            let sequence = self.take_sequence();
            self.pending.push(EngineEvent {
                kind: EventKind::NoteOff,
                at_sec: note.start_sec,
                at_tick: note.at_tick,
                sequence,
                destination,
                voice: note.voice,
                channel: note.channel,
                note_id: previous.note_id,
                note: note.note,
                velocity: 0,
                program: -1,
            });
        }

        let note_id = self.note_id;
        self.note_id += 1;

        let on_sequence = self.take_sequence();
        let off_sequence = self.take_sequence();

        self.pending.push(EngineEvent {
            kind: EventKind::NoteOn,
            at_sec: note.start_sec,
            at_tick: note.at_tick,
            sequence: on_sequence,
            destination,
            voice: note.voice,
            channel: note.channel,
            note_id,
            note: note.note,
            velocity: note.velocity,
            program: -1,
        });

        self.pending.push(EngineEvent {
            kind: EventKind::NoteOff,
            at_sec: note.start_sec + note.duration_sec.max(0.0),
            at_tick: note.at_tick + note.duration_ticks,
            sequence: off_sequence,
            destination,
            voice: note.voice,
            channel: note.channel,
            note_id,
            note: note.note,
            velocity: 0,
            program: -1,
        });

        self.active.insert(key, ActiveNote { note_id, off_sequence });
    }

    pub fn add_program_changes(&mut self, at_sec: f64, at_tick: f64, programs: &[ProgramChange]) {
        for item in programs {
            let sequence = self.take_sequence();
            self.pending.push(EngineEvent {
                kind: EventKind::ProgramChange,
                at_sec,
                at_tick,
                sequence,
                destination: OutputDestination::Midi,
                voice: item.voice,
                channel: item.channel,
                note_id: 0,
                note: 0,
                velocity: 0,
                program: item.program,
            });
        }
    }

    /// Everything due before `end_sec`, in order, removed from the queue.
    pub fn drain_before(&mut self, end_sec: f64) -> Vec<EngineEvent> {
        let mut ready: Vec<EngineEvent> = Vec::new();
        let mut future: Vec<EngineEvent> = Vec::new();

        for event in self.pending.drain(..) {
            if event.at_sec < end_sec {
                ready.push(event);
            } else {
                future.push(event);
            }
        }

        self.pending = future;
        ready.sort_by(compare_engine_events);

        // A note stops being active once its own off has gone out. The id check
        // matters: a retrigger has already replaced the entry, and that newer
        // note must not be forgotten by the older one's release.
        for event in &ready {
            if event.kind != EventKind::NoteOff {
                continue;
            }

            let key = ActiveKey {
                destination: event.destination,
                channel: event.channel,
                note: event.note,
            };

            if self.active.get(&key).map(|a| a.note_id) == Some(event.note_id) {
                self.active.remove(&key);
            }
        }

        ready
    }

    pub fn pending_count(&self) -> usize {
        self.pending.len()
    }

    pub fn reset(&mut self) {
        self.pending.clear();
        self.active.clear();
    }

    fn take_sequence(&mut self) -> u64 {
        let sequence = self.sequence;
        self.sequence += 1;
        sequence
    }
}
