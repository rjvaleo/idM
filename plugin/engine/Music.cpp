#include "Music.h"

#include "Num.h"

#include <array>
#include <cstdlib>

namespace idm
{

namespace
{

constexpr int chromaticDegrees[]      = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
constexpr int majorDegrees[]          = { 0, 2, 4, 5, 7, 9, 11 };
constexpr int minorDegrees[]          = { 0, 2, 3, 5, 7, 8, 10 };
constexpr int dorianDegrees[]         = { 0, 2, 3, 5, 7, 9, 10 };
constexpr int mixolydianDegrees[]     = { 0, 2, 4, 5, 7, 9, 10 };
constexpr int lydianDegrees[]         = { 0, 2, 4, 6, 7, 9, 11 };
constexpr int phrygianDegrees[]       = { 0, 1, 3, 5, 7, 8, 10 };
constexpr int harmonicMinorDegrees[]  = { 0, 2, 3, 5, 7, 8, 11 };
constexpr int majorPentatonicDegrees[]= { 0, 2, 4, 7, 9 };
constexpr int minorPentatonicDegrees[]= { 0, 3, 5, 7, 10 };
constexpr int bluesDegrees[]          = { 0, 3, 5, 6, 7, 10 };

constexpr const char* noteNames[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

/** The signed pitch-class distance from `pc` to `targetPc`, folded into
    [-6, 5] so ties resolve downward. */
int signedDelta (int targetPc, int pc) noexcept
{
    return floorMod (targetPc - pc + 6, 12) - 6;
}

/** The degree nearest `rel`, measured around the twelve-tone circle. */
int nearestDegree (const int* degrees, int count, int rel) noexcept
{
    auto best = degrees[0];
    auto bestDist = 99;

    for (int i = 0; i < count; ++i)
    {
        const auto raw = std::abs (rel - degrees[i]);
        const auto dist = raw < 12 - raw ? raw : 12 - raw;

        if (dist < bestDist)
        {
            bestDist = dist;
            best = degrees[i];
        }
    }

    return best;
}

} // namespace

int floorMod (int value, int modulus) noexcept
{
    const auto r = value % modulus;
    return r < 0 ? r + modulus : r;
}

int floorDiv (int value, int divisor) noexcept
{
    auto q = value / divisor;

    if ((value % divisor != 0) && ((value < 0) != (divisor < 0)))
        --q;

    return q;
}

Degrees degreesOf (Scale scale) noexcept
{
    switch (scale)
    {
        case Scale::chromatic:       return { chromaticDegrees, 12 };
        case Scale::major:           return { majorDegrees, 7 };
        case Scale::minor:           return { minorDegrees, 7 };
        case Scale::dorian:          return { dorianDegrees, 7 };
        case Scale::mixolydian:      return { mixolydianDegrees, 7 };
        case Scale::lydian:          return { lydianDegrees, 7 };
        case Scale::phrygian:        return { phrygianDegrees, 7 };
        case Scale::harmonicMinor:   return { harmonicMinorDegrees, 7 };
        case Scale::majorPentatonic: return { majorPentatonicDegrees, 5 };
        case Scale::minorPentatonic: return { minorPentatonicDegrees, 5 };
        case Scale::blues:           return { bluesDegrees, 6 };
    }

    return { chromaticDegrees, 12 };
}

const char* nameOf (Scale scale) noexcept
{
    switch (scale)
    {
        case Scale::chromatic:       return "chromatic";
        case Scale::major:           return "major";
        case Scale::minor:           return "minor";
        case Scale::dorian:          return "dorian";
        case Scale::mixolydian:      return "mixolydian";
        case Scale::lydian:          return "lydian";
        case Scale::phrygian:        return "phrygian";
        case Scale::harmonicMinor:   return "harmonicMinor";
        case Scale::majorPentatonic: return "majorPentatonic";
        case Scale::minorPentatonic: return "minorPentatonic";
        case Scale::blues:           return "blues";
    }

    return "chromatic";
}

bool scaleFromName (const std::string& name, Scale& out) noexcept
{
    static const std::array<Scale, 11> all {
        Scale::chromatic, Scale::major, Scale::minor, Scale::dorian,
        Scale::mixolydian, Scale::lydian, Scale::phrygian, Scale::harmonicMinor,
        Scale::majorPentatonic, Scale::minorPentatonic, Scale::blues
    };

    for (auto scale : all)
    {
        if (name == nameOf (scale))
        {
            out = scale;
            return true;
        }
    }

    return false;
}

std::string midiToName (int note)
{
    const auto octave = floorDiv (note, 12) - 1;
    return std::string (noteNames[floorMod (note, 12)]) + std::to_string (octave);
}

int snapToScale (int note, int root, Scale scale) noexcept
{
    const auto degrees = degreesOf (scale);

    if (degrees.count == 12)
        return note; // chromatic: no change

    const auto pc = floorMod (note, 12);
    const auto rootPc = floorMod (root, 12);
    const auto rel = floorMod (pc - rootPc, 12);

    const auto best = nearestDegree (degrees.values, degrees.count, rel);
    return note + signedDelta ((rootPc + best) % 12, pc);
}

int snapToChord (int note, int root, Scale scale) noexcept
{
    const auto degrees = degreesOf (scale);

    int chord[3];

    if (degrees.count == 12)
    {
        chord[0] = 0; chord[1] = 4; chord[2] = 7;
    }
    else
    {
        chord[0] = degrees[0]; chord[1] = degrees[2]; chord[2] = degrees[4];
    }

    const auto pc = floorMod (note, 12);
    const auto rootPc = floorMod (root, 12);
    const auto rel = floorMod (pc - rootPc, 12);

    const auto best = nearestDegree (chord, 3, rel);
    return note + signedDelta ((rootPc + best) % 12, pc);
}

int diatonicTranspose (int note, int root, Scale scale, int steps) noexcept
{
    const auto degrees = degreesOf (scale);

    if (degrees.count == 12)
        return note + steps;

    const auto rootPc = floorMod (root, 12);
    const auto snapped = snapToScale (note, root, scale);
    const auto relPc = floorMod (snapped - rootPc, 12);

    // `indexOf` yields -1 when absent. Mirrored rather than tidied away: the
    // arithmetic below is defined for it and the TypeScript relies on that.
    auto di = -1;

    for (int i = 0; i < degrees.count; ++i)
        if (degrees[i] == relPc)
        {
            di = i;
            break;
        }

    const auto len = degrees.count;
    const auto total = di + steps;
    const auto octave = floorDiv (total, len);
    const auto wrapped = floorMod (total, len);

    return snapped - relPc + degrees[wrapped] + 12 * octave;
}

int clampMidi (double note) noexcept
{
    return (int) clampTo (jsRound (note), 0.0, 127.0);
}

} // namespace idm
