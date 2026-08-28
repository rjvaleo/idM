//! M Classic's generative engine.
//!
//! The port is gated on conformance, not on review: every stage must reproduce
//! the fixtures in `src/engine/__goldens__/` that the TypeScript engine emits.
//! See `tests/conformance.rs`.

pub mod rng;

pub use rng::{BrownianWalk, Rng};
