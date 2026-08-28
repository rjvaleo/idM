//! The Time Distortion Map, checked against the TypeScript engine.
//!
//! The fixture carries the map definitions as well as the results, so the two
//! languages cannot disagree about what was measured. Every float is compared
//! as a bit pattern: "close enough" is not the standard here, because the
//! planner's tick arithmetic runs on top of these values.

use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;

use mclassic_engine::{TimeMap, TimeMapPoint};

fn fixture(name: &str) -> String {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("..")
        .join("src")
        .join("engine")
        .join("__goldens__")
        .join(name);

    fs::read_to_string(&path).unwrap_or_else(|e| {
        panic!("cannot read {}: {e}\nRun: npm run goldens", path.display())
    })
}

fn f(hex: &str) -> f64 {
    f64::from_bits(u64::from_str_radix(hex, 16).expect("bad float bits"))
}

#[derive(PartialEq, Clone, Copy)]
enum Section {
    None,
    Maps,
    Normalize,
    IsNeutral,
    RealToClock,
    ClockToReal,
    Seconds,
    Distort,
}

fn section_for(header: &str) -> Section {
    match () {
        _ if header.contains("maps:") => Section::Maps,
        _ if header.contains("normalizeTimeMap") => Section::Normalize,
        _ if header.contains("isNeutralTimeMap") => Section::IsNeutral,
        _ if header.contains("realToClock") => Section::RealToClock,
        _ if header.contains("clockToReal") => Section::ClockToReal,
        _ if header.contains("timeMapSeconds") => Section::Seconds,
        _ if header.contains("distortClockSeconds") => Section::Distort,
        _ => Section::None,
    }
}

/// `id|length|denominator|x:y;x:y;...`
fn parse_map(line: &str) -> (String, TimeMap) {
    let parts: Vec<&str> = line.split('|').collect();
    let points = if parts[3].is_empty() {
        Vec::new()
    } else {
        parts[3]
            .split(';')
            .map(|pair| {
                let (x, y) = pair.split_once(':').expect("point must be x:y");
                TimeMapPoint { x: x.parse().unwrap(), y: y.parse().unwrap() }
            })
            .collect()
    };

    (
        parts[0].to_string(),
        TimeMap {
            points,
            length: parts[1].parse().unwrap(),
            denominator: parts[2].parse().unwrap(),
        },
    )
}

#[test]
fn timemap_reproduces_the_typescript_engine_exactly() {
    let text = fixture("timemap.txt");
    let mut maps: HashMap<String, TimeMap> = HashMap::new();
    let mut section = Section::None;
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

        let at = format!("timemap.txt:{}", lineno + 1);

        if section == Section::Maps {
            let (id, map) = parse_map(line);
            maps.insert(id, map);
            continue;
        }

        let f_: Vec<&str> = line.split(',').collect();
        let map = maps.get(f_[0]).unwrap_or_else(|| panic!("{at}: unknown map {}", f_[0]));

        match section {
            Section::Normalize => {
                let index: usize = f_[1].parse().unwrap();
                let got = map.normalized();
                let p = got.points.get(index).unwrap_or_else(|| {
                    panic!("{at}: {} normalised to {} points, wanted index {index}", f_[0], got.points.len())
                });
                assert_eq!(p.x.to_bits(), f(f_[2]).to_bits(), "{at}: {} point {index} x", f_[0]);
                assert_eq!(p.y.to_bits(), f(f_[3]).to_bits(), "{at}: {} point {index} y", f_[0]);
                checked += 2;
            }

            Section::IsNeutral => {
                let want = f_[1] == "1";
                assert_eq!(map.is_neutral(), want, "{at}: {} is_neutral", f_[0]);
                checked += 1;
            }

            Section::RealToClock | Section::ClockToReal => {
                let input = f(f_[1]);
                let want = f(f_[2]);
                let got = if section == Section::RealToClock {
                    map.real_to_clock(input)
                } else {
                    map.clock_to_real(input)
                };
                assert_eq!(
                    got.to_bits(), want.to_bits(),
                    "{at}: {} at phase {input}\n  got  {got}\n  want {want}", f_[0]
                );
                checked += 1;
            }

            Section::Seconds => {
                let tempo = f(f_[1]);
                let want = f(f_[2]);
                assert_eq!(
                    map.seconds(tempo).to_bits(), want.to_bits(),
                    "{at}: {} seconds at tempo {tempo}", f_[0]
                );
                checked += 1;
            }

            Section::Distort => {
                let (tempo, clock, want) = (f(f_[1]), f(f_[2]), f(f_[3]));
                let got = map.distort_clock_seconds(tempo, clock);
                assert_eq!(
                    got.to_bits(), want.to_bits(),
                    "{at}: {} distort(tempo {tempo}, clock {clock})\n  got  {got}\n  want {want}", f_[0]
                );
                checked += 1;
            }

            Section::Maps | Section::None => panic!("{at}: value outside any known section"),
        }
    }

    assert_eq!(maps.len(), 12, "expected 12 maps in the fixture");
    assert!(checked > 700, "only {checked} values checked; fixture looks truncated");
}
