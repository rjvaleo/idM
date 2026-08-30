// The per-step chain, checked against the TypeScript engine.
//
// The cursor sequences matter as much as the values: pos, last and the Brownian
// position are carried step to step, so a port that produces the right first
// index and the wrong cursor diverges on the second.

#include "Harness.h"

#include "../engine/Transform.h"

namespace mclassic::conformance
{
namespace
{

NoteOrderMix mixNamed (const std::string& id)
{
    if (id == "all-original") return { 100, 0, 0 };
    if (id == "all-cyclic")   return { 0, 100, 0 };
    if (id == "all-utterly")  return { 0, 0, 100 };
    if (id == "even")         return { 34, 33, 33 };
    if (id == "classic")      return { 60, 30, 10 };
    if (id == "none")         return { 0, 0, 0 };

    std::printf ("  unknown mix %s\n", id.c_str());
    ++totals().failures;
    return {};
}

} // namespace

void checkTransform()
{
    Random rng { 0 };
    NoteOrderCursor cursor {};

    walk ("transform.txt", [&] (const std::string& header,
                                const std::vector<std::string>& f,
                                const std::string& at)
    {
        if (header.find ("stepDurationSeconds") != std::string::npos)
        {
            expectBits (stepDurationSeconds (fromBits (f[0]), fromBits (f[1]), fromBits (f[2])),
                        fromBits (f[3]), at + " stepDurationSeconds");
        }
        else if (header.find ("normalizeVelocityRange") != std::string::npos)
        {
            int low = 0, high = 0;
            normalizeVelocityRange ({ fromBits (f[0]), fromBits (f[1]) }, low, high);
            expect (low, std::stoi (f[2]), at + " low");
            expect (high, std::stoi (f[3]), at + " high");
        }
        else if (header.find ("velocityForAccent") != std::string::npos)
        {
            expect (velocityForAccent ({ fromBits (f[0]), fromBits (f[1]) }, fromBits (f[2])),
                    std::stoi (f[3]), at + " velocityForAccent");
        }
        else if (header.find ("gate") != std::string::npos)
        {
            if (std::stoi (f[2]) == 0)
                rng = Random { (uint32_t) std::stoul (f[0]) };

            expect (gate (fromBits (f[1]), rng), f[3] == "1", at + " gate #" + f[2]);
        }
        else if (header.find ("nextMixedStepIndex") != std::string::npos)
        {
            if (std::stoi (f[3]) == 0)
            {
                rng = Random { (uint32_t) std::stoul (f[1]) };
                cursor = {};
            }

            NoteSource source {};
            NoteOrderCursor advanced {};
            const auto index = nextMixedStepIndex (mixNamed (f[0]), cursor, std::stoi (f[2]),
                                                   rng, source, advanced);
            cursor = advanced;

            expect (index, std::stoi (f[4]), at + " " + f[0] + " index #" + f[3]);
            expect (std::string (nameOf (source)), f[5], at + " " + f[0] + " source #" + f[3]);
            expect (cursor.pos, std::stoll (f[6]), at + " " + f[0] + " pos #" + f[3]);
            expect (cursor.last, std::stoi (f[7]), at + " " + f[0] + " last #" + f[3]);
        }
        else if (header.find ("nextStepIndex") != std::string::npos)
        {
            if (std::stoi (f[3]) == 0)
            {
                rng = Random { (uint32_t) std::stoul (f[1]) };
                cursor = {};
            }

            NoteOrder order {};

            if (! noteOrderFromName (f[0].c_str(), order))
            {
                std::printf ("  unknown note order %s\n", f[0].c_str());
                ++totals().failures;
                return;
            }

            NoteOrderCursor advanced {};
            const auto index = nextStepIndex (order, cursor, std::stoi (f[2]), rng, advanced);
            cursor = advanced;

            expect (index, std::stoi (f[4]), at + " " + f[0] + " index #" + f[3]);
            expect (cursor.pos, std::stoll (f[5]), at + " " + f[0] + " pos #" + f[3]);
            expect (cursor.last, std::stoi (f[6]), at + " " + f[0] + " last #" + f[3]);
            expectBits (cursor.bval, fromBits (f[7]), at + " " + f[0] + " bval #" + f[3]);
        }
        else if (header.find ("makeCyclicOrder") != std::string::npos)
        {
            const auto got = makeCyclicOrder (std::stoi (f[0]), (uint32_t) std::stoul (f[1]));
            const auto want = split (f[2], ';');

            expect (got.size(), want.size(), at + " order length");

            for (size_t i = 0; i < want.size() && i < got.size(); ++i)
                expect (got[i], std::stoi (want[i]), at + " order[" + std::to_string (i) + "]");
        }
        else if (header.find ("noteOrderMixFromEdges") != std::string::npos)
        {
            const auto got = noteOrderMixFromEdges (fromBits (f[0]), fromBits (f[1]));
            expect (got.original, std::stoi (f[2]), at + " original");
            expect (got.cyclic, std::stoi (f[3]), at + " cyclic");
            expect (got.utterly, std::stoi (f[4]), at + " utterly");
        }
    });
}

} // namespace mclassic::conformance
