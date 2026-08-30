//! Shared plumbing for the conformance tests.
#![allow(dead_code)]

use std::fs;
use std::path::PathBuf;

use serde_json::Value;

use mclassic_engine::cyclic::CyclicStep;
use mclassic_engine::music::Scale;
use mclassic_engine::project::{
    CyclicLengths, CyclicValues, CyclicVariables, Pattern, ProjectState, StepEvent, VoiceState,
};
use mclassic_engine::timemap::{TimeMap, TimeMapPoint};
use mclassic_engine::transform::{NoteOrderMix, VelocityRange};

/// Read a fixture emitted by the TypeScript engine.
pub fn fixture(name: &str) -> String {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("..")
        .join("src")
        .join("engine")
        .join("__goldens__")
        .join(name);

    fs::read_to_string(&path)
        .unwrap_or_else(|e| panic!("cannot read {}: {e}\nRun: npm run goldens", path.display()))
}

/// A float from its exact bit pattern.
pub fn f(hex: &str) -> f64 {
    f64::from_bits(u64::from_str_radix(hex, 16).expect("bad float bits"))
}

/// Walk a fixture, handing each non-comment line to `visit` along with the
/// header of the section it sits under and a `file:line` label for messages.
pub fn walk(name: &str, mut visit: impl FnMut(&str, &[&str], &str)) -> usize {
    let text = fixture(name);
    let mut section = String::new();
    let mut rows = 0usize;

    for (lineno, line) in text.lines().enumerate() {
        let line = line.trim();
        if line.is_empty() {
            continue;
        }
        if let Some(header) = line.strip_prefix('#') {
            section = header.trim().to_string();
            continue;
        }

        let fields: Vec<&str> = line.split(',').collect();
        visit(&section, &fields, &format!("{name}:{}", lineno + 1));
        rows += 1;
    }

    rows
}

/// Load a project the TypeScript engine emitted.
///
/// Reading the state rather than rebuilding it isolates whatever is under test:
/// a divergence is then the module's, never a project builder's.
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

pub fn load_project(name: &str) -> ProjectState {
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

