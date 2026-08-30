// The Random checker. Each module contributes one of these; `Runner.cpp` calls
// them all.

#include "Harness.h"

#include "../engine/Random.h"

namespace mclassic::conformance
{
namespace
{
/** Which block of `rng.txt` the parser is inside. */
enum class Section { none, rawDraws, intBelow, pickAvoiding, walk };

Section sectionFor (const std::string& header)
{
    if (header.find ("raw u32 draws") != std::string::npos)    return Section::rawDraws;
    if (header.find ("Rng.int") != std::string::npos)          return Section::intBelow;
    if (header.find ("pickIndexAvoiding") != std::string::npos) return Section::pickAvoiding;
    if (header.find ("BrownianWalk") != std::string::npos)     return Section::walk;
    return Section::none;
}

void checkRandom()
{
    const auto text = readFixture ("rng.txt");
    std::stringstream stream (text);
    std::string line;
    int lineNumber = 0;

    auto section = Section::none;
    mclassic::Random rng { 0 };
    mclassic::BrownianWalk walk { mclassic::Random { 0 } };

    while (std::getline (stream, line))
    {
        ++lineNumber;

        if (line.empty())
            continue;

        if (line[0] == '#')
        {
            section = sectionFor (line);
            continue;
        }

        const auto f = split (line, ',');
        const auto at = "rng.txt:" + std::to_string (lineNumber);

        if (section == Section::rawDraws)
        {
            const auto seed = static_cast<uint32_t> (std::stoul (f[0]));
            const auto index = std::stoi (f[1]);
            const auto want = static_cast<uint32_t> (std::stoul (f[2]));

            if (index == 0)
                rng = mclassic::Random { seed };

            expect (rng.nextU32(), want, at + " seed " + f[0] + " draw " + f[1]);
        }
        else if (section == Section::intBelow)
        {
            const auto seed = static_cast<uint32_t> (std::stoul (f[0]));
            const auto n = static_cast<uint32_t> (std::stoul (f[1]));
            const auto index = std::stoi (f[2]);
            const auto want = static_cast<uint32_t> (std::stoul (f[3]));

            if (index == 0)
                rng = mclassic::Random { seed };

            expect (rng.intBelow (n), want, at + " int(" + f[1] + ") #" + f[2]);
        }
        else if (section == Section::pickAvoiding)
        {
            const auto seed = static_cast<uint32_t> (std::stoul (f[0]));
            const auto n = static_cast<uint32_t> (std::stoul (f[1]));
            const auto avoid = static_cast<uint32_t> (std::stoul (f[2]));
            const auto index = std::stoi (f[3]);
            const auto want = static_cast<uint32_t> (std::stoul (f[4]));

            if (index == 0)
                rng = mclassic::Random { seed };

            expect (rng.pickIndexAvoiding (n, avoid),
                    want,
                    at + " pickIndexAvoiding(" + f[1] + ", " + f[2] + ") #" + f[3]);
        }
        else if (section == Section::walk)
        {
            const auto seed = static_cast<uint32_t> (std::stoul (f[0]));
            const auto index = std::stoi (f[1]);
            const auto want = fromBits (f[2]);

            if (index == 0)
                walk = mclassic::BrownianWalk { mclassic::Random { seed } };

            // Compared as bits: "close enough" is not the standard, because the
            // planner's arithmetic runs on top of these values.
            uint64_t gotBits = 0, wantBits = 0;
            const auto got = walk.next();
            std::memcpy (&gotBits, &got, sizeof (gotBits));
            std::memcpy (&wantBits, &want, sizeof (wantBits));

            expect (gotBits, wantBits, at + " walk #" + f[1]);
        }
        else
        {
            std::printf ("  %s: value outside any known section\n", at.c_str());
            ++totals().failures;
        }
    }
}

} // namespace

void checkRandom()
{
    auto section = Section::none;
    mclassic::Random rng { 0 };
    mclassic::BrownianWalk walk { mclassic::Random { 0 } };

    conformance::walk ("rng.txt", [&] (const std::string& header,
                                       const std::vector<std::string>& f,
                                       const std::string& at)
    {
        if (! header.empty())
            section = sectionFor (header);

        if (section == Section::rawDraws)
        {
            const auto index = std::stoi (f[1]);

            if (index == 0)
                rng = mclassic::Random { static_cast<uint32_t> (std::stoul (f[0])) };

            expect (rng.nextU32(), static_cast<uint32_t> (std::stoul (f[2])),
                    at + " seed " + f[0] + " draw " + f[1]);
        }
        else if (section == Section::intBelow)
        {
            if (std::stoi (f[2]) == 0)
                rng = mclassic::Random { static_cast<uint32_t> (std::stoul (f[0])) };

            expect (rng.intBelow (static_cast<uint32_t> (std::stoul (f[1]))),
                    static_cast<uint32_t> (std::stoul (f[3])),
                    at + " int(" + f[1] + ") #" + f[2]);
        }
        else if (section == Section::pickAvoiding)
        {
            if (std::stoi (f[3]) == 0)
                rng = mclassic::Random { static_cast<uint32_t> (std::stoul (f[0])) };

            expect (rng.pickIndexAvoiding (static_cast<uint32_t> (std::stoul (f[1])),
                                           static_cast<uint32_t> (std::stoul (f[2]))),
                    static_cast<uint32_t> (std::stoul (f[4])),
                    at + " pickIndexAvoiding(" + f[1] + ", " + f[2] + ") #" + f[3]);
        }
        else if (section == Section::walk)
        {
            if (std::stoi (f[1]) == 0)
                walk = mclassic::BrownianWalk { mclassic::Random { static_cast<uint32_t> (std::stoul (f[0])) } };

            expectBits (walk.next(), fromBits (f[2]), at + " walk #" + f[1]);
        }
    });
}

} // namespace mclassic::conformance
