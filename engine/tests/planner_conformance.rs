//! The planner, checked against the committed traces.
//!
//! This is the gate the whole port exists for. The project state is read from
//! the JSON the TypeScript emitted rather than rebuilt here, so a divergence is
//! the planner's and not a project-builder's. The expected output is
//! `voices-NN.trace`, byte for byte.

mod common;

use common::fixture;
use serde_json::Value;

use mclassic_engine::cyclic::CyclicStep;
use mclassic_engine::music::Scale;
use mclassic_engine::project::{
    CyclicLengths, CyclicValues, CyclicVariables, Pattern, ProjectState, StepEvent, VoiceState,
};
use mclassic_engine::timemap::{TimeMap, TimeMapPoint};
use mclassic_engine::trace::{trace_detail, trace_project};
use mclassic_engine::transform::{NoteOrderMix, VelocityRange};

fn n(v: &Value, key: &str) -> f64 {
    v.get(key)
        .and_then(Value::as_f64)
        .unwrap_or_else(|| panic!("missing numeric field {key} in {v}"))
}

fn b(v: &Value, key: &str) -> bool {
    v.get(key).and_then(Value::as_bool).unwrap_or(false)
}

fn arr<'a>(v: &'a Value, key: &str) -> &'a Vec<Value> {
    v.get(key)
        .and_then(Value::as_array)
        .unwrap_or_else(|| panic!("missing array field {key}"))
}

fn steps(v: &Value, key: &str) -> Vec<StepEvent> {
    arr(v, key)
        .iter()
        .map(|s| StepEvent {
            pitches: arr(s, "pitches").iter().map(|p| p.as_i64().unwrap() as i32).collect(),
        })
        .collect()
}

/// A cyclic step is a bare level or an inclusive range; documents carry both.
fn cyclic_step(v: &Value) -> CyclicStep {
    match v {
        Value::Number(_) => CyclicStep::Level(v.as_f64().unwrap()),
        _ => CyclicStep::Range { min: n(v, "min"), max: n(v, "max") },
    }
}

fn cyclic_rows(v: &Value, kind: &str) -> Vec<Vec<CyclicStep>> {
    arr(v, kind)
        .iter()
        .map(|voice| voice.as_array().unwrap().iter().map(cyclic_step).collect())
        .collect()
}

fn lengths(v: &Value, kind: &str) -> Vec<i32> {
    arr(v, kind).iter().map(|x| x.as_i64().unwrap() as i32).collect()
}

fn time_map(v: &Value) -> TimeMap {
    TimeMap {
        points: arr(v, "points")
            .iter()
            .map(|p| TimeMapPoint { x: n(p, "x"), y: n(p, "y") })
            .collect(),
        length: n(v, "length"),
        denominator: n(v, "denominator"),
    }
}

fn voice(v: &Value) -> VoiceState {
    let mix = v.get("noteOrderMix").unwrap();
    let range = v.get("velocityRange").unwrap();

    VoiceState {
        pattern_index: n(v, "patternIndex") as usize,
        play_enabled: b(v, "playEnabled"),
        transposition: n(v, "transposition") as i32,
        note_order_mix: NoteOrderMix {
            original: n(mix, "original") as i32,
            cyclic: n(mix, "cyclic") as i32,
            utterly: n(mix, "utterly") as i32,
        },
        density: n(v, "density"),
        velocity_range: VelocityRange { low: n(range, "low"), high: n(range, "high") },
        time_base_numerator: n(v, "timeBaseNumerator"),
        time_base_denominator: n(v, "timeBaseDenominator"),
        phase: n(v, "phase"),
        time_distort: time_map(v.get("timeDistort").unwrap()),
        legato: n(v, "legato"),
        output_channels: arr(v, "outputChannels").iter().map(|c| c.as_i64().unwrap() as i32).collect(),
        mouse_advance: b(v, "mouseAdvance"),
    }
}

fn load(kind: &str, voices: u32) -> ProjectState {
    let name = format!("{kind}project-{voices:02}.json");
    let raw: Value = serde_json::from_str(&fixture(&name))
        .unwrap_or_else(|e| panic!("{name} is not valid JSON: {e}"));

    let cyclic = raw.get("cyclic").unwrap();
    let cl = raw.get("cyclicLengths").unwrap();
    let cv = raw.get("cyclicValues").unwrap();

    ProjectState {
        tempo: n(&raw, "tempo"),
        patterns: arr(&raw, "patterns")
            .iter()
            .map(|p| Pattern {
                steps: steps(p, "steps"),
                scrambled_steps: steps(p, "scrambledSteps"),
                output_length: n(p, "outputLength") as i32,
            })
            .collect(),
        voices: arr(&raw, "voices").iter().map(voice).collect(),
        root: n(&raw, "root") as i32,
        scale: Scale::from_name(raw.get("scale").unwrap().as_str().unwrap()).unwrap(),
        scale_snap: b(&raw, "scaleSnap"),
        seed: n(&raw, "seed") as u32,
        diatonic_transpose: b(&raw, "diatonicTranspose"),
        second_order_transpose: b(&raw, "secondOrderTranspose"),
        chord_tones: b(&raw, "chordTones"),
        cyclic: CyclicVariables {
            accent: cyclic_rows(cyclic, "accent"),
            legato: cyclic_rows(cyclic, "legato"),
            rhythm: cyclic_rows(cyclic, "rhythm"),
        },
        cyclic_lengths: CyclicLengths {
            accent: lengths(cl, "accent"),
            legato: lengths(cl, "legato"),
            rhythm: lengths(cl, "rhythm"),
        },
        cyclic_values: CyclicValues {
            legato: arr(cv, "legato").iter().map(|x| x.as_f64().unwrap()).collect(),
            rhythm: arr(cv, "rhythm").iter().map(|x| x.as_f64().unwrap()).collect(),
        },
    }
}

/// `traceDefaultProject` defaults: seed 1, eight seconds, four windows.
///
/// `kind` is "" for the plain fixture and "rich-" for the one that actually
/// exercises the planner: cyclic ranges, rests, all three Note Order sources,
/// several channels, staggered phases and a bent Time Distortion Map.
fn check(kind: &str, trace: &str, voices: u32) {
    let project = load(kind, voices);
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
        let project = load("rich-", voices);
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
