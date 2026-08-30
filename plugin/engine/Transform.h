#pragma once

#include "Random.h"

#include <cstdint>
#include <vector>

namespace idm
{

enum class NoteOrder { original, reverse, random, randomWalk, brownian };

bool noteOrderFromName (const char* name, NoteOrder& out) noexcept;

/** Which of the three Note Order lists a step was drawn from. */
enum class NoteSource { original, cyclic, utterly };

const char* nameOf (NoteSource source) noexcept;

/** Per-Voice traversal bookkeeping, carried from step to step. */
struct NoteOrderCursor
{
    /** Logical position counter. */
    long long pos = 0;
    /** Last index read, for no-repeat and the walk. */
    int last = -1;
    /** 0..1 Brownian walk position. */
    double bval = 0.5;
};

struct NoteOrderMix
{
    int original = 0;
    int cyclic = 0;
    int utterly = 0;
};

struct VelocityRange
{
    double low = 0.0;
    double high = 127.0;
};

/** Seconds per step for a Voice's time base. A whole note is `4 * (60 / tempo)`
    seconds; a step is `numerator/denominator` of a whole note. */
double stepDurationSeconds (double tempo, double numerator, double denominator) noexcept;

/** Density gate: whether this step actually sounds. */
bool gate (double density, Random& rng) noexcept;

/** Clamp and order the two MIDI endpoints of the Velocity Range. */
void normalizeVelocityRange (VelocityRange range, int& low, int& high) noexcept;

/** Accent 0 is a rest; levels 1-4 divide the selected range evenly. */
int velocityForAccent (VelocityRange range, double rawLevel) noexcept;

/** The step index a Voice reads next, and the advanced cursor. */
int nextStepIndex (NoteOrder order, NoteOrderCursor cursor, int length,
                   Random& rng, NoteOrderCursor& advanced);

/** The stored, repeating permutation behind Cyclic Random mode. */
std::vector<int> makeCyclicOrder (int length, uint32_t seed);

/** Convert the two movable edge controls into M's three percentages. */
NoteOrderMix noteOrderMixFromEdges (double originalValue, double utterlyValue) noexcept;

/** Pick among the three note-order sources by percentage. Original and Cyclic
    advance repeatably; Utterly chooses afresh on every step. */
int nextMixedStepIndex (NoteOrderMix mix, NoteOrderCursor cursor, int length,
                        Random& rng, NoteSource& source, NoteOrderCursor& advanced);

} // namespace idm
