#include "TimeMap.h"

#include "Num.h"

#include <algorithm>
#include <cmath>

namespace mclassic
{

namespace
{

enum class Axis { x, y };

double read (const TimeMapPoint& p, Axis from) noexcept
{
    return from == Axis::x ? p.x : p.y;
}

double write (const TimeMapPoint& p, Axis from) noexcept
{
    return from == Axis::x ? p.y : p.x;
}

/** Interpolate a monotonic polyline, reading `from` and returning the other axis. */
double interpolate (const std::vector<TimeMapPoint>& polyline, double value, Axis from)
{
    const auto v = clampTo (value, 0.0, 1.0);

    // Walk to the first segment that reaches `v`. Taking the first rather than
    // the last matters where the graph jumps: the answer is the value just
    // before the jump, not just after.
    size_t i = 0;

    while (i + 2 < polyline.size() && v > read (polyline[i + 1], from))
        ++i;

    const auto& a = polyline[i];
    const auto& b = polyline[i + 1];
    const auto span = read (b, from) - read (a, from);

    // A zero-width span is a breakpoint sitting on the axis being read, so that
    // axis never advances across the segment and there is nothing to
    // interpolate — the value at the near end is the answer.
    if (span == 0.0)
        return write (a, from);

    return write (a, from) + ((v - read (a, from)) / span) * (write (b, from) - write (a, from));
}

} // namespace

bool TimeMap::isNeutral() const
{
    for (const auto& p : points)
        if (std::abs (p.x - p.y) >= 1e-9)
            return false;

    return true;
}

TimeMap TimeMap::normalized() const
{
    TimeMap out;
    out.length = length;
    out.denominator = denominator;
    out.points.reserve (points.size());

    for (const auto& p : points)
        out.points.push_back ({ clampTo (p.x, 0.0, 1.0), clampTo (p.y, 0.0, 1.0) });

    // Stable, like `Array.prototype.sort` since ES2019: two points sharing an x
    // must keep the order they were written in.
    std::stable_sort (out.points.begin(), out.points.end(),
                      [] (const TimeMapPoint& a, const TimeMapPoint& b) { return a.x < b.x; });

    double prevX = 0.0, prevY = 0.0;

    for (auto& p : out.points)
    {
        p.x = std::max (p.x, prevX);
        p.y = std::max (p.y, prevY);
        prevX = p.x;
        prevY = p.y;
    }

    return out;
}

std::vector<TimeMapPoint> TimeMap::polyline() const
{
    std::vector<TimeMapPoint> out;
    out.reserve (points.size() + 2);
    out.push_back ({ 0.0, 0.0 });
    out.insert (out.end(), points.begin(), points.end());
    out.push_back ({ 1.0, 1.0 });
    return out;
}

double TimeMap::realToClock (double realPhase) const
{
    return interpolate (normalized().polyline(), realPhase, Axis::x);
}

double TimeMap::clockToReal (double clockPhase) const
{
    return interpolate (normalized().polyline(), clockPhase, Axis::y);
}

double TimeMap::seconds (double tempo) const
{
    if (tempo <= 0.0 || length <= 0.0 || denominator <= 0.0)
        return 0.0;

    const auto quarterSec = 60.0 / tempo;
    return length * quarterSec * (4.0 / denominator);
}

double TimeMap::distortClockSeconds (double tempo, double clockSec) const
{
    const auto span = seconds (tempo);

    if (span <= 0.0 || isNeutral() || clockSec < 0.0)
        return clockSec;

    const auto cycle = std::floor (clockSec / span);
    const auto phase = (clockSec - cycle * span) / span;

    return (cycle + clockToReal (phase)) * span;
}

} // namespace mclassic
