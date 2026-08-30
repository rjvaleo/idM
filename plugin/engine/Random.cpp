#include "Random.h"

#include "Num.h"

namespace mclassic
{

double brownianStep (double value, double stepSize, Random& rng) noexcept
{
    const auto delta = (rng.next() * 2.0 - 1.0) * stepSize;
    auto v = value + delta;

    if (v > 1.0)
        v = 2.0 - v; // reflect off the top

    if (v < 0.0)
        v = -v; // reflect off the bottom

    return clampTo (v, 0.0, 1.0);
}

} // namespace mclassic
