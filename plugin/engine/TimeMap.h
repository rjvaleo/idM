#pragma once

#include <vector>

namespace idm
{

/** A breakpoint inside the unit square. */
struct TimeMapPoint
{
    double x = 0.0;
    double y = 0.0;
};

/** The Time Distortion Map — M's re-mapping of clock time onto real time.

    Both axes are normalised over one cycle and the corners (0,0) and (1,1) are
    implicit, which is what makes the map time-preserving: however hard the
    middle is bent, a cycle still takes exactly as long as it would have.

    The planner needs the inverse direction — it knows which tick an event falls
    on and must ask when that sounds — so `clockToReal` is what matters
    downstream.
*/
struct TimeMap
{
    /** Breakpoints inside the unit square, ordered by x. Corners are implicit. */
    std::vector<TimeMapPoint> points;
    /** How many units of `denominator` one cycle covers. */
    double length = 1.0;
    /** The unit as a note division: 1 = whole, 4 = quarter, 8 = eighth. */
    double denominator = 4.0;

    /** True when the map leaves time alone. An empty map is neutral, matching
        `Array.prototype.every` on an empty array. */
    bool isNeutral() const;

    /** Put the breakpoints in order and force both axes to run forwards.
        Neither real time nor clock time can go backwards, so a point that would
        double back is pinned to its predecessor rather than dropped. */
    TimeMap normalized() const;

    /** The drawable polyline: the breakpoints between the two fixed corners. */
    std::vector<TimeMapPoint> polyline() const;

    /** Real Time -> Clock Time. This is the curve as drawn. */
    double realToClock (double realPhase) const;

    /** Clock Time -> Real Time: the direction the planner needs. */
    double clockToReal (double clockPhase) const;

    /** How long one cycle lasts, in seconds. */
    double seconds (double tempo) const;

    /** Elapsed clock time into the real time it should sound at. The map
        repeats for as long as the Voice plays. */
    double distortClockSeconds (double tempo, double clockSec) const;
};

} // namespace idm
