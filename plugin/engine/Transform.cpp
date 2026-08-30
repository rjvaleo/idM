#include "Transform.h"

#include "Music.h"
#include "Num.h"

#include <cmath>
#include <cstring>

namespace mclassic
{

bool noteOrderFromName (const char* name, NoteOrder& out) noexcept
{
    if (std::strcmp (name, "original") == 0)    { out = NoteOrder::original;   return true; }
    if (std::strcmp (name, "reverse") == 0)     { out = NoteOrder::reverse;    return true; }
    if (std::strcmp (name, "random") == 0)      { out = NoteOrder::random;     return true; }
    if (std::strcmp (name, "random-walk") == 0) { out = NoteOrder::randomWalk; return true; }
    if (std::strcmp (name, "brownian") == 0)    { out = NoteOrder::brownian;   return true; }
    return false;
}

const char* nameOf (NoteSource source) noexcept
{
    switch (source)
    {
        case NoteSource::original: return "original";
        case NoteSource::cyclic:   return "cyclic";
        case NoteSource::utterly:  return "utterly";
    }

    return "original";
}

double stepDurationSeconds (double tempo, double numerator, double denominator) noexcept
{
    const auto wholeNote = 4.0 * (60.0 / tempo);
    return wholeNote * (numerator / denominator);
}

bool gate (double density, Random& rng) noexcept
{
    return rng.chance (density);
}

void normalizeVelocityRange (VelocityRange range, int& low, int& high) noexcept
{
    const auto a = clampMidi (range.low);
    const auto b = clampMidi (range.high);

    low = a < b ? a : b;
    high = a > b ? a : b;
}

int velocityForAccent (VelocityRange range, double rawLevel) noexcept
{
    const auto level = (int) clampTo (jsRound (rawLevel), 0.0, 4.0);

    if (level == 0)
        return 0;

    int low = 0, high = 0;
    normalizeVelocityRange (range, low, high);

    return (int) jsRound ((double) low + (double) (high - low) * (double) (level - 1) / 3.0);
}

int nextStepIndex (NoteOrder order, NoteOrderCursor cursor, int length,
                   Random& rng, NoteOrderCursor& advanced)
{
    auto bval = cursor.bval;
    int index = 0;

    switch (order)
    {
        case NoteOrder::original:
            index = (int) (cursor.pos % (long long) length);
            break;

        case NoteOrder::reverse:
            index = length - 1 - (int) (cursor.pos % (long long) length);
            break;

        case NoteOrder::random:
            index = (int) rng.pickIndexAvoiding ((uint32_t) length, (uint32_t) cursor.last);
            break;

        case NoteOrder::brownian:
        {
            // 1/f-ish smooth wander: the walk position maps onto the step range,
            // so the read head drifts rather than jumping. The free function
            // rather than a BrownianWalk, which would copy the generator and
            // desynchronise every draw after it.
            bval = brownianStep (cursor.bval, 0.18, rng);
            const auto scaled = (int) std::floor (bval * (double) length);
            index = length - 1 < scaled ? length - 1 : scaled;
            break;
        }

        case NoteOrder::randomWalk:
        {
            const auto base = cursor.last < 0 ? 0 : cursor.last;
            const auto dir = rng.chance (0.5) ? 1 : -1;
            index = floorMod (base + dir, length);
            break;
        }
    }

    advanced = { cursor.pos + 1, index, bval };
    return index;
}

std::vector<int> makeCyclicOrder (int length, uint32_t seed)
{
    std::vector<int> order ((size_t) (length < 0 ? 0 : length));

    for (size_t i = 0; i < order.size(); ++i)
        order[i] = (int) i;

    Random rng { seed };

    for (auto i = order.size(); i > 1;)
    {
        --i;
        const auto j = (size_t) rng.intBelow ((uint32_t) (i + 1));
        std::swap (order[i], order[j]);
    }

    return order;
}

NoteOrderMix noteOrderMixFromEdges (double originalValue, double utterlyValue) noexcept
{
    const auto original = (int) clampTo (jsRound (originalValue), 0.0, 100.0);
    const auto utterly = (int) clampTo (jsRound (utterlyValue), 0.0, (double) (100 - original));

    return { original, 100 - original - utterly, utterly };
}

int nextMixedStepIndex (NoteOrderMix mix, NoteOrderCursor cursor, int length,
                        Random& rng, NoteSource& source, NoteOrderCursor& advanced)
{
    const auto roll = rng.next() * 100.0;
    int index = 0;

    if (roll < (double) mix.original)
    {
        index = (int) (cursor.pos % (long long) length);
        source = NoteSource::original;
    }
    else if (roll < (double) (mix.original + mix.cyclic))
    {
        index = (int) (cursor.pos % (long long) length);
        source = NoteSource::cyclic;
    }
    else
    {
        index = (int) rng.pickIndexAvoiding ((uint32_t) length, (uint32_t) cursor.last);
        source = NoteSource::utterly;
    }

    advanced = { cursor.pos + 1, index, cursor.bval };
    return index;
}

} // namespace mclassic
