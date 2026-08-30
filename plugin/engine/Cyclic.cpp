#include "Cyclic.h"

#include "Num.h"

namespace mclassic
{

namespace
{

int clampLevel (double value) noexcept
{
    return (int) clampTo (jsRound (value), 0.0, 4.0);
}

} // namespace

int cyclicLengthFromStepIndex (double stepIndex) noexcept
{
    return (int) clampTo (jsRound (stepIndex) + 1.0, 1.0, 16.0);
}

CyclicLevelRange normalizeCyclicStep (CyclicStep step) noexcept
{
    const auto a = clampLevel (step.min);
    const auto b = clampLevel (step.max);

    return { a < b ? a : b, a > b ? a : b };
}

int pickCyclicLevel (CyclicStep step, Random& rng) noexcept
{
    const auto range = normalizeCyclicStep (step);

    if (range.min == range.max)
        return range.min;

    return range.min + (int) rng.intBelow ((uint32_t) (range.max - range.min + 1));
}

} // namespace mclassic
