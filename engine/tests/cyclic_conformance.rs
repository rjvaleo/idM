//! `cyclic.ts` checked against the Rust port.

mod common;

use common::{f, walk};
use mclassic_engine::cyclic::{
    cyclic_length_from_step_index, normalize_cyclic_step, pick_cyclic_level, CyclicStep,
};
use mclassic_engine::Rng;

#[test]
fn cyclic_reproduces_the_typescript_engine_exactly() {
    let mut rng = Rng::new(0);
    let mut checked = 0usize;

    let rows = walk("cyclic.txt", |section, f_, at| {
        match section {
            s if s.starts_with("normalizeCyclicStep(number)") => {
                let input = f(f_[0]);
                let got = normalize_cyclic_step(CyclicStep::Level(input));
                assert_eq!(got.min, f_[1].parse::<i32>().unwrap(), "{at}: min of {input}");
                assert_eq!(got.max, f_[2].parse::<i32>().unwrap(), "{at}: max of {input}");
            }

            s if s.starts_with("normalizeCyclicStep(range)") => {
                let (min, max) = (f(f_[0]), f(f_[1]));
                let got = normalize_cyclic_step(CyclicStep::Range { min, max });
                assert_eq!(got.min, f_[2].parse::<i32>().unwrap(), "{at}: min of [{min}, {max}]");
                assert_eq!(got.max, f_[3].parse::<i32>().unwrap(), "{at}: max of [{min}, {max}]");
            }

            s if s.starts_with("pickCyclicLevel(sequence)") => {
                let (seed, index, min, max, want) = (
                    f_[0].parse::<u32>().unwrap(),
                    f_[1].parse::<usize>().unwrap(),
                    f_[2].parse::<f64>().unwrap(),
                    f_[3].parse::<f64>().unwrap(),
                    f_[4].parse::<i32>().unwrap(),
                );
                // One generator across the whole sequence: this is what fails
                // if a point range is allowed to spend a draw.
                if index == 0 {
                    rng = Rng::new(seed);
                }
                let got = pick_cyclic_level(CyclicStep::Range { min, max }, &mut rng);
                assert_eq!(got, want, "{at}: sequence step {index} over [{min}, {max}]");
            }

            s if s.starts_with("pickCyclicLevel") => {
                let (seed, min, max, index, want) = (
                    f_[0].parse::<u32>().unwrap(),
                    f_[1].parse::<f64>().unwrap(),
                    f_[2].parse::<f64>().unwrap(),
                    f_[3].parse::<usize>().unwrap(),
                    f_[4].parse::<i32>().unwrap(),
                );
                // A fresh Rng at index 0: a point range must consume no draw,
                // so the whole sequence shifts if that ever changes.
                if index == 0 {
                    rng = Rng::new(seed);
                }
                let got = pick_cyclic_level(CyclicStep::Range { min, max }, &mut rng);
                assert_eq!(got, want, "{at}: pick [{min}, {max}] #{index}");
            }

            s if s.starts_with("cyclicLengthFromStepIndex") => {
                let input = f(f_[0]);
                let want = f_[1].parse::<i32>().unwrap();
                assert_eq!(cyclic_length_from_step_index(input), want, "{at}: length of {input}");
            }

            other => panic!("{at}: unknown section {other:?}"),
        }
        checked += 1;
    });

    assert_eq!(rows, checked);
    assert!(checked > 160, "only {checked} rows checked; fixture looks truncated");
}
