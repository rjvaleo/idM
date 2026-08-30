// The cyclic variables, checked against the TypeScript engine.

#include "Harness.h"

#include "../engine/Cyclic.h"

namespace mclassic::conformance
{

void checkCyclic()
{
    Random rng { 0 };

    walk ("cyclic.txt", [&] (const std::string& header,
                             const std::vector<std::string>& f,
                             const std::string& at)
    {
        // The sequence header starts with the plain one, so it must be tested
        // first or its rows are misparsed as the simpler shape.
        if (header.find ("pickCyclicLevel(sequence)") != std::string::npos)
        {
            // One generator across the whole sequence: this is what fails if a
            // point range is allowed to spend a draw.
            if (std::stoi (f[1]) == 0)
                rng = Random { (uint32_t) std::stoul (f[0]) };

            expect (pickCyclicLevel (CyclicStep::range (std::stod (f[2]), std::stod (f[3])), rng),
                    std::stoi (f[4]),
                    at + " sequence step " + f[1] + " over [" + f[2] + ", " + f[3] + "]");
        }
        else if (header.find ("normalizeCyclicStep(number)") != std::string::npos)
        {
            const auto got = normalizeCyclicStep (CyclicStep::level (fromBits (f[0])));
            expect (got.min, std::stoi (f[1]), at + " min");
            expect (got.max, std::stoi (f[2]), at + " max");
        }
        else if (header.find ("normalizeCyclicStep(range)") != std::string::npos)
        {
            const auto got = normalizeCyclicStep (CyclicStep::range (fromBits (f[0]), fromBits (f[1])));
            expect (got.min, std::stoi (f[2]), at + " min");
            expect (got.max, std::stoi (f[3]), at + " max");
        }
        else if (header.find ("pickCyclicLevel") != std::string::npos)
        {
            if (std::stoi (f[3]) == 0)
                rng = Random { (uint32_t) std::stoul (f[0]) };

            expect (pickCyclicLevel (CyclicStep::range (std::stod (f[1]), std::stod (f[2])), rng),
                    std::stoi (f[4]),
                    at + " pick [" + f[1] + ", " + f[2] + "] #" + f[3]);
        }
        else if (header.find ("cyclicLengthFromStepIndex") != std::string::npos)
        {
            expect (cyclicLengthFromStepIndex (fromBits (f[0])), std::stoi (f[1]), at + " length");
        }
    });
}

} // namespace mclassic::conformance
