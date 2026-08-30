//! `music.ts` checked against the Rust port.
//!
//! Notes outside 0..127 are included deliberately: the pitch-class arithmetic
//! is modular, JavaScript's `%` keeps the sign of the dividend, and a port that
//! uses Rust's `%` instead of `rem_euclid` passes every positive case.

mod common;

use common::{f, walk};
use mclassic_engine::music::{
    clamp_midi, diatonic_transpose, midi_to_name, snap_to_chord, snap_to_scale, Scale,
};

fn scale(name: &str) -> Scale {
    Scale::from_name(name).unwrap_or_else(|| panic!("unknown scale {name}"))
}

#[test]
fn music_reproduces_the_typescript_engine_exactly() {
    let mut checked = 0usize;

    let rows = walk("music.txt", |section, f_, at| {
        match section {
            s if s.starts_with("scales:") => {
                let want: Vec<i32> = f_[1].split(';').map(|d| d.parse().unwrap()).collect();
                assert_eq!(scale(f_[0]).degrees(), want.as_slice(), "{at}: {} degrees", f_[0]);
            }

            s if s.starts_with("snapToScale") => {
                let (root, note, want) = (
                    f_[1].parse::<i32>().unwrap(),
                    f_[2].parse::<i32>().unwrap(),
                    f_[3].parse::<i32>().unwrap(),
                );
                assert_eq!(
                    snap_to_scale(note, root, scale(f_[0])), want,
                    "{at}: snap_to_scale({note}, {root}, {})", f_[0]
                );
            }

            s if s.starts_with("snapToChord") => {
                let (root, note, want) = (
                    f_[1].parse::<i32>().unwrap(),
                    f_[2].parse::<i32>().unwrap(),
                    f_[3].parse::<i32>().unwrap(),
                );
                assert_eq!(
                    snap_to_chord(note, root, scale(f_[0])), want,
                    "{at}: snap_to_chord({note}, {root}, {})", f_[0]
                );
            }

            s if s.starts_with("diatonicTranspose") => {
                let (root, note, steps, want) = (
                    f_[1].parse::<i32>().unwrap(),
                    f_[2].parse::<i32>().unwrap(),
                    f_[3].parse::<i32>().unwrap(),
                    f_[4].parse::<i32>().unwrap(),
                );
                assert_eq!(
                    diatonic_transpose(note, root, scale(f_[0]), steps), want,
                    "{at}: diatonic_transpose({note}, {root}, {}, {steps})", f_[0]
                );
            }

            s if s.starts_with("clampMidi") => {
                let note = f(f_[0]);
                let want = f_[1].parse::<i32>().unwrap();
                assert_eq!(clamp_midi(note), want, "{at}: clamp_midi({note})");
            }

            s if s.starts_with("midiToName") => {
                let note = f_[0].parse::<i32>().unwrap();
                assert_eq!(midi_to_name(note), f_[1], "{at}: midi_to_name({note})");
            }

            other => panic!("{at}: unknown section {other:?}"),
        }
        checked += 1;
    });

    assert_eq!(rows, checked);
    assert!(checked > 2500, "only {checked} rows checked; fixture looks truncated");
}
