//! `NoteLifecycle` checked against the TypeScript engine.
//!
//! This is the layer a golden trace cannot see. A trace stops at planned notes;
//! everything that decides what reaches a MIDI port — generated releases,
//! retrigger resolution, and the total order — happens after that.

mod common;

use common::{fixture, load_project};

use mclassic_engine::events::{EventKind, NoteLifecycle, OutputDestination, ProgramChange};
use mclassic_engine::planner::{make_cursors, plan_window};
use mclassic_engine::trace::trace_rngs;

/// `traceLifecycle`'s defaults: seed 1, eight seconds, four windows.
fn check(voices: u32) {
    let project = load_project(&format!("rich-project-{voices:02}.json"));
    let mut rngs = trace_rngs(project.voices.len(), 1);
    let mut cursors = make_cursors(&project, 0.0);
    let mut lifecycle = NoteLifecycle::new();

    // Both, because the ordering rule sorts on the destination and one
    // destination cannot show that it does.
    let destinations = [OutputDestination::Synth, OutputDestination::Midi];

    let programs: Vec<ProgramChange> = project
        .voices
        .iter()
        .enumerate()
        .flat_map(|(index, voice)| {
            voice.output_channels.iter().map(move |&channel| ProgramChange {
                voice: index,
                channel,
                program: ((index * 7) % 128) as i32,
            })
        })
        .collect();

    lifecycle.add_program_changes(0.0, 0.0, &programs);

    let span_sec = 8.0;
    let windows = 4usize;
    let step = span_sec / windows as f64;
    let mut rows: Vec<String> = Vec::new();

    for w in 0..windows {
        let end = step * (w + 1) as f64;
        let (notes, next, _steps) =
            plan_window(&project, &cursors, &mut rngs, step * w as f64, end);
        cursors = next;

        lifecycle.ingest(&notes, &destinations);

        for event in lifecycle.drain_before(end) {
            let is_program = event.kind == EventKind::ProgramChange;
            rows.push(format!(
                "{},{:016x},{},{},{},{},{},{},{},{},{}",
                event.kind.name(),
                event.at_sec.to_bits(),
                fmt(event.at_tick),
                event.sequence,
                event.destination.name(),
                event.voice,
                event.channel,
                if is_program { -1 } else { event.note_id as i64 },
                if is_program { -1 } else { event.note as i64 },
                if is_program { -1 } else { event.velocity as i64 },
                if is_program { event.program as i64 } else { -1 },
            ));
        }

        rows.push(format!("# window {w} drained, {} pending", lifecycle.pending_count()));
    }

    let got = rows.join("\n");
    let want = fixture(&format!("lifecycle-{voices:02}.txt"));
    let want = want.trim_end_matches('\n');

    assert!(
        want.lines().filter(|l| !l.starts_with('#')).count() >= 20,
        "lifecycle-{voices:02}.txt looks empty"
    );

    if got != want {
        let g: Vec<&str> = got.lines().collect();
        let w: Vec<&str> = want.lines().collect();
        let first = (0..g.len().max(w.len())).find(|&i| g.get(i) != w.get(i)).unwrap_or(0);
        panic!(
            "lifecycle-{voices:02}.txt differs\n  rust {} rows, typescript {} rows\n  \
             first difference at row {}\n    rust: {:?}\n    ts:   {:?}",
            g.len(), w.len(), first + 1, g.get(first), w.get(first)
        );
    }
}

/// Ticks print as integers, the way `Array.prototype.join` renders them.
fn fmt(value: f64) -> String {
    if value.is_finite() && value.fract() == 0.0 && value.abs() < 9.0e18 {
        format!("{}", value as i64)
    } else {
        format!("{value}")
    }
}

#[test]
fn lifecycle_reproduces_the_typescript_engine_exactly() {
    for voices in [1, 4, 8, 16] {
        check(voices);
    }
}

/// Guards the retrigger path specifically: if the fixture stopped containing
/// overlaps, the suite above would still pass while testing nothing.
#[test]
fn the_fixture_actually_exercises_retriggers() {
    let text = fixture("lifecycle-16.txt");
    let rows: Vec<Vec<&str>> = text
        .lines()
        .filter(|l| !l.starts_with('#') && !l.trim().is_empty())
        .map(|l| l.split(',').collect())
        .collect();

    // A retrigger shows up as a note-off sharing its instant, destination,
    // channel and pitch with a note-on.
    let mut coincident = 0;
    for off in rows.iter().filter(|r| r[0] == "note-off") {
        if rows.iter().any(|on| {
            on[0] == "note-on" && on[1] == off[1] && on[4] == off[4] && on[6] == off[6] && on[8] == off[8]
        }) {
            coincident += 1;
        }
    }

    assert!(coincident > 50, "only {coincident} retriggers in the fixture");

    let destinations: std::collections::BTreeSet<&str> = rows.iter().map(|r| r[4]).collect();
    assert_eq!(destinations.len(), 2, "both destinations must appear: {destinations:?}");

    let programs = rows.iter().filter(|r| r[0] == "program-change").count();
    assert!(programs > 0, "no program changes in the fixture");
}
