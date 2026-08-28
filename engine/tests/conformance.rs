//! The Rust engine is checked against the TypeScript one, not against taste.
//!
//! `src/engine/__goldens__/rng.txt` is emitted by `scripts/goldens.ts` running
//! the real TypeScript implementation. Every value here therefore came out of
//! the engine this port is replacing. A single differing bit fails the build.

use std::fs;
use std::path::PathBuf;

use mclassic_engine::{BrownianWalk, Rng};

fn fixture(name: &str) -> String {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("..")
        .join("src")
        .join("engine")
        .join("__goldens__")
        .join(name);

    fs::read_to_string(&path)
        .unwrap_or_else(|e| panic!("cannot read {}: {e}\nRun: npx vite-node scripts/goldens.ts", path.display()))
}

/// Which block of the fixture the parser is currently inside.
#[derive(PartialEq, Clone, Copy)]
enum Section {
    None,
    RawDraws,
    Int,
    PickAvoiding,
    Walk,
}

fn section_for(header: &str) -> Section {
    if header.contains("raw u32 draws") {
        Section::RawDraws
    } else if header.contains("Rng.int") {
        Section::Int
    } else if header.contains("pickIndexAvoiding") {
        Section::PickAvoiding
    } else if header.contains("BrownianWalk") {
        Section::Walk
    } else {
        Section::None
    }
}

#[test]
fn rng_reproduces_the_typescript_engine_exactly() {
    let text = fixture("rng.txt");

    let mut section = Section::None;
    let mut rng = Rng::new(0);
    let mut walk = BrownianWalk::new(Rng::new(0));
    let mut checked = 0usize;

    for (lineno, line) in text.lines().enumerate() {
        let line = line.trim();

        if line.is_empty() {
            continue;
        }
        if let Some(header) = line.strip_prefix('#') {
            section = section_for(header);
            continue;
        }

        let f: Vec<&str> = line.split(',').collect();
        let at = format!("rng.txt:{}", lineno + 1);

        match section {
            Section::RawDraws => {
                let (seed, index, want) = (
                    f[0].parse::<u32>().unwrap(),
                    f[1].parse::<usize>().unwrap(),
                    f[2].parse::<u32>().unwrap(),
                );
                if index == 0 {
                    rng = Rng::new(seed);
                }
                assert_eq!(rng.next_u32(), want, "{at}: seed {seed} draw {index}");
                checked += 1;
            }

            Section::Int => {
                let (seed, n, index, want) = (
                    f[0].parse::<u32>().unwrap(),
                    f[1].parse::<u32>().unwrap(),
                    f[2].parse::<usize>().unwrap(),
                    f[3].parse::<u32>().unwrap(),
                );
                if index == 0 {
                    rng = Rng::new(seed);
                }
                assert_eq!(rng.int(n), want, "{at}: seed {seed} int({n}) #{index}");
                checked += 1;
            }

            Section::PickAvoiding => {
                let (seed, n, avoid, index, want) = (
                    f[0].parse::<u32>().unwrap(),
                    f[1].parse::<u32>().unwrap(),
                    f[2].parse::<u32>().unwrap(),
                    f[3].parse::<usize>().unwrap(),
                    f[4].parse::<u32>().unwrap(),
                );
                if index == 0 {
                    rng = Rng::new(seed);
                }
                assert_eq!(
                    rng.pick_index_avoiding(n, avoid),
                    want,
                    "{at}: seed {seed} pick_index_avoiding({n}, {avoid}) #{index}"
                );
                checked += 1;
            }

            Section::Walk => {
                let (seed, index, want) = (
                    f[0].parse::<u32>().unwrap(),
                    f[1].parse::<usize>().unwrap(),
                    u64::from_str_radix(f[2], 16).unwrap(),
                );
                if index == 0 {
                    walk = BrownianWalk::new(Rng::new(seed));
                }
                let got = walk.next().to_bits();
                assert_eq!(
                    got, want,
                    "{at}: seed {seed} walk #{index}\n  got  {:016x} ({})\n  want {:016x} ({})",
                    got,
                    f64::from_bits(got),
                    want,
                    f64::from_bits(want)
                );
                checked += 1;
            }

            Section::None => panic!("{at}: value outside any known section"),
        }
    }

    // Guards against a fixture that silently stops being read.
    assert!(checked > 700, "only {checked} values checked; fixture looks truncated");
}
