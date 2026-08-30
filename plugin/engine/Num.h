#pragma once

#include <cmath>

namespace idm
{

/** JavaScript's `Math.round`, which rounds a half **toward +Infinity**.

    C++'s `std::round` rounds a half *away from zero*, so the two disagree on
    every negative half: `Math.round(-0.5)` is `-0`, `std::round(-0.5)` is `-1`.
    The engine rounds velocities, levels, MIDI notes and tick positions, and
    reaching for the obvious function would move notes.
*/
inline double jsRound (double v) noexcept
{
    return std::floor (v + 0.5);
}

/** Clamp, in the order the TypeScript applies it. */
inline double clampTo (double v, double low, double high) noexcept
{
    return v < low ? low : (v > high ? high : v);
}

} // namespace idm
