#include "StepSource.h"

#include <cmath>

namespace mclassic
{

constexpr int StepSource::scale[];

void StepSource::collect (double ppqStart, double ppqEnd, std::vector<TimedMidi>& out) const
{
    if (! (ppqEnd > ppqStart))
        return;

    // The first grid position at or after the start. `ceil` rather than a
    // running counter, so a locate lands on the right step without replaying
    // everything between.
    auto step = std::ceil (ppqStart / stepPpq);

    for (; step * stepPpq < ppqEnd; step += 1.0)
    {
        const auto atPpq = step * stepPpq;

        if (atPpq < ppqStart)
            continue;

        const auto index = (int) (((long long) step % scaleSize + scaleSize) % scaleSize);

        out.push_back ({ atPpq, true, 1, scale[index], 100 });
        out.push_back ({ atPpq + stepPpq * gateFraction, false, 1, scale[index], 0 });
    }
}

} // namespace mclassic
