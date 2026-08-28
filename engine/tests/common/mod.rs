//! Shared plumbing for the conformance tests.
#![allow(dead_code)]

use std::fs;
use std::path::PathBuf;

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
