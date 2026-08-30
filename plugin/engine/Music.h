#pragma once

#include <string>

namespace idm
{

/** Scales, snapping, and diatonic transposition.

    All of the arithmetic here is pitch-class modular. JavaScript's `%` keeps
    the sign of the dividend and `Math.floor` of a quotient is a floor rather
    than a truncation, so every wrap here goes through `floorMod` and every
    quotient through `floorDiv`. A negative note is where a careless port
    breaks, and nowhere else.
*/
enum class Scale
{
    chromatic,
    major,
    minor,
    dorian,
    mixolydian,
    lydian,
    phrygian,
    harmonicMinor,
    majorPentatonic,
    minorPentatonic,
    blues
};

/** Scale degrees as semitone offsets from the root. */
struct Degrees
{
    const int* values = nullptr;
    int count = 0;

    const int* begin() const noexcept { return values; }
    const int* end() const noexcept { return values + count; }
    int operator[] (int i) const noexcept { return values[i]; }
};

Degrees degreesOf (Scale scale) noexcept;

/** The name a scale is stored under in a document. */
const char* nameOf (Scale scale) noexcept;
bool scaleFromName (const std::string& name, Scale& out) noexcept;

/** Euclidean remainder and quotient — JavaScript's, not C++'s. */
int floorMod (int value, int modulus) noexcept;
int floorDiv (int value, int divisor) noexcept;

std::string midiToName (int note);

/** Snap a MIDI note into the given key and scale. Ties resolve downward. */
int snapToScale (int note, int root, Scale scale) noexcept;

/** Snap to the nearest tone of the key's tonic triad. */
int snapToChord (int note, int root, Scale scale) noexcept;

/** Move `steps` scale degrees, folding through the key. In chromatic, steps are
    plain semitones. */
int diatonicTranspose (int note, int root, Scale scale, int steps) noexcept;

int clampMidi (double note) noexcept;

} // namespace idm
