//! Ported from `src/engine/timemap.ts`, and required to agree with it exactly.
//!
//! The Time Distortion Map re-maps clock time onto real time. Both axes are
//! normalised over one cycle and the corners (0,0) and (1,1) are implicit, which
//! is what makes the map time-preserving: however hard the middle is bent, a
//! cycle still takes exactly as long as it would have.
//!
//! The planner needs the inverse direction — it knows which tick an event falls
//! on and must ask when that actually sounds — so `clock_to_real` is the one
//! that matters downstream.

/// A breakpoint inside the unit square.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TimeMapPoint {
    pub x: f64,
    pub y: f64,
}

#[derive(Clone, Debug, PartialEq)]
pub struct TimeMap {
    /// Breakpoints inside the unit square, ordered by x. Corners are implicit.
    pub points: Vec<TimeMapPoint>,
    /// How many units of `denominator` one cycle of the map covers.
    pub length: f64,
    /// The unit as a note division: 1 = whole, 4 = quarter, 8 = eighth.
    pub denominator: f64,
}

impl Default for TimeMap {
    /// The "ineffectual" map: a straight diagonal over one quarter note.
    fn default() -> Self {
        Self { points: Vec::new(), length: 1.0, denominator: 4.0 }
    }
}

fn clamp01(v: f64) -> f64 {
    if v < 0.0 {
        0.0
    } else if v > 1.0 {
        1.0
    } else {
        v
    }
}

/// Which axis `interpolate` is reading.
#[derive(Clone, Copy, PartialEq)]
enum Axis {
    X,
    Y,
}

impl TimeMap {
    pub fn neutral() -> Self {
        Self::default()
    }

    /// True when the map leaves time alone, so callers can skip the arithmetic.
    /// An empty map is neutral, matching `Array.prototype.every` on `[]`.
    pub fn is_neutral(&self) -> bool {
        self.points.iter().all(|p| (p.x - p.y).abs() < 1e-9)
    }

    /// Put the breakpoints in order and force both axes to run forwards.
    /// Neither real time nor clock time can go backwards, so a point that would
    /// double back is pinned to its predecessor rather than dropped.
    pub fn normalized(&self) -> Self {
        let mut points: Vec<TimeMapPoint> = self
            .points
            .iter()
            .map(|p| TimeMapPoint { x: clamp01(p.x), y: clamp01(p.y) })
            .collect();

        // Stable, like `Array.prototype.sort` since ES2019: two points sharing
        // an x must keep the order they were written in.
        points.sort_by(|a, b| a.x.partial_cmp(&b.x).expect("time map points must not be NaN"));

        let mut prev_x = 0.0_f64;
        let mut prev_y = 0.0_f64;

        for p in &mut points {
            p.x = p.x.max(prev_x);
            p.y = p.y.max(prev_y);
            prev_x = p.x;
            prev_y = p.y;
        }

        Self { points, length: self.length, denominator: self.denominator }
    }

    /// The drawable polyline: the breakpoints between the two fixed corners.
    pub fn polyline(&self) -> Vec<TimeMapPoint> {
        let mut out = Vec::with_capacity(self.points.len() + 2);
        out.push(TimeMapPoint { x: 0.0, y: 0.0 });
        out.extend_from_slice(&self.points);
        out.push(TimeMapPoint { x: 1.0, y: 1.0 });
        out
    }

    /// Real Time -> Clock Time. This is the curve as drawn.
    pub fn real_to_clock(&self, real_phase: f64) -> f64 {
        interpolate(&self.normalized().polyline(), real_phase, Axis::X)
    }

    /// Clock Time -> Real Time: the inverse, and the direction the planner needs.
    pub fn clock_to_real(&self, clock_phase: f64) -> f64 {
        interpolate(&self.normalized().polyline(), clock_phase, Axis::Y)
    }

    /// How long one cycle lasts, in seconds. Length is a count times a note
    /// value, so one whole note and four quarter notes describe the same span.
    pub fn seconds(&self, tempo: f64) -> f64 {
        if tempo <= 0.0 || self.length <= 0.0 || self.denominator <= 0.0 {
            return 0.0;
        }

        let quarter_sec = 60.0 / tempo;
        self.length * quarter_sec * (4.0 / self.denominator)
    }

    /// Elapsed clock time into the real time it should sound at. The map
    /// repeats for as long as the Voice plays.
    pub fn distort_clock_seconds(&self, tempo: f64, clock_sec: f64) -> f64 {
        let span = self.seconds(tempo);

        if span <= 0.0 || self.is_neutral() || clock_sec < 0.0 {
            return clock_sec;
        }

        let cycle = (clock_sec / span).floor();
        let phase = (clock_sec - cycle * span) / span;

        (cycle + self.clock_to_real(phase)) * span
    }
}

/// Interpolate a monotonic polyline, reading `from` and returning the other axis.
fn interpolate(polyline: &[TimeMapPoint], value: f64, from: Axis) -> f64 {
    let read = |p: &TimeMapPoint| if from == Axis::X { p.x } else { p.y };
    let write = |p: &TimeMapPoint| if from == Axis::X { p.y } else { p.x };

    let v = clamp01(value);

    // Walk to the first segment that reaches `v`. Taking the first rather than
    // the last matters where the graph jumps: the answer is the value just
    // before the jump, not just after.
    let mut i = 0usize;
    while i + 2 < polyline.len() && v > read(&polyline[i + 1]) {
        i += 1;
    }

    let a = &polyline[i];
    let b = &polyline[i + 1];
    let span = read(b) - read(a);

    // A zero-width span is a breakpoint sitting on the axis being read, so that
    // axis never advances across the segment and there is nothing to
    // interpolate - the value at the near end is the answer.
    if span == 0.0 {
        return write(a);
    }

    write(a) + ((v - read(a)) / span) * (write(b) - write(a))
}
