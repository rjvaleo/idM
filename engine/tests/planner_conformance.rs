//! The planner, checked against the committed traces.
//!
//! This is the gate the whole port exists for. The project state is read from
//! the JSON the TypeScript emitted rather than rebuilt here, so a divergence is
//! the planner's and not a project-builder's. The expected output is
//! `voices-NN.trace`, byte for byte.

mod common;

use common::{fixture, load_project};

use mclassic_engine::trace::{trace_detail, trace_project};

/// `traceDefaultProject` defaults: seed 1, eight seconds, four windows.
///
/// `kind` is "" for the plain fixture and "rich-" for the one that actually
/// exercises the planner: cyclic ranges, rests, all three Note Order sources,
/// several channels, staggered phases and a bent Time Distortion Map.
fn check(kind: &str, trace: &str, voices: u32) {
    let project = load_project(&format!("{kind}project-{voices:02}.json"));
    assert_eq!(project.voices.len(), voices as usize, "{voices}: wrong voice count loaded");

    let got = trace_project(&project, 1, 8.0, 4);
    let want = fixture(&format!("{trace}-{voices:02}.trace"));
    let want = want.trim_end_matches('\n');

    // Two empty strings compare equal. Guard against a planner that silently
    // produces nothing agreeing with a fixture nobody noticed was blank.
    assert!(
        want.lines().count() >= 8,
        "{trace}-{voices:02}.trace holds only {} lines; the fixture looks empty",
        want.lines().count()
    );
    assert!(!got.is_empty(), "the Rust planner produced no notes at all");

    if got != want {
        let g: Vec<&str> = got.lines().collect();
        let w: Vec<&str> = want.lines().collect();
        let first = (0..g.len().max(w.len()))
            .find(|&i| g.get(i) != w.get(i))
            .unwrap_or(0);

        panic!(
            "{trace}-{voices:02}.trace differs\n  rust {} notes, typescript {} notes\n  \
             first difference at line {}\n    rust: {:?}\n    ts:   {:?}",
            g.len(), w.len(), first + 1, g.get(first), w.get(first)
        );
    }
}

#[test]
fn planner_reproduces_the_plain_traces() {
    for voices in [1, 4, 8, 16] {
        check("", "voices", voices);
    }
}

/// Scale Snap and Chord Tones, which the rich fixture hides: Diatonic
/// Transpose snaps internally, so the snap that follows it is a no-op there.
#[test]
fn planner_reproduces_the_guardrail_traces() {
    for voices in [1, 4, 8, 16] {
        check("guard-", "guard", voices);
    }
}

/// Seconds, the Rhythm multiplier and the Note Order source - none of which a
/// trace carries, and so none of which a trace can pin.
#[test]
fn planner_reproduces_the_seconds_and_sources() {
    for voices in [1u32, 4, 8, 16] {
        let project = load_project(&format!("rich-project-{voices:02}.json"));
        let got = trace_detail(&project, 1, 8.0, 4);
        let want = fixture(&format!("detail-{voices:02}.txt"));
        let want = want.trim_end_matches('\n');

        assert!(want.lines().count() >= 8, "detail-{voices:02}.txt looks empty");

        if got != want {
            let g: Vec<&str> = got.lines().collect();
            let w: Vec<&str> = want.lines().collect();
            let first = (0..g.len().max(w.len())).find(|&i| g.get(i) != w.get(i)).unwrap_or(0);
            panic!(
                "detail-{voices:02}.txt differs\n  rust {} rows, typescript {} rows\n  \
                 first difference at row {}\n    rust: {:?}\n    ts:   {:?}",
                g.len(), w.len(), first + 1, g.get(first), w.get(first)
            );
        }
    }
}

/// The one that matters. The plain fixture leaves most of the planner
/// unobserved; this drives the cyclic draws, the rests, the scrambled list and
/// the second-order transposition.
#[test]
fn planner_reproduces_the_rich_traces() {
    for voices in [1, 4, 8, 16] {
        check("rich-", "rich", voices);
    }
}
