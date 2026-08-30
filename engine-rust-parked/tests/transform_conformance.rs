//! `transform.ts` checked against the Rust port.
//!
//! The cursor sequences matter as much as the values: `pos`, `last` and the
//! Brownian position are carried from step to step, so a port that produces the
//! right first index and the wrong cursor diverges on the second.

mod common;

use common::{f, walk};
use mclassic_engine::transform::{
    gate, make_cyclic_order, next_mixed_step_index, next_step_index,
    note_order_mix_from_edges, normalize_velocity_range, step_duration_seconds,
    velocity_for_accent, NoteOrder, NoteOrderCursor, NoteOrderMix, VelocityRange,
};
use mclassic_engine::Rng;

fn mix(id: &str) -> NoteOrderMix {
    let (original, cyclic, utterly) = match id {
        "all-original" => (100, 0, 0),
        "all-cyclic" => (0, 100, 0),
        "all-utterly" => (0, 0, 100),
        "even" => (34, 33, 33),
        "classic" => (60, 30, 10),
        "none" => (0, 0, 0),
        other => panic!("unknown mix {other}"),
    };
    NoteOrderMix { original, cyclic, utterly }
}

#[test]
fn transform_reproduces_the_typescript_engine_exactly() {
    let mut rng = Rng::new(0);
    let mut cursor = NoteOrderCursor::default();
    let mut checked = 0usize;

    let rows = walk("transform.txt", |section, f_, at| {
        match section {
            s if s.starts_with("stepDurationSeconds") => {
                let (tempo, n, d, want) = (f(f_[0]), f(f_[1]), f(f_[2]), f(f_[3]));
                assert_eq!(
                    step_duration_seconds(tempo, n, d).to_bits(), want.to_bits(),
                    "{at}: step_duration_seconds({tempo}, {n}, {d})"
                );
            }

            s if s.starts_with("normalizeVelocityRange") => {
                let (low, high) = (f(f_[0]), f(f_[1]));
                let (got_low, got_high) = normalize_velocity_range(VelocityRange { low, high });
                assert_eq!(got_low, f_[2].parse::<i32>().unwrap(), "{at}: low of [{low}, {high}]");
                assert_eq!(got_high, f_[3].parse::<i32>().unwrap(), "{at}: high of [{low}, {high}]");
            }

            s if s.starts_with("velocityForAccent") => {
                let (low, high, level) = (f(f_[0]), f(f_[1]), f(f_[2]));
                let want = f_[3].parse::<i32>().unwrap();
                assert_eq!(
                    velocity_for_accent(VelocityRange { low, high }, level), want,
                    "{at}: velocity_for_accent([{low}, {high}], {level})"
                );
            }

            s if s.starts_with("gate") => {
                let (seed, density, index) = (
                    f_[0].parse::<u32>().unwrap(),
                    f(f_[1]),
                    f_[2].parse::<usize>().unwrap(),
                );
                let want = f_[3] == "1";
                if index == 0 {
                    rng = Rng::new(seed);
                }
                assert_eq!(gate(density, &mut rng), want, "{at}: gate({density}) #{index}");
            }

            s if s.starts_with("nextStepIndex") => {
                let order = NoteOrder::from_name(f_[0]).unwrap();
                let (seed, length, index) = (
                    f_[1].parse::<u32>().unwrap(),
                    f_[2].parse::<i32>().unwrap(),
                    f_[3].parse::<usize>().unwrap(),
                );
                if index == 0 {
                    rng = Rng::new(seed);
                    cursor = NoteOrderCursor::default();
                }

                let (got, next) = next_step_index(order, cursor, length, &mut rng);
                cursor = next;

                assert_eq!(got, f_[4].parse::<i32>().unwrap(), "{at}: {} index #{index}", f_[0]);
                assert_eq!(cursor.pos, f_[5].parse::<i64>().unwrap(), "{at}: {} pos #{index}", f_[0]);
                assert_eq!(cursor.last, f_[6].parse::<i32>().unwrap(), "{at}: {} last #{index}", f_[0]);
                assert_eq!(
                    cursor.bval.to_bits(), f(f_[7]).to_bits(),
                    "{at}: {} bval #{index}", f_[0]
                );
            }

            s if s.starts_with("makeCyclicOrder") => {
                let (length, seed) = (f_[0].parse::<i32>().unwrap(), f_[1].parse::<u32>().unwrap());
                let want: Vec<i32> = f_[2].split(';').map(|v| v.parse().unwrap()).collect();
                assert_eq!(
                    make_cyclic_order(length, seed), want,
                    "{at}: make_cyclic_order({length}, {seed})"
                );
            }

            s if s.starts_with("noteOrderMixFromEdges") => {
                let (o, u) = (f(f_[0]), f(f_[1]));
                let got = note_order_mix_from_edges(o, u);
                assert_eq!(got.original, f_[2].parse::<i32>().unwrap(), "{at}: original");
                assert_eq!(got.cyclic, f_[3].parse::<i32>().unwrap(), "{at}: cyclic");
                assert_eq!(got.utterly, f_[4].parse::<i32>().unwrap(), "{at}: utterly");
            }

            s if s.starts_with("nextMixedStepIndex") => {
                let m = mix(f_[0]);
                let (seed, length, index) = (
                    f_[1].parse::<u32>().unwrap(),
                    f_[2].parse::<i32>().unwrap(),
                    f_[3].parse::<usize>().unwrap(),
                );
                if index == 0 {
                    rng = Rng::new(seed);
                    cursor = NoteOrderCursor::default();
                }

                let (got, source, next) = next_mixed_step_index(m, cursor, length, &mut rng);
                cursor = next;

                assert_eq!(got, f_[4].parse::<i32>().unwrap(), "{at}: {} index #{index}", f_[0]);
                assert_eq!(source.name(), f_[5], "{at}: {} source #{index}", f_[0]);
                assert_eq!(cursor.pos, f_[6].parse::<i64>().unwrap(), "{at}: {} pos #{index}", f_[0]);
                assert_eq!(cursor.last, f_[7].parse::<i32>().unwrap(), "{at}: {} last #{index}", f_[0]);
            }

            other => panic!("{at}: unknown section {other:?}"),
        }
        checked += 1;
    });

    assert_eq!(rows, checked);
    assert!(checked > 900, "only {checked} rows checked; fixture looks truncated");
}
