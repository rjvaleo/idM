#pragma once

#include "Random.h"

namespace mclassic
{

/** A cyclic step is either a single level or an inclusive range of them.

    Documents carry both: ranges are the newer form, and a bare number is what
    older projects stored.
*/
struct CyclicStep
{
    double min = 0.0;
    double max = 0.0;

    static CyclicStep level (double v) noexcept { return { v, v }; }
    static CyclicStep range (double lo, double hi) noexcept { return { lo, hi }; }
};

struct CyclicLevelRange
{
    int min = 0;
    int max = 0;
};

/** A zero-based marker column as M's inclusive 1..16 loop length. */
int cyclicLengthFromStepIndex (double stepIndex) noexcept;

/** Accept a bare level as well as a range, and canonicalise to an ordered one. */
CyclicLevelRange normalizeCyclicStep (CyclicStep step) noexcept;

/** Pick uniformly and inclusively.

    A point range consumes **no** randomness. That is not an optimisation: it is
    what keeps projects seeded before ranges existed sounding as they did. Spend
    a draw here and every subsequent value in the performance shifts.
*/
int pickCyclicLevel (CyclicStep step, Random& rng) noexcept;

} // namespace mclassic
